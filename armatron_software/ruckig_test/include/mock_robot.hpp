#pragma once

#include <array>
#include <vector>
#include <iostream>
#include <cmath>
#include <ruckig/ruckig.hpp>
#include "mock_motor.hpp"
#include "motor_defs.hpp"

// Differential motors state
struct DifferentialMotors {
    double left_motor_angle_rad = 0.0;
    double right_motor_angle_rad = 0.0;
    double roll_angle_rad = 0.0;
    double pitch_angle_rad = 0.0;
    double roll_angle_deg = 0.0;
    double pitch_angle_deg = 0.0;
};

// Robot state structure
struct RobotState {
    std::array<double, 7> joint_angles_deg;
    std::array<double, 7> joint_speeds_deg_s;
    std::array<double, 7> joint_accelerations_deg_s2;
    std::array<double, 7> joint_max_speeds_deg_s;
    std::array<double, 7> joint_max_accelerations_deg_s2;
    std::array<double, 7> joint_max_jerks_deg_s3;
    std::array<double, 7> target_joint_angles_deg;
    std::array<double, 7> target_joint_speeds_deg_s;
    std::array<double, 7> target_joint_accelerations_deg_s2;
    
    DifferentialMotors differential_motors;
    
    // Ruckig trajectory generation
    ruckig::InputParameter<7> ruckig_input;
    ruckig::OutputParameter<7> ruckig_output;
    ruckig::Ruckig<7> ruckig_otg{0.005}; // 200Hz control rate
    
    bool trajectory_active = false;
    double trajectory_progress = 0.0;
};

class MockRobotInterface {
private:
    std::vector<Motor> m_motors;
    RobotState m_state;
    float m_max_speed_modifier = 1.0;

    // Calculate differential motor positions for given roll and pitch targets
    std::pair<int32_t, int32_t> getDifferentialAngles(double target_roll_rad, double target_pitch_rad) {
        // Get current motor angles
        double current_left = m_motors[6].getState().multiTurnPosition;
        double current_right = m_motors[5].getState().multiTurnPosition;
        
        // Convert pitch limits from degrees to radians
        const double PITCH_LIMIT_LOW_RAD = DIFF_PITCH_ANGLE_LIMIT_LOW * M_PI / 180.0;
        const double PITCH_LIMIT_HIGH_RAD = DIFF_PITCH_ANGLE_LIMIT_HIGH * M_PI / 180.0;

        // Clamp target pitch within limits
        target_pitch_rad = std::clamp(target_pitch_rad, PITCH_LIMIT_LOW_RAD, PITCH_LIMIT_HIGH_RAD);
        
        // Wrap pitch to [-π, π] to find shortest path
        double wrapped_pitch = std::fmod(target_pitch_rad + M_PI, 2*M_PI);
        if (wrapped_pitch < 0) wrapped_pitch += 2*M_PI;
        wrapped_pitch -= M_PI;
        
        // Calculate target motor angles based on roll and pitch
        double target_left = -target_roll_rad + wrapped_pitch;
        double target_right = -target_roll_rad - wrapped_pitch;
        
        // Convert target angles to raw units (degrees * 10)
        target_left = target_left * (180.0 / M_PI) * 10.0;
        target_right = target_right * (180.0 / M_PI) * 10.0;
        
        // Calculate differences from current positions
        double left_diff = target_left - current_left;
        double right_diff = target_right - current_right;
        
        // Calculate final target positions
        int32_t final_left = static_cast<int32_t>(current_left + left_diff);
        int32_t final_right = static_cast<int32_t>(current_right + right_diff);
        
        std::cout << "[MockRobotInterface::getDifferentialAngles] Command Summary:" << std::endl
                  << "  Inputs:" << std::endl
                  << "    Target Roll: " << target_roll_rad << " rad (" << target_roll_rad * 180.0/M_PI << " deg)" << std::endl
                  << "    Target Pitch: " << target_pitch_rad << " rad (" << target_pitch_rad * 180.0/M_PI << " deg)" << std::endl
                  << "    Wrapped Pitch: " << wrapped_pitch << " rad (" 
                  << wrapped_pitch * 180.0/M_PI << " deg)" << std::endl
                  << "  Final Commands:" << std::endl
                  << "    Left Motor (ID 7): Position=" << final_left << " (0.1 deg)" << std::endl
                  << "    Right Motor (ID 6): Position=" << final_right << " (0.1 deg)" << std::endl;
        
        return {final_left, final_right};
    }

    void updateDifferentialMotors() {
        m_state.differential_motors.right_motor_angle_rad = (m_motors[5].getState().multiTurnPosition / 10) * (M_PI / 180);
        m_state.differential_motors.left_motor_angle_rad = (m_motors[6].getState().multiTurnPosition / 10) * (M_PI / 180);
        double left_angle = m_state.differential_motors.left_motor_angle_rad;
        double right_angle = m_state.differential_motors.right_motor_angle_rad;
        
        double roll = -0.5 * (left_angle + right_angle);
        double rawPitch = 0.5 * (left_angle - right_angle);
        
        // Wrap pitch to [-π, π]
        double pitch = std::fmod(rawPitch + M_PI, 2*M_PI);
        if (pitch < 0) pitch += 2*M_PI;
        pitch -= M_PI;
        
        m_state.differential_motors.roll_angle_rad = roll;
        m_state.differential_motors.pitch_angle_rad = pitch;
        m_state.differential_motors.roll_angle_deg = roll * (180 / M_PI);
        m_state.differential_motors.pitch_angle_deg = pitch * (180 / M_PI);
    }

public:
    MockRobotInterface() {
        // Initialize motors with IDs 1 through 7
        // Joint 1 - MG8015 - Base Shoulder
        m_motors.emplace_back(1, MG8015_REDUCTION_RATIO, MG8015_SINGLE_TURN_DEG_SCALE_MAX, 
                             JOINT_1_ANGLE_LIMIT_LOW, JOINT_1_ANGLE_LIMIT_HIGH, 
                             JOINT_1_MAX_SPEED, JOINT_1_MAX_ACCEL, JOINT_1_MAX_JERK, false);
        
        // Joint 2 - MG8015 - Mid Shoulder
        m_motors.emplace_back(2, MG8015_REDUCTION_RATIO, MG8015_SINGLE_TURN_DEG_SCALE_MAX, 
                             JOINT_2_ANGLE_LIMIT_LOW, JOINT_2_ANGLE_LIMIT_HIGH, 
                             JOINT_2_MAX_SPEED, JOINT_2_MAX_ACCEL, JOINT_2_MAX_JERK, false);
        
        // Joint 3 - MG8008 - Brachium Shoulder
        m_motors.emplace_back(3, MG8008_REDUCTION_RATIO, MG8008_SINGLE_TURN_DEG_SCALE_MAX, 
                             JOINT_3_ANGLE_LIMIT_LOW, JOINT_3_ANGLE_LIMIT_HIGH, 
                             JOINT_3_MAX_SPEED, JOINT_3_MAX_ACCEL, JOINT_3_MAX_JERK, false);
        
        // Joint 4 - MG8008 - Elbow Flexor
        m_motors.emplace_back(4, MG8008_REDUCTION_RATIO, MG8008_SINGLE_TURN_DEG_SCALE_MAX, 
                             JOINT_4_ANGLE_LIMIT_LOW, JOINT_4_ANGLE_LIMIT_HIGH, 
                             JOINT_4_MAX_SPEED, JOINT_4_MAX_ACCEL, JOINT_4_MAX_JERK, false);
        
        // Joint 5 - MG4010 - Forearm Rotator
        m_motors.emplace_back(5, MG4010_REDUCTION_RATIO, MG4010_SINGLE_TURN_DEG_SCALE_MAX, 
                             JOINT_5_ANGLE_LIMIT_LOW, JOINT_5_ANGLE_LIMIT_HIGH, 
                             JOINT_5_MAX_SPEED, JOINT_5_MAX_ACCEL, JOINT_5_MAX_JERK, false);
        
        // Joint 6 - MG4005 - Wrist Differential #1
        m_motors.emplace_back(6, MG4005_REDUCTION_RATIO, MG4005_SINGLE_TURN_DEG_SCALE_MAX, 
                             JOINT_6_ANGLE_LIMIT_LOW, JOINT_6_ANGLE_LIMIT_HIGH, 
                             DIFF_MAX_SPEED, DIFF_MAX_ACCEL, DIFF_MAX_JERK, true);
        
        // Joint 7 - MG4005 - Wrist Differential #2
        m_motors.emplace_back(7, MG4005_REDUCTION_RATIO, MG4005_SINGLE_TURN_DEG_SCALE_MAX, 
                             JOINT_7_ANGLE_LIMIT_LOW, JOINT_7_ANGLE_LIMIT_HIGH, 
                             DIFF_MAX_SPEED, DIFF_MAX_ACCEL, DIFF_MAX_JERK, true);

        // Initialize state arrays
        // Use default values at first
        for (int i = 0; i < 7; i++) {
            m_state.joint_angles_deg[i] = 0.0;
            m_state.joint_speeds_deg_s[i] = 0.0;
            m_state.joint_accelerations_deg_s2[i] = 0.0;
            m_state.joint_max_speeds_deg_s[i] = 0.0;
            m_state.joint_max_accelerations_deg_s2[i] = 0.0;
            m_state.joint_max_jerks_deg_s3[i] = 0.0;
            m_state.target_joint_angles_deg[i] = 0.0;
            m_state.target_joint_speeds_deg_s[i] = 0.0;
            m_state.target_joint_accelerations_deg_s2[i] = 0.0;
        }
    }

    // Get motor by ID (1-indexed)
    Motor& getMotor(int i) {
        if (i < 1 || i > static_cast<int>(m_motors.size())) {
            throw std::out_of_range("Motor index out of range");
        }
        return m_motors[i - 1];
    }

    // Get current robot state
    const RobotState& getState() const {
        return m_state;
    }

    // Update all motor states
    void updateAll() {
        for(int i = 0; i < 7; i++) {
            auto& m = m_motors[i];
            
            if (i == 5 || i == 6) {
                m_state.joint_angles_deg[i] = m.getState().multiTurnPosition; // Stay in raw units for differential motors
                m_state.joint_speeds_deg_s[i] = m.getState().speedDeg_s*10;
                m_state.joint_max_speeds_deg_s[i] = m.getMaxSpeed()*m.getMaxSpeedModifier()*10;
                m_state.joint_max_accelerations_deg_s2[i] = m.getMaxAcceleration()*m.getMaxSpeedModifier()*10;
                m_state.joint_max_jerks_deg_s3[i] = m.getMaxJerk()*m.getMaxSpeedModifier()*10;
            } else {
                m_state.joint_angles_deg[i] = m.getState().multiTurnDeg_Mapped;
                m_state.joint_speeds_deg_s[i] = m.getState().speedDeg_s;
                m_state.joint_max_speeds_deg_s[i] = m.getMaxSpeed()*m.getMaxSpeedModifier();
                m_state.joint_max_accelerations_deg_s2[i] = m.getMaxAcceleration()*m.getMaxSpeedModifier();
                m_state.joint_max_jerks_deg_s3[i] = m.getMaxJerk()*m.getMaxSpeedModifier();
            }
        }
        
        updateDifferentialMotors();
        updateJointTrajectories();
    }

    // Sets joint angles for motors 1 through n
    void setMultiJointAngles(const std::vector<float>& joint_angles, const std::vector<float>& joint_speeds) {
        for (int i = 1; i <= std::min(joint_angles.size(), joint_speeds.size()); i++) {
            float ang_target_deg = joint_angles.at(i-1);
            std::cout << "[MockRobotInterface] Multi Joint Command Received | Motor: " << i << " | Target (deg): " << ang_target_deg << "\n";
            
            // BUG: This check prevents processing of negative angles
            if (ang_target_deg >= 0.0) {
                auto &curr_mot = m_motors[i-1];
                float ang_target_raw = joint_angles.at(i-1);
                float maxSpeed = joint_speeds.at(i-1);
                m_motors[i-1].setMultiAngleWithSpeed(static_cast<int32_t>(ang_target_raw), static_cast<uint16_t>(maxSpeed));
                std::cout << "                 Target Angle Raw: " << ang_target_raw << "\n";
                std::cout << "                 MaxSpeed: " << maxSpeed << "\n";
            }
        }
    }

    // Move to joint position along ruckig generated joint trajectories
    void moveToJointPosition(const std::array<double, 7>& target_position) {
        std::cout << "[MockRobotInterface] moveToJointPosition: [";
        for (size_t i = 0; i < target_position.size(); ++i) {
            std::cout << target_position[i];
            if (i < target_position.size() - 1) std::cout << ", ";
        }
        std::cout << "] degrees" << std::endl;
        
        // Set target positions in state
        for (size_t i = 0; i < 5; ++i) {
            m_state.target_joint_angles_deg[i] = target_position[i];
            m_state.target_joint_speeds_deg_s[i] = 0.0; // Stop at target
            m_state.target_joint_accelerations_deg_s2[i] = 0.0;
        }
        
        // Calculate differential motor positions
        double target_roll_rad = target_position[5] * M_PI / 180.0;
        double target_pitch_rad = target_position[6] * M_PI / 180.0;

        auto [final_left, final_right] = getDifferentialAngles(target_roll_rad, target_pitch_rad);
        
        // BUG: The indices are swapped here - this would cause issues
        m_state.target_joint_angles_deg[6] = final_left;   // Left motor (ID 7) at index 6
        m_state.target_joint_angles_deg[5] = final_right;  // Right motor (ID 6) at index 5

        // Set up input parameters
        for (size_t i = 0; i < 7; ++i) {
            m_state.ruckig_input.current_position[i] = m_state.joint_angles_deg[i];
            m_state.ruckig_input.current_velocity[i] = m_state.joint_speeds_deg_s[i];
            m_state.ruckig_input.current_acceleration[i] = m_state.joint_accelerations_deg_s2[i];
            m_state.ruckig_input.target_position[i] = m_state.target_joint_angles_deg[i];
            m_state.ruckig_input.target_velocity[i] = m_state.target_joint_speeds_deg_s[i];
            m_state.ruckig_input.target_acceleration[i] = m_state.target_joint_accelerations_deg_s2[i];
            m_state.ruckig_input.max_velocity[i] = m_state.joint_max_speeds_deg_s[i];
            m_state.ruckig_input.max_acceleration[i] = m_state.joint_max_accelerations_deg_s2[i];
            m_state.ruckig_input.max_jerk[i] = m_state.joint_max_jerks_deg_s3[i];
        }

        // Activate trajectory
        m_state.trajectory_active = true;
        m_state.trajectory_progress = 0.0;
    }

    void updateJointTrajectories() {
        if (!m_state.trajectory_active) return;
        
        ruckig::Result r = m_state.ruckig_otg.update(m_state.ruckig_input, m_state.ruckig_output);
        if (r == ruckig::Result::Working) {
            // Convert arrays to vectors with proper type conversion
            std::vector<float> positions(7);
            std::vector<float> velocities(7);
            
            for (size_t i = 0; i < 7; ++i) {
                positions[i] = static_cast<float>(m_state.ruckig_output.new_position[i]);
                velocities[i] = static_cast<float>(m_state.ruckig_output.new_velocity[i]);
                
                // Debug output
                std::cout << "Joint " << i+1 << ": pos=" << positions[i] 
                          << " vel=" << velocities[i] << std::endl;
            }
            
            // Adjust speeds for differential motors
            velocities[5] /= 10.0f;
            velocities[6] /= 10.0f;
            
            setMultiJointAngles(positions, velocities);
            m_state.ruckig_output.pass_to_input(m_state.ruckig_input);  // prepare for the next cycle
        } else if (r == ruckig::Result::Finished) {
            m_state.trajectory_active = false;
            std::cout << "Trajectory duration: " << m_state.ruckig_output.trajectory.get_duration() << " [s]." << std::endl;
        } else {
            // Handle errors by stopping trajectory
            m_state.trajectory_active = false;
            std::cout << "Trajectory error: " << static_cast<int>(r) << std::endl;
        }
    }

    void setHoldPosition() { 
        m_state.trajectory_active = false; 
        // Reset Ruckig state
        for (size_t i = 0; i < 7; ++i) {
            m_state.ruckig_input.current_position[i] = m_state.joint_angles_deg[i];
            m_state.ruckig_input.current_velocity[i] = 0.0;
            m_state.ruckig_input.current_acceleration[i] = 0.0;
            m_state.ruckig_input.target_position[i] = m_state.joint_angles_deg[i];
            m_state.ruckig_input.target_velocity[i] = 0.0;
            m_state.ruckig_input.target_acceleration[i] = 0.0;
        }
        for (auto &m : m_motors) { 
            m.setSpeed(0); 
        } 
    }

    void setMaxSpeedModifier(double modifier) {
        m_max_speed_modifier = modifier;
        for (auto &m : m_motors) {
            m.setMaxSpeedModifier(modifier);
        }
    }
}; 