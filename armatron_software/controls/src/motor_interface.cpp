#include "motor_interface.hpp"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cmath>
#include <chrono>
#include <algorithm>

namespace
{
    // Helper to pack a 16-bit into two bytes
    inline void pack16(std::vector<uint8_t>& data, size_t idx, int16_t val)
    {
        data[idx]   = static_cast<uint8_t>( val       & 0xFF );
        data[idx+1] = static_cast<uint8_t>((val >> 8) & 0xFF );
    }
    // Helper to pack a 32-bit into four bytes
    inline void pack32(std::vector<uint8_t>& data, size_t idx, int32_t val)
    {
        data[idx]   = static_cast<uint8_t>( val        & 0xFF );
        data[idx+1] = static_cast<uint8_t>((val >> 8)  & 0xFF );
        data[idx+2] = static_cast<uint8_t>((val >> 16) & 0xFF );
        data[idx+3] = static_cast<uint8_t>((val >> 24) & 0xFF );
    }

    inline int16_t unpack16(const struct can_frame& f, int loIdx)
    {
        return static_cast<int16_t>(
            (static_cast<int16_t>(f.data[loIdx+1]) << 8) |
            f.data[loIdx]
        );
    }

    inline int32_t unpack32(const struct can_frame& f, int loIdx)
    {
        return static_cast<int32_t>(
            (static_cast<int32_t>(f.data[loIdx+3]) << 24) |
            (static_cast<int32_t>(f.data[loIdx+2]) << 16) |
            (static_cast<int32_t>(f.data[loIdx+1]) <<  8) |
            f.data[loIdx]
        );
    }
    inline int64_t unpack64(const struct can_frame& f, int loIdx)
    {
        return static_cast<int64_t>(
            (static_cast<int64_t>(f.data[loIdx + 7]) << 56) |
            (static_cast<int64_t>(f.data[loIdx + 6]) << 48) |
            (static_cast<int64_t>(f.data[loIdx + 5]) << 40) |
            (static_cast<int64_t>(f.data[loIdx + 4]) << 32) |
            (static_cast<int64_t>(f.data[loIdx + 3]) << 24) |
            (static_cast<int64_t>(f.data[loIdx + 2]) << 16) |
            (static_cast<int64_t>(f.data[loIdx + 1]) <<  8) |
                static_cast<int64_t>(f.data[loIdx])
        );
    }


    // Define a constant for 2*pi
    const double TWO_PI = 2.0 * M_PI;

    // Helper conversion functions
    inline double degreesToRadians(double degrees) {
        return degrees * M_PI / 180.0;
    }

    inline double radiansToDegrees(double radians) {
        return radians * 180.0 / M_PI;
    }

    inline bool checkAngleSync(double single_angle, double multi_angle) {
        return std::abs(single_angle - multi_angle) < 1;
    }
    inline double wrapAngle(double angle) {
        return fmod(angle, 360.0);
    }
} // end anon

Motor::Motor(uint8_t motorId, CANHandler& canRef, float reduction_ratio, float single_loop_max_ang_raw, float Nm_to_iq_m, float Nm_to_iq_b, float single_loop_ang_limit_low, float single_loop_ang_limit_high, float max_speed, float max_accel, float max_jerk, bool is_differential)
    : m_motorId(motorId), m_can(canRef), m_reduction_ratio(reduction_ratio), m_single_loop_max_ang_raw(single_loop_max_ang_raw), 
    m_Nm_to_iq_m(Nm_to_iq_m), m_Nm_to_iq_b(Nm_to_iq_b), 
    m_single_loop_ang_limit_low(single_loop_ang_limit_low), m_single_loop_ang_limit_high(single_loop_ang_limit_high), 
    m_max_speed(max_speed), m_max_accel(max_accel), m_max_jerk(max_jerk), m_is_differential(is_differential)
{
    // Clear state
    std::memset(&m_state, 0, sizeof(m_state));
}

//-------------------------------------------
//  Public Methods
//-------------------------------------------
const MotorState& Motor::getState() const
{
    return m_state;
}

void Motor::motorOff()
{
    sendCmd(0x80);
    readFrameForCommand(0x80);
}

void Motor::motorOn()
{
    sendCmd(0x88);
    readFrameForCommand(0x88);
}

void Motor::motorStop()
{
    sendCmd(0x81);
    readFrameForCommand(0x81);
}

void Motor::openLoopControl(int16_t powerControl)
{
    // Command 0xA0
    // data => [0x00,0x00,0x00,0x00, powerLo, powerHi, 0x00,0x00]
    std::vector<uint8_t> data(8, 0);
    pack16(data, 4, powerControl);

    sendCmd(0xA0, data);
    readFrameForCommand(0xA0);
}

void Motor::setTorque(int16_t iqControl)
{
    // 0xA1 => torque
    // data => [0x00,0x00,0x00, iqLo, iqHi, 0x00, 0x00, 0x00]
    std::vector<uint8_t> d(8, 0);
    pack16(d, 3, iqControl);

    sendCmd(0xA1, d);
    readFrameForCommand(0xA1);
}

void Motor::setSpeed(float speedControl)
{
    // 0xA2 => speed 
    // data => [0x00,0x00,0x00, speed0, speed1, speed2, speed3, 0x00]
    // convert speed to motors expected units (0.01 deg/s per LSB)
    speedControl = speedControl * 100;
    if (m_motorId == 6 || m_motorId == 7) {
        speedControl = speedControl * 10;
    } else {
        speedControl = speedControl * m_reduction_ratio;
    }
    int32_t speed_command = static_cast<int32_t>(speedControl);
    std::vector<uint8_t> d(8, 0);
    pack32(d, 3, speed_command);

    // std::cerr << "[Motor::setSpeed] speedControl: " << speedControl << std::endl;
    // std::cerr << "[Motor::setSpeed] speed_command: " << speed_command << std::endl;

    sendCmd(0xA2, d);
    readFrameForCommand(0xA2);
}

void Motor::setMultiAngle(int32_t angleControl)
{
    // 0xA3 => multi angle (1 frame)
    // data => [0x00,0x00,0x00, angle0, angle1, angle2, angle3, 0x00]
    int32_t clamped_angle = std::clamp(angleControl, static_cast<int32_t>(m_single_loop_ang_limit_low), static_cast<int32_t>(m_single_loop_ang_limit_high));
    int32_t scaled_angle = clamped_angle * 100 * m_reduction_ratio; // Scale to motor expectation
    std::vector<uint8_t> d(8, 0);
    pack32(d, 3, scaled_angle);

    sendCmd(0xA3, d);
    readFrameForCommand(0xA3);
}

void Motor::setMultiAngleWithSpeed(int32_t angle, uint16_t maxSpeed)
{
    // 0xA4 => angle + speed
    // data => [0x00, spdLo, spdHi, angle0, angle1, angle2, angle3]
    int32_t clamped_angle = std::clamp(angle, static_cast<int32_t>(m_single_loop_ang_limit_low), static_cast<int32_t>(m_single_loop_ang_limit_high));
    int32_t scaled_angle = clamped_angle * 100 * m_reduction_ratio; // Scale to motor expectation
    uint16_t scaled_speed = std::clamp(maxSpeed, static_cast<uint16_t>(0), static_cast<uint16_t>(m_max_speed*m_max_speed_modifier)) * ((m_motorId == 6 || m_motorId == 7) ? 10 : m_reduction_ratio);
    if (scaled_speed == 0) { scaled_speed = 1;}
    std::vector<uint8_t> d(7, 0);
    d[1] = static_cast<uint8_t>( scaled_speed & 0xFF );
    d[2] = static_cast<uint8_t>((scaled_speed >> 8) & 0xFF );
    pack32(d, 3, scaled_angle);
    //std::cerr << "[Motor::setMultiAngleWithSpeed] Input speed: " << maxSpeed << " | Scaled speed: " << scaled_speed << std::endl;
    //std::cerr << "[Motor::setMultiAngleWithSpeed] Input angle: " << angle << " | Scaled angle: " << scaled_angle << std::endl;

    sendCmd(0xA4, d);
    readFrameForCommand(0xA4);
}

void Motor::setSingleAngle(uint8_t spinDirection, int32_t angle)
{
    // 0xA5 => single angle, data => [dir,0,0, angLo,angHi,angHi2,angHi3]
    int32_t clamped_angle = std::clamp(angle, static_cast<int32_t>(m_single_loop_ang_limit_low), static_cast<int32_t>(m_single_loop_ang_limit_high));
    int32_t scaled_angle = clamped_angle * 100 * m_reduction_ratio; // Scale to motor expectation
    std::vector<uint8_t> d(7, 0);
    d[0] = spinDirection;
    pack32(d, 3, scaled_angle);
    sendCmd(0xA5, d);
    readFrameForCommand(0xA5);
}
 
void Motor::setSingleAngleWithSpeed(uint8_t spinDirection, int32_t angle, uint16_t maxSpeed)
{
    // 0xA6 => single angle 2
    // data => [dir, spdLo,spdHi, angLo, angHi, angHi2, angHi3]
    int32_t clamped_angle = std::clamp(angle, static_cast<int32_t>(m_single_loop_ang_limit_low), static_cast<int32_t>(m_single_loop_ang_limit_high));
    int32_t scaled_angle = clamped_angle * 100 * m_reduction_ratio; // Scale to motor expectation
    std::vector<uint8_t> d(7, 0);
    d[0] = spinDirection;
    d[1] = static_cast<uint8_t>( maxSpeed & 0xFF );
    d[2] = static_cast<uint8_t>((maxSpeed >> 8) & 0xFF );
    pack32(d, 3, scaled_angle);

    sendCmd(0xA6, d);
    readFrameForCommand(0xA6);
}

void Motor::setIncrementAngle(int32_t incAngle)
{
    // 0xA7 => inc angle
    // data => [0x00,0x00,0x00, inc0,inc1,inc2,inc3]
    std::vector<uint8_t> d(7,0);
    pack32(d, 3, incAngle);

    sendCmd(0xA7, d);
    readFrameForCommand(0xA7);
}

void Motor::setIncrementAngleWithSpeed(int32_t incAngle, uint16_t maxSpeed)
{
    // 0xA8 => inc angle + speed
    // data => [0x00, spdLo, spdHi, inc0, inc1, inc2, inc3]
    std::vector<uint8_t> d(7,0);
    d[1] = static_cast<uint8_t>(maxSpeed & 0xFF);
    d[2] = static_cast<uint8_t>((maxSpeed >> 8) & 0xFF);
    pack32(d, 3, incAngle);

    sendCmd(0xA8, d);
    readFrameForCommand(0xA8);
}

void Motor::clearMultiLoopAngle(){
    sendCmd(0x93);
}

std::vector<uint8_t> Motor::readPID()
{
    // 0x30 => read PID param
    // data => [0x30, 0,0,0,0,0,0,0] 
    std::cout << "[Motor::readPID] Sending read PID command for motor " << static_cast<int>(m_motorId) << std::endl;
    sendCmd(0x30);
    readFrameForCommand(0x30);
    // Return a dummy vector or implement as needed.
    return std::vector<uint8_t>();
}

void Motor::writePID_RAM(uint8_t angKp, uint8_t angKi, uint8_t spdKp, uint8_t spdKi, uint8_t iqKp, uint8_t iqKi)
{
    // 0x31 => write PID to RAM
    // data => [0x31, 0, angKp, angKi, spdKp, spdKi, iqKp, iqKi]
    std::vector<uint8_t> d(8);
    d[0] = 0x00;
    d[1] = angKp;
    d[2] = angKi;
    d[3] = spdKp;
    d[4] = spdKi;
    d[5] = iqKp;
    d[6] = iqKi;

    sendCmd(0x31, d);
    readFrameForCommand(0x31);
}

void Motor::writePID_ROM(uint8_t angKp, uint8_t angKi, uint8_t spdKp, uint8_t spdKi, uint8_t iqKp, uint8_t iqKi)
{
    // 0x32 => write PID to ROM
    std::vector<uint8_t> d(8);
    d[0] = 0x00;
    d[1] = angKp;
    d[2] = angKi;
    d[3] = spdKp;
    d[4] = spdKi;
    d[5] = iqKp;
    d[6] = iqKi;

    sendCmd(0x32, d);
    readFrameForCommand(0x32);
}

int32_t Motor::readAcceleration()
{
    // 0x33 => read accel
    sendCmd(0x33);
    readFrameForCommand(0x33);
    // The parse sets an internal variable or you can store in m_state if you want
    return 0; 
}

void Motor::writeAcceleration(int32_t accel)
{
    // 0x34 => write accel
    // data => [0x34,0,0,0,acc0,acc1,acc2,acc3] 
    // We'll skip the second approach, we only have 8 data total, the first byte is command if we were doing raw, but we do do a separate param
    std::vector<uint8_t> d(8,0);
    pack32(d,3,accel);

    sendCmd(0x34, d);
    readFrameForCommand(0x34);
}

void Motor::readEncoder()
{
    // 0x90 => read encoder
    sendCmd(0x90);
    readFrameForCommand(0x90);
}

void Motor::writeEncoderOffset(uint16_t offset)
{
    // 0x91 => write offset to ROM
    // data => [0x91,0,0,0,0,0, offLo, offHi]
    std::vector<uint8_t> d(7,0);
    d[5] = static_cast<uint8_t>( offset & 0xFF );
    d[6] = static_cast<uint8_t>((offset >> 8) & 0xFF);

    sendCmd(0x91, d);
    readFrameForCommand(0x91);
}

void Motor::writeCurrentPosAsZero()
{
    // 0x19 => write current pos to zero
    std::vector<uint8_t> d(7,0);
    sendCmd(0x19, d);
    readFrameForCommand(0x19);
}

void Motor::readMultiAngle()
{
    // 0x92 => read multi angle
    sendCmd(0x92);
    readFrameForCommand(0x92);
}

void Motor::readSingleAngle()
{
    // 0x94 => read single angle
    sendCmd(0x94);
    readFrameForCommand(0x94);
}

void Motor::clearAngle()
{
    // 0x95 => clear motor angle
    sendCmd(0x95);
    readFrameForCommand(0x95);
}

void Motor::readState1_Error()
{
    // 0x9A => read temp, voltage, error
    sendCmd(0x9A);
    readFrameForCommand(0x9A);
}

void Motor::clearError()
{
    // 0x9B => clear error
    sendCmd(0x9B);
    readFrameForCommand(0x9B);
}

void Motor::readState2()
{
    // 0x9C => read temp, torque current, speed, encoder
    sendCmd(0x9C);
    readFrameForCommand(0x9C);
}

void Motor::readState3()
{
    // 0x9D => read temp, IA, IB, IC 
    sendCmd(0x9D);
    readFrameForCommand(0x9D);
}

// MOTOR RANGE MAPPING FUNCTIONS
// Forward function:
// Maps raw motor units (0 to maxUnits) to radians [0, 2*pi).
// No clamping is done because we want the exact value reported from the motor.
float Motor::motorRawToRadians(float rawValue) {
    return (static_cast<float>(rawValue) / m_single_loop_max_ang_raw) * TWO_PI;
}

// Reverse function:
// Converts a radian value back to raw motor units.
// The computed raw value is then clamped to the active range [limitLow, limitHigh].
float Motor::motorRadiansToRaw(float radians) {
    // Compute the raw value (rounding to the nearest integer)
    float rawValue = static_cast<float>((radians / TWO_PI) * m_single_loop_max_ang_raw);
    std::cout << "[MotorInterface] called motorRadiansToRaw() w/ output: " << std::clamp(rawValue, m_single_loop_ang_limit_low, m_single_loop_ang_limit_high) << "\n";
    std::cout << "[MotorInterface] called motorRadiansToRaw() w/ output: " << std::clamp(rawValue, m_single_loop_ang_limit_low, m_single_loop_ang_limit_high) << "\n";
    // Clamp the raw value to the allowed active range   
    return std::clamp(rawValue, m_single_loop_ang_limit_low, m_single_loop_ang_limit_high);
}

//-------------------------------------------
//  Private Helpers
//-------------------------------------------
bool Motor::sendCmd(uint8_t command, const std::vector<uint8_t>& data)
{
    return m_can.sendMessage(canID(), command, data);
}

/**
 * @brief readFrameForCommand: Attempt one read of a frame. In real code, 
 *        you'd possibly do a loop or a timeout-based approach. We parse
 *        the doc-specified response and store in m_state.
 */
void Motor::readFrameForCommand(uint8_t expectedCmd)
{
    struct can_frame frame;
    auto start = std::chrono::steady_clock::now();

    if (!(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(10))) {
        std::cout << "[Motor Interface] CAN Loop Overrun (10ms - motor_interface.cpp::readFrameForCommand)\n";
    }
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(10)) {
        if (m_can.receiveMessage(frame)) {
            // Verify frame is from the correct motor.
            if ((frame.can_id & 0x7FF) != canID()) {
                return;
            }
            // Check command byte.
            if (frame.data[0] != expectedCmd && !(expectedCmd == 0x19 && frame.data[0] == 0x19)) {
                return;
            }
            // Parse response based on the command.
            switch (frame.data[0]) {
            case 0xA0:
            case 0xA1:
            case 0xA2:
            case 0xA3:
            case 0xA4:
            case 0xA5:
            case 0xA6:
            case 0xA7:
            case 0xA8:
            {
                // Motion control responses: [cmd, temp, torqueLo, torqueHi, speedLo, speedHi, encLo, encHi]
                if (frame.can_dlc >= 8) {
                    int8_t t   = static_cast<int8_t>(frame.data[1]);
                    int16_t iq = unpack16(frame, 2);
                    int16_t spd = unpack16(frame, 4);
                    uint16_t enc = static_cast<uint16_t>((static_cast<uint16_t>(frame.data[7]) << 8) | frame.data[6]);
                    m_state.temperatureC = t;
                    m_state.torqueCurrentA = iq; 
                    m_state.speedDeg_s = spd / ((m_motorId == 6 || m_motorId == 7) ? 10 : m_reduction_ratio);
                    m_state.encoderVal = enc;
                    m_state.errorPresent = false;
                    m_state.errorCode = 0;
                }
                break;
            }
            case 0x30:
            {
                // Read PID parameters response: [0x30, 0, angleKp, angleKi, speedKp, speedKi, torqueKp, torqueKi]
                std::cout << "[Motor::readFrameForCommand] Received PID response for motor " << static_cast<int>(m_motorId) << std::endl;
                std::cout << "[Motor::readFrameForCommand] Raw data: ";
                for (int i = 0; i < frame.can_dlc; i++) {
                    std::cout << static_cast<int>(frame.data[i]) << " ";
                }
                std::cout << std::endl;
                
                m_state.m_gains.angKp = frame.data[2];
                m_state.m_gains.angKi = frame.data[3];
                m_state.m_gains.spdKp = frame.data[4];
                m_state.m_gains.spdKi = frame.data[5];
                m_state.m_gains.iqKp = frame.data[6];
                m_state.m_gains.iqKi = frame.data[7];
                
                std::cout << "[Motor::readFrameForCommand] Parsed gains: "
                          << "angKp=" << static_cast<int>(m_state.m_gains.angKp) << " "
                          << "angKi=" << static_cast<int>(m_state.m_gains.angKi) << " "
                          << "spdKp=" << static_cast<int>(m_state.m_gains.spdKp) << " "
                          << "spdKi=" << static_cast<int>(m_state.m_gains.spdKi) << " "
                          << "iqKp=" << static_cast<int>(m_state.m_gains.iqKp) << " "
                          << "iqKi=" << static_cast<int>(m_state.m_gains.iqKi) << std::endl;
                break;
            }
            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
            {
                // PID and acceleration commands: echo the sent command and parameters.
                break;
            }
            case 0x90:
            {
                // Read encoder response: [0x90, 0, encLo, encHi, rawLo, rawHi, offLo, offHi]
                if (frame.can_dlc >= 8) {
                    int16_t enc    = unpack16(frame, 2);
                    int16_t encRaw = unpack16(frame, 4);
                    int16_t off    = unpack16(frame, 6);
                    m_state.encoderVal = enc; 
                }
                break;
            }
            case 0x9A:
            {
                // Read Motor State1 response: [0x9A, temp, 0, voltLo, voltHi, 0, 0, errByte]
                if (frame.can_dlc >= 8) {
                    int8_t tmpC = static_cast<int8_t>(frame.data[1]);
                    uint16_t volt = static_cast<uint16_t>((frame.data[4] << 8) | frame.data[3]);
                    uint8_t err = frame.data[7];
                    m_state.temperatureC = tmpC;
                    m_state.busVoltage = volt * 0.1;
                    m_state.errorPresent = (err != 0);
                    m_state.errorCode = err;
                }
                break;
            }
            case 0x9B:
            {
                // Clear error response: same format as 0x9A.
                if (frame.can_dlc >= 8) {
                    int8_t tmpC = static_cast<int8_t>(frame.data[1]);
                    uint16_t volt = static_cast<uint16_t>((frame.data[4] << 8) | frame.data[3]);
                    uint8_t err = frame.data[7];
                    m_state.temperatureC = tmpC;
                    m_state.busVoltage = volt * 0.1;
                    m_state.errorPresent = (err != 0);
                    m_state.errorCode = err;
                }
                break;
            }
            case 0x9C:
            {
                // Read Motor State2 response: [0x9C, temp, torqueLo, torqueHi, speedLo, speedHi, encLo, encHi]
                if (frame.can_dlc >= 8) {
                    int8_t t = static_cast<int8_t>(frame.data[1]);
                    int16_t iq = unpack16(frame, 2);
                    int16_t spd = unpack16(frame, 4);
                    uint16_t e = static_cast<uint16_t>((frame.data[7] << 8) | frame.data[6]);
                    m_state.temperatureC = t;
                    m_state.torqueCurrentA = iq;
                    m_state.speedDeg_s = spd / ((m_motorId == 6 || m_motorId == 7) ? 10 : m_reduction_ratio);
                    m_state.encoderVal = e;
                }
                break;
            }
            case 0x9D:
            {
                // Read Motor State3 response: [0x9D, temp, iA_L, iA_H, iB_L, iB_H, iC_L, iC_H]
                if (frame.can_dlc >= 8) {
                    int8_t t = static_cast<int8_t>(frame.data[1]);
                    m_state.temperatureC = t;
                }
                break;
            }
            case 0x92:
            {
                // Read multi-turn angle response.
                // Assume response: [0x92, angLo 1, ang2, ang3, ang4, ang5, ang6, angHi7]
                if (frame.can_dlc >= 8) {
                    int32_t angle = unpack64(frame, 1);
                    // Assume angle is in 0.01 degrees per LSB.
                    m_state.multiTurnPosition = angle * 0.01;
                    m_state.multiTurnDeg_Mapped = wrapAngle(m_state.multiTurnPosition / m_reduction_ratio);
                    m_state.multiTurnRad_Mapped = degreesToRadians(m_state.multiTurnDeg_Mapped);
                    m_is_synced = checkAngleSync(m_state.positionDeg_Mapped, m_state.multiTurnDeg_Mapped);
                    if (!m_is_synced && !(m_motorId == 6 || m_motorId == 7)) {
                        if (m_state.positionDeg_Mapped > 0 && m_state.positionDeg_Mapped < 180) {
                            clearMultiLoopAngle();
                        }
                    }
                }
                break;
            }
            case 0x94:
            {
                // Read single-turn angle response.
                // Assume response: [0x94, 0, 0, ang0, ang1, ang2, ang3, 0]
                if (frame.can_dlc >= 8) {
                    int32_t angle = unpack32(frame, 4);
                    m_state.positionDeg = angle * 0.01;
                    m_state.positionRad_Mapped = degreesToRadians(m_state.positionDeg / m_reduction_ratio) ;
                    m_state.positionDeg_Mapped = m_state.positionDeg / m_reduction_ratio;
                }
                break;
            }
            case 0x95:
            {
                // Clear angle loop response. We assume an acknowledgment.
                // No additional data parsing is needed.
                break;
            }
            case 0x19:
            {
                // Write current position as zero response.
                // Assume response includes an offset in the last two bytes.
                if (frame.can_dlc >= 8) {
                    int16_t offset = unpack16(frame, 6);
                    // For example, store the offset or print it.
                    std::cout << "[Motor Interface] Current position zero offset: " << offset << "\n";
                }
                break;
            }
            case 0x91:
            {
                // Write encoder offset response. Echo confirmation.
                if (frame.can_dlc >= 8) {
                    int16_t offset = unpack16(frame, 6);
                    std::cout << "[Motor Interface] Encoder offset set to: " << offset << "\n";
                }
                break;
            }
            default:
                // Unknown command; do nothing.
                break;
            } // end switch

            return; // Successfully parsed matching frame.
        } // if receiveMessage
    } // while loop
    std::cout << "[Motor Interface] CAN Loop Overrun (10ms - motor_interface.cpp::readFrameForCommand)\n";
}
