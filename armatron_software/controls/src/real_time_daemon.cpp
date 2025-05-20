#include "real_time_daemon.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <jsoncpp/json/json.h>
#include <algorithm>
#include "utils.hpp"


namespace
{
const char* DEFAULT_SOCKET_PATH = "/home/debian/.armatron/robot_socket";
}

/**********************************************************/
/* Constructor / Destructor                               */
/**********************************************************/
RealTimeDaemon::RealTimeDaemon(RobotInterface& robot)
    : m_robot(robot)
    , m_sockfd(-1)
    , m_socketPath(DEFAULT_SOCKET_PATH)
{
    // We might remove any stale socket file
    ::unlink(m_socketPath.c_str());
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Constructor: Removed stale socket file if exists.\n");
}

RealTimeDaemon::~RealTimeDaemon()
{
    stop();
}

/**********************************************************/
/* Start / Stop                                           */
/**********************************************************/
void RealTimeDaemon::start()
{
    m_running = true;
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Starting daemon.\n");

    // 1) Create the socket
    m_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Socket created: " << m_sockfd << "\n");
    if (m_sockfd < 0) {
        throw std::runtime_error("Failed to create Unix domain socket");
    }

    // 2) Bind the socket
    sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socketPath.c_str(), sizeof(addr.sun_path)-1);

    if (bind(m_sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(m_sockfd);
        throw std::runtime_error("Failed to bind to " + m_socketPath);
    }
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Socket bound to " << m_socketPath << "\n");

    // 3) Listen
    if (listen(m_sockfd, 5) < 0) {
        close(m_sockfd);
        throw std::runtime_error("listen() failed");
    }
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Listening on socket.\n");

    // 4) Start the socket thread
    m_socketThread = std::thread(&RealTimeDaemon::socketThreadFunc, this);
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Socket thread started.\n");

    // 5) Start the real-time control thread
    m_controlThread = std::thread(&RealTimeDaemon::controlThreadFunc, this);
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Control thread started.\n");

    std::cout << "[RealTimeDaemon] Started, socket at " << m_socketPath << "\n";
    std::cout << "[RealTimeDaemon] Control thread running at " << CONTROL_RATE_HZ << " Hz\n";
}

void RealTimeDaemon::stop()
{
    if (!m_running) return;

    m_running = false;
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Stopping daemon.\n");

    // close socket
    if (m_sockfd >= 0) {
        close(m_sockfd);
        m_sockfd = -1;
        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Socket closed.\n");
    }

    if (m_socketThread.joinable()) {
        m_socketThread.join();
        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Socket thread joined.\n");
    }
    if (m_controlThread.joinable()) {
        m_controlThread.join();
        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Control thread joined.\n");
    }

    // Close all connected client sockets.
    {
        std::lock_guard<std::mutex> lk(m_clientFdsMutex);
        for (auto fd : m_clientFds) {
            close(fd);
        }
        m_clientFds.clear();
    }
    ::unlink(m_socketPath.c_str());
    std::cout << "[RealTimeDaemon] Stopped.\n";
}

/**********************************************************/
/* Socket Thread                                          */
/**********************************************************/
void RealTimeDaemon::socketThreadFunc()
{
    while (m_running) {
        int clientFd = accept(m_sockfd, nullptr, nullptr);
        if (clientFd < 0) {
            if (!m_running) break;
            std::cerr << "[RealTimeDaemon] Accept error.\n";
            continue;
        }
        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Accepted client FD=" << clientFd << "\n");

        {
            // Add the client FD to our list.
            std::lock_guard<std::mutex> lk(m_clientFdsMutex);
            m_clientFds.push_back(clientFd);
        }
        // Spawn a new thread to handle incoming messages from this client.
        std::thread(&RealTimeDaemon::clientHandler, this, clientFd).detach();
    }
}

// This method handles reading from a single client connection.
void RealTimeDaemon::clientHandler(int clientFd)
{
    char buf[1024];
    std::stringstream partial;
    while (m_running) {
        ssize_t r = recv(clientFd, buf, sizeof(buf), 0);
        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Received " << r << " bytes from client FD=" << clientFd << "\n");
        if (r <= 0) {
            if (!partial.str().empty()) {
                std::string line = partial.str();
                std::lock_guard<std::mutex> lk(m_inboundMutex);
                m_inboundQueue.push( IPCMessage{ line } );
                IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Flushed partial JSON: " << line << "\n");
            }
            break;
        }
        for (int i = 0; i < r; i++) {
            if (buf[i] == '\n') {
                std::string line = partial.str();
                partial.str(std::string());
                partial.clear();
                IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Received JSON line: " << line << "\n");
                if (!line.empty()) {
                    // Check if this is a high-priority command
                    bool is_high_priority = false;
                    if (line.find("\"cmd\":\"setESTOP\"") != std::string::npos || 
                        line.find("\"cmd\": \"setESTOP\"") != std::string::npos ||
                        line.find("\"cmd\":\"setHoldPosition\"") != std::string::npos || 
                        line.find("\"cmd\": \"setHoldPosition\"") != std::string::npos) {
                        is_high_priority = true;
                        std::cout << "[RealTimeDaemon] Received HIGH PRIORITY command: " 
                                  << (line.find("setESTOP") != std::string::npos ? "ESTOP" : "HOLD POSITION") 
                                  << " - clearing command queue!" << std::endl;
                    }
                    
                    if (is_high_priority) {
                        std::lock_guard<std::mutex> lk(m_inboundMutex);
                        IPCMessage message{line};
                        
                        // Clear the queue of any pending commands
                        std::queue<IPCMessage> empty;
                        std::swap(m_inboundQueue, empty);
                        
                        // Execute the high-priority command immediately
                        if (line.find("setESTOP") != std::string::npos) {
                            handleEmergencyStop();
                        } else if (line.find("setHoldPosition") != std::string::npos) {
                            handleHoldPosition();
                        }
                    } else {
                        // Normal commands go through the queue
                        std::lock_guard<std::mutex> lk(m_inboundMutex);
                        IPCMessage message{line}; // Non-high-priority message
                        m_inboundQueue.push(message);
                        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Pushed JSON line into queue: " << line << "\n");
                    }
                }
            } else {
                partial << buf[i];
            }
        }
    }
    close(clientFd);
    {
        std::lock_guard<std::mutex> lk(m_clientFdsMutex);
        auto it = std::find(m_clientFds.begin(), m_clientFds.end(), clientFd);
        if (it != m_clientFds.end()) {
            m_clientFds.erase(it);
        }
    }
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Closed client FD=" << clientFd << "\n");
}


/**********************************************************/
/* Control Thread                                         */
/**********************************************************/
bool RealTimeDaemon::configureRealTimeThread()
{
    // First try to get real-time scheduling
    struct sched_param param;
    param.sched_priority = RT_THREAD_PRIORITY;
    
    int result = sched_setscheduler(0, RT_THREAD_POLICY, &param);
    if (result != 0) {
        if (errno == EPERM) {
            std::cerr << "[RealTimeDaemon] Warning: No permission for real-time scheduling.\n"
                      << "Please ensure the systemd service has proper real-time settings:\n"
                      << "1. Check that the service file is installed in /etc/systemd/system/\n"
                      << "2. Run: sudo systemctl daemon-reload\n"
                      << "3. Run: sudo systemctl restart armatron-control.service\n";
        } else {
            std::cerr << "[RealTimeDaemon] Warning: Failed to set real-time scheduling policy: " 
                      << strerror(errno) << std::endl;
        }
        
        // Try to get maximum normal priority as fallback
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &param) != 0) {
            std::cerr << "[RealTimeDaemon] Warning: Failed to set normal scheduling priority: "
                      << strerror(errno) << std::endl;
        }
    }

    // Try to lock memory (this might also fail without proper permissions)
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        if (errno == ENOMEM) {
            std::cerr << "[RealTimeDaemon] Warning: Failed to lock memory - insufficient memory limits.\n"
                      << "Please ensure the systemd service has LimitMEMLOCK=infinity set.\n";
        } else {
            std::cerr << "[RealTimeDaemon] Warning: Failed to lock memory: " 
                      << strerror(errno) << std::endl;
        }
    }

    // Try to set CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);  // Pin to CPU 0

    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[RealTimeDaemon] Warning: Failed to set CPU affinity: " 
                  << strerror(errno) << std::endl;
    }

    // Return true if we got real-time scheduling, false otherwise
    return (result == 0);
}

void RealTimeDaemon::lockMemory()
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::cerr << "[RealTimeDaemon] Failed to lock memory: " 
                  << strerror(errno) << std::endl;
    }
}

void RealTimeDaemon::setThreadAffinity()
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);  // Pin to CPU 0

    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[RealTimeDaemon] Failed to set CPU affinity: " 
                  << strerror(errno) << std::endl;
    }
}

void RealTimeDaemon::controlThreadFunc()
{
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Control thread running.\n");
    
    // Configure real-time settings
    if (!configureRealTimeThread()) {
        std::cerr << "[RealTimeDaemon] Failed to configure real-time thread. Continuing with soft real-time.\n";
    }
    
    // Control loop configuration
    constexpr std::chrono::nanoseconds CONTROL_PERIOD{1000000000 / CONTROL_RATE_HZ};
    
    auto nextTime = std::chrono::steady_clock::now();
    unsigned int cycleCount = 0;

    while (m_running) {
        // 1) Process inbound commands first
        {
            // Lock both mutexes to prevent conflicts with emergency commands
            std::lock_guard<std::mutex> lk(m_inboundMutex);
            std::lock_guard<std::mutex> em_lk(m_emergency_mutex);
            
            while (!m_inboundQueue.empty()) {
                auto msg = m_inboundQueue.front();
                m_inboundQueue.pop();
                IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Processing command: " << msg.json << "\n");
                handleCommand(msg.json);
            }
        }

        // 2) Do real-time update for all motors
        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Updating all motors.\n");
        m_robot.updateAll(); // This does CAN read/writes and state management
        IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Updated all motors.\n");

        // 3) Broadcast states at configured rate
        if (++cycleCount % BROADCAST_DIVIDER == 0) {
            Json::Value jroot;
            jroot["type"] = "motorStates";

            // gather each motor's state
            for (int i = 1; i < 8; i++) {
                auto &mot = m_robot.getMotor(i);
                auto st   = mot.getState();
                Json::Value mjs;
                mjs["temp"]       = st.temperatureC;
                mjs["torqueA"]    = st.torqueCurrentA;
                mjs["speedDeg_s"] = st.speedDeg_s;
                mjs["posDeg"]     = st.positionDeg;
                mjs["multiTurnRaw"] = st.multiTurnPosition;
                mjs["multiTurnRad_Mapped"] = st.multiTurnRad_Mapped;
                mjs["multiTurnDeg_Mapped"] = st.multiTurnDeg_Mapped;
                mjs["error"]      = (st.errorPresent ? 1 : 0);
                mjs["encoder_val"] = st.encoderVal;
                mjs["positionRad_Mapped"] = st.positionRad_Mapped;
                mjs["positionDeg_Mapped"] = st.positionDeg_Mapped;
                
                // Add motor gains
                Json::Value gains;
                gains["angKp"] = static_cast<int>(st.m_gains.angKp);
                gains["angKi"] = static_cast<int>(st.m_gains.angKi);
                gains["spdKp"] = static_cast<int>(st.m_gains.spdKp);
                gains["spdKi"] = static_cast<int>(st.m_gains.spdKi);
                gains["iqKp"]  = static_cast<int>(st.m_gains.iqKp);
                gains["iqKi"]  = static_cast<int>(st.m_gains.iqKi);
                mjs["gains"] = gains;
                
                jroot["motors"][std::to_string(i)] = mjs;
            }

            // Add diff crossbar and tool data
            jroot["diff_roll_rad"] = m_robot.getState().differential_motors.roll_angle_rad;
            jroot["diff_pitch_rad"] = m_robot.getState().differential_motors.pitch_angle_rad;
            jroot["diff_roll_deg"] = m_robot.getState().differential_motors.roll_angle_deg;
            jroot["diff_pitch_deg"] = m_robot.getState().differential_motors.pitch_angle_deg;

            // Add digital twin data
            const auto& state = m_robot.getState();
            if (state.twin_active) {
                Json::Value twinData;
                twinData["active"] = true;
                Json::Value twinAngles;
                
                // Add first 5 joints from the array
                for (int i = 0; i < 5; ++i) {
                    twinAngles.append(state.twin_joint_angles_deg[i]);
                }
                
                // Add differential angles for joints 6 and 7
                twinAngles.append(state.twin_diff_pitch_rad);
                twinAngles.append(state.twin_diff_roll_rad);
                
                twinData["joint_angles_deg"] = twinAngles;
                jroot["twin"] = twinData;
            } else {
                jroot["twin"]["active"] = false;
            }

            Json::StreamWriterBuilder builder;
            builder["indentation"] = ""; // Force compact, single-line output.
            std::string outStr = Json::writeString(builder, jroot);
            IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Broadcasting state: " << outStr << "\n");
            sendJson(outStr); 
        }

        // Busy wait until next tick for hard real-time
        nextTime += CONTROL_PERIOD;
        while (std::chrono::steady_clock::now() < nextTime) {
            // Busy wait - this is more deterministic than sleep
        }
    }
}

/**********************************************************/
/* handleCommand                                          */
/**********************************************************/
void RealTimeDaemon::handleCommand(const std::string& jsonStr)
{
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Handling command: " << jsonStr << "\n");
    // Parse JSON using JsonCPP
    Json::CharReaderBuilder rb;
    Json::Value root;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(rb.newCharReader());
    bool ok = reader->parse(jsonStr.data(), jsonStr.data() + jsonStr.size(), &root, &errs);
    if (!ok) {
        std::cerr << "[RealTimeDaemon] Invalid JSON: " << errs << "\n";
        return;
    }
    if (!root.isObject()) return;
    
    std::string cmd = root["cmd"].asString();
    int motorID = root.get("motorID", 1).asInt();
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] Command parsed: " << cmd << " for motorID " << motorID << "\n");

    try {
        auto &mot = m_robot.getMotor(motorID);

        if (cmd == "motorOn") {
            mot.motorOn();
        } else if (cmd == "motorOff") {
            mot.motorOff();
        } else if (cmd == "motorStop") {
            mot.motorStop();
        } else if (cmd == "setHoldPosition") {
            m_robot.setHoldPosition();
        } else if (cmd == "setESTOP") {
            m_robot.setESTOP();
        } else if (cmd == "openLoopControl") {
            int power = root.get("powerControl", 0).asInt();
            mot.openLoopControl(static_cast<int16_t>(power));
        } else if (cmd == "setTorque") {
            int value = root.get("value", 0).asInt();
            mot.setTorque(static_cast<int16_t>(value));
        } else if (cmd == "setSpeed") {
            int value = root.get("value", 0).asInt();
            mot.setSpeed(static_cast<int32_t>(value));
        } else if (cmd == "setMultiAngle") {
            int value = root.get("value", 0).asInt();
            mot.setMultiAngle(static_cast<int32_t>(value));
        } else if (cmd == "setMultiAngleWithSpeed") {
            int angle = root.get("angle", 0).asInt();
            int maxSpeed = root.get("maxSpeed", 0).asInt();
            mot.setMultiAngleWithSpeed(static_cast<int32_t>(angle), static_cast<uint16_t>(maxSpeed));
        } else if (cmd == "setSingleAngle") {
            int spin = root.get("spinDirection", 0).asInt();
            int angle = root.get("angle", 0).asInt();
            mot.setSingleAngle(static_cast<uint8_t>(spin), static_cast<int32_t>(angle));
        } else if (cmd == "setSingleAngleWithSpeed") {
            int spin = root.get("spinDirection", 0).asInt();
            int angle = root.get("angle", 0).asInt();
            int maxSpeed = root.get("maxSpeed", 0).asInt();
            std::cout << "[RealTimeDaemon] Received setSingleAngleWithSpeed | Spin: " << spin << " | Angle: " << angle << "\n";
            mot.setSingleAngleWithSpeed(static_cast<uint8_t>(spin), static_cast<int32_t>(angle), static_cast<uint16_t>(maxSpeed));
        } else if (cmd == "setMultiJointAngles") {
            std::vector<float> angles;
            std::vector<float> speeds;
            if (root.isMember("angles") && root["angles"].isArray() &&
                root.isMember("speeds") && root["speeds"].isArray()) {
                const Json::Value anglesJson = root["angles"];
                const Json::Value speedsJson = root["speeds"];
                for (Json::ArrayIndex i = 0; i < anglesJson.size(); i++) {
                    angles.push_back(anglesJson[i].asFloat());
                }
                for (Json::ArrayIndex i = 0; i < speedsJson.size(); i++) {
                    speeds.push_back(speedsJson[i].asFloat());
                }
                m_robot.setMultiJointAngles(angles, speeds);
            } else {
                std::cerr << "[RealTimeDaemon] setMultiJointAngles: Invalid or missing angles/speeds arrays." << std::endl;
            }
        } else if (cmd == "setDifferentialAngles") {
            if (root.isMember("roll") && root.isMember("pitch") && 
                root.isMember("maxSpeed")) {
                double roll = root["roll"].asDouble();
                double pitch = root["pitch"].asDouble();
                double max_motor_speed = root["maxSpeed"].asDouble();
                std::cerr << "[RealTimeDaemon] setDifferentialAngles: "
                         << "roll=" << roll << " rad, "
                         << "pitch=" << pitch << " rad, "
                         << "maxSpeed=" << max_motor_speed << " deg/s, " << std::endl;
                m_robot.setDifferentialAngles(roll, pitch, max_motor_speed);
            } else {
                std::cerr << "[RealTimeDaemon] setDifferentialAngles: Invalid or missing parameters." << std::endl;
            }
        } else if (cmd == "moveToJointPositionRuckig") {
            if (root.isMember("angles") && root["angles"].isArray() && root["angles"].size() == 7) {
                std::array<double, 7> target_positions;
                const Json::Value anglesJson = root["angles"];
                
                // Convert JSON angles to array
                for (Json::ArrayIndex i = 0; i < anglesJson.size(); i++) {
                    target_positions[i] = anglesJson[i].asDouble();
                }
                
                // Log the command for debugging
                std::cerr << "[RealTimeDaemon] moveToJointPositionRuckig: [";
                for (size_t i = 0; i < target_positions.size(); ++i) {
                    std::cerr << target_positions[i];
                    if (i < target_positions.size() - 1) std::cerr << ", ";
                }
                std::cerr << "] degrees" << std::endl;
                
                // Execute the trajectory
                m_robot.moveToJointPosition(target_positions);
            } else {
                std::cerr << "[RealTimeDaemon] moveToJointPositionRuckig: Invalid or missing angles array. Expected 7 angles." << std::endl;
            }
        } else if (cmd == "setMaxSpeedModifier") {
            double modifier = root["modifier"].asDouble();
            std::cerr << "[RealTimeDaemon] setMaxSpeedModifier: " << modifier << std::endl;
            m_robot.setMaxSpeedModifier(modifier);
        } else if (cmd == "setIncrementAngle") {
            int value = root.get("incAngle", 0).asInt();
            mot.setIncrementAngle(static_cast<int32_t>(value));
        } else if (cmd == "setIncrementAngleWithSpeed") {
            int incAngle = root.get("incAngle", 0).asInt();
            int maxSpeed = root.get("maxSpeed", 0).asInt();
            mot.setIncrementAngleWithSpeed(static_cast<int32_t>(incAngle), static_cast<uint16_t>(maxSpeed));
        } else if (cmd == "syncSingleAndMulti"){
            mot.clearMultiLoopAngle();
        } else if (cmd == "readPID") {
            auto pid = mot.readPID();
            // Optionally, you can send the PID values back to the client.
        } else if (cmd == "writePID_RAM") {
            uint8_t angKp = root.get("angKp", 100).asUInt();
            uint8_t angKi = root.get("angKi", 50).asUInt();
            uint8_t spdKp = root.get("spdKp", 50).asUInt();
            uint8_t spdKi = root.get("spdKi", 20).asUInt();
            uint8_t iqKp  = root.get("iqKp", 50).asUInt();
            uint8_t iqKi  = root.get("iqKi", 50).asUInt();
            mot.writePID_RAM(angKp, angKi, spdKp, spdKi, iqKp, iqKi);
        } else if (cmd == "writePID_ROM") {
            uint8_t angKp = root.get("angKp", 100).asUInt();
            uint8_t angKi = root.get("angKi", 50).asUInt();
            uint8_t spdKp = root.get("spdKp", 50).asUInt();
            uint8_t spdKi = root.get("spdKi", 20).asUInt();
            uint8_t iqKp  = root.get("iqKp", 50).asUInt();
            uint8_t iqKi  = root.get("iqKi", 50).asUInt();
            mot.writePID_ROM(angKp, angKi, spdKp, spdKi, iqKp, iqKi);
        } else if (cmd == "readAcceleration") {
            int32_t accel = mot.readAcceleration();
            // Optionally send accel back
        } else if (cmd == "writeAcceleration") {
            int accel = root.get("accel", 0).asInt();
            mot.writeAcceleration(static_cast<int32_t>(accel));
        } else if (cmd == "readEncoder") {
            mot.readEncoder();
        } else if (cmd == "writeEncoderOffset") {
            int offset = root.get("offset", 0).asInt();
            mot.writeEncoderOffset(static_cast<uint16_t>(offset));
        } else if (cmd == "writeCurrentPosAsZero") {
            mot.writeCurrentPosAsZero();
        } else if (cmd == "readMultiAngle") {
            mot.readMultiAngle();
        } else if (cmd == "readSingleAngle") {
            mot.readSingleAngle();
        } else if (cmd == "clearAngle") {
            mot.clearAngle();
        } else if (cmd == "readState1_Error") {
            mot.readState1_Error();
        } else if (cmd == "clearError") {
            mot.clearError();
        } else if (cmd == "readState2") {
            mot.readState2();
        } else if (cmd == "readState3") {
            mot.readState3();
        } else {
            std::cerr << "[RealTimeDaemon] Unknown command: " << cmd << "\n";
        }
    } catch (std::exception &ex) {
        std::cerr << "[RealTimeDaemon] handleCommand exception: " << ex.what() << "\n";
    }
}


/**********************************************************/
/* sendJson                                               */
/**********************************************************/
void RealTimeDaemon::sendJson(const std::string& jsonStr)
{
    // Append a newline to ensure the Node side can correctly detect message boundaries.
    std::string jsonWithNewline = jsonStr + "\n";
    IFRTDEBUG(std::cout << "[RealTimeDaemon][DEBUG] sendJson called with: " << jsonWithNewline << "\n");
    
    // Send the JSON string to all connected clients.
    std::lock_guard<std::mutex> lk(m_clientFdsMutex);
    for (auto it = m_clientFds.begin(); it != m_clientFds.end(); ) {
        ssize_t written = write(*it, jsonWithNewline.c_str(), jsonWithNewline.size());
        if (written < 0) {
            std::cerr << "[RealTimeDaemon] Error writing to client FD=" << *it << ". Closing connection.\n";
            close(*it);
            it = m_clientFds.erase(it);
        } else {
            ++it;
        }
    }
}

// Direct emergency commands - can be called from any thread
void RealTimeDaemon::handleEmergencyStop()
{
    std::cout << "[RealTimeDaemon] EMERGENCY STOP TRIGGERED - DIRECT EXECUTION" << std::endl;
    
    // Lock to ensure we don't have conflicts with the control thread
    std::lock_guard<std::mutex> lock(m_emergency_mutex);
    
    // Directly call the robot ESTOP method - bypassing the command queue
    m_robot.setESTOP();
}

void RealTimeDaemon::handleHoldPosition()
{
    std::cout << "[RealTimeDaemon] HOLD POSITION TRIGGERED - DIRECT EXECUTION" << std::endl;
    
    // Lock to ensure we don't have conflicts with the control thread
    std::lock_guard<std::mutex> lock(m_emergency_mutex);
    
    // Directly call the robot hold position method - bypassing the command queue
    m_robot.setHoldPosition();
}
