#ifndef REAL_TIME_DAEMON_HPP
#define REAL_TIME_DAEMON_HPP

#include "robot_interface.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <vector>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>

/**
 * @brief A minimal message structure to pass between Node and the real-time code
 */
struct IPCMessage {
    std::string json; 
};

/**
 * @brief RealTimeDaemon sets up:
 *  1) A high-frequency control loop for the motors
 *  2) A Unix domain socket server to receive commands from Node
 *  3) A mechanism to send motor states to Node
 * 
 * The control loop runs at 200Hz with hard real-time scheduling when available.
 * If real-time scheduling is not available, it falls back to maximum normal priority.
 */
class RealTimeDaemon
{
public:
    RealTimeDaemon(RobotInterface& robot);
    ~RealTimeDaemon();

    /**
     * @brief Start the daemon:
     *  1) Binds /home/debian/.armatron/robot_socket
     *  2) Spawns a real-time loop thread at high freq
     *  3) Spawns a socket listener thread
     */
    void start();

    /**
     * @brief Stop everything, join threads
     */
    void stop();
    
    /**
     * @brief Emergency stop handler - bypasses command queue for immediate execution
     * Can be safely called from any thread (socket, web, etc)
     */
    void handleEmergencyStop();
    
    /**
     * @brief Hold position handler - bypasses command queue for immediate execution
     * Can be safely called from any thread (socket, web, etc)
     */
    void handleHoldPosition();

private:
    RobotInterface& m_robot;
    std::atomic<bool> m_running { false };

    // Socket stuff
    int m_sockfd;
    std::thread m_socketThread;
    std::string m_socketPath;

    // Real-time loop
    std::thread m_controlThread;

    // Thread-safe queue for inbound commands
    std::mutex m_inboundMutex;
    std::queue<IPCMessage> m_inboundQueue;

    // Outbound queuing 
    std::mutex m_outboundMutex;
    std::queue<IPCMessage> m_outboundQueue;

    // Client Handling
    std::vector<int> m_clientFds;
    std::mutex m_clientFdsMutex;
    
    // Emergency handling mutex
    std::mutex m_emergency_mutex;

    // Real-time thread configuration
    static constexpr int RT_THREAD_PRIORITY = 99;  // Maximum real-time priority
    static constexpr int RT_THREAD_POLICY = SCHED_FIFO;  // First-in-first-out scheduling
    static constexpr size_t RT_STACK_SIZE = 1024 * 1024;  // 1MB stack for real-time thread

    // Control loop configuration
    static constexpr unsigned int CONTROL_RATE_HZ = 200;  // 200Hz control loop
    static constexpr unsigned int STATE_BROADCAST_RATE_HZ = 60;  // 60Hz state broadcast
    static constexpr unsigned int BROADCAST_DIVIDER = CONTROL_RATE_HZ / STATE_BROADCAST_RATE_HZ;

    /**
     * @brief Configure the real-time thread with proper scheduling and memory locking
     * @return true if real-time scheduling was successfully configured, false otherwise
     */
    bool configureRealTimeThread();

    /**
     * @brief Lock all current and future memory pages to prevent paging
     * This helps ensure deterministic real-time behavior
     */
    void lockMemory();

    /**
     * @brief Set CPU affinity to pin the thread to a specific CPU core
     * This helps reduce scheduling jitter
     */
    void setThreadAffinity();

    /**
     * @brief Configure the socket server and start accepting connections
     */
    void socketThreadFunc();

    /**
     * @brief Handle incoming messages from a specific client
     * @param clientFd The file descriptor of the client connection
     */
    void clientHandler(int clientFd);

    /**
     * @brief Main control loop running at CONTROL_RATE_HZ
     * This thread is configured for real-time scheduling when available
     */
    void controlThreadFunc();

    /**
     * @brief Parse and execute a command received from the web interface
     * @param jsonStr The JSON string containing the command
     */
    void handleCommand(const std::string& jsonStr);

    /**
     * @brief Send a JSON message to all connected clients
     * @param jsonStr The JSON string to send
     */
    void sendJson(const std::string& jsonStr);
};

#endif
