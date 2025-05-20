#pragma once

#include <iostream>
#include <cmath>
#include "motor_defs.hpp"

// Simplified motor state structure
struct MotorState {
    double multiTurnPosition = 0.0;      // Current multi-turn position
    double multiTurnDeg_Mapped = 0.0;    // Mapped to degrees
    double speedDeg_s = 0.0;             // Current speed in deg/s
    bool isDifferential = false;         // Whether this is a differential motor
};

// Mock Motor class that simulates the real Motor
class Motor {
private:
    uint8_t m_motorId;
    float m_reduction_ratio;
    float m_single_loop_max_ang_raw;
    float m_single_loop_ang_limit_low;
    float m_single_loop_ang_limit_high;
    float m_max_speed;
    float m_max_accel;
    float m_max_jerk;
    float m_max_speed_modifier = 1.0;
    bool m_is_differential;
    MotorState m_state;

public:
    Motor(uint8_t motorId, float reduction_ratio, float single_loop_max_ang_raw, 
          float single_loop_ang_limit_low, float single_loop_ang_limit_high,
          float max_speed, float max_accel, float max_jerk, bool is_differential)
        : m_motorId(motorId)
        , m_reduction_ratio(reduction_ratio)
        , m_single_loop_max_ang_raw(single_loop_max_ang_raw)
        , m_single_loop_ang_limit_low(single_loop_ang_limit_low)
        , m_single_loop_ang_limit_high(single_loop_ang_limit_high)
        , m_max_speed(max_speed)
        , m_max_accel(max_accel)
        , m_max_jerk(max_jerk)
        , m_is_differential(is_differential)
    {
        m_state.isDifferential = is_differential;
    }

    // Simulated method to set angle with speed
    void setMultiAngleWithSpeed(int32_t angle, uint16_t maxSpeed) {
        float raw_angle = static_cast<float>(angle);
        
        std::cout << "[Motor::setMultiAngleWithSpeed] Scaled speed: " << maxSpeed << std::endl;
        std::cout << "                 Target Angle Raw: " << raw_angle << std::endl;
        std::cout << "                 MaxSpeed: " << static_cast<int16_t>(maxSpeed) << std::endl;
        
        // In a real system, this would command the motor to move
        // Here we just update the state directly for simulation
        m_state.multiTurnPosition = raw_angle;
        
        // Update mapped degrees based on reduction ratio
        if (m_is_differential) {
            m_state.multiTurnDeg_Mapped = raw_angle;
        } else {
            m_state.multiTurnDeg_Mapped = raw_angle / m_reduction_ratio;
        }
    }

    // Set speed
    void setSpeed(int32_t speed) {
        m_state.speedDeg_s = static_cast<float>(speed);
    }

    // Get current state
    const MotorState& getState() const {
        return m_state;
    }

    // Get max speed
    float getMaxSpeed() const {
        return m_max_speed;
    }

    // Get max acceleration
    float getMaxAcceleration() const {
        return m_max_accel;
    }

    // Get max jerk
    float getMaxJerk() const {
        return m_max_jerk;
    }

    // Get max speed modifier
    float getMaxSpeedModifier() const {
        return m_max_speed_modifier;
    }

    // Set max speed modifier
    void setMaxSpeedModifier(float modifier) {
        m_max_speed_modifier = modifier;
    }
}; 