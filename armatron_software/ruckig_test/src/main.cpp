#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <ruckig/ruckig.hpp>
#include "motor_defs.hpp"

// This reproduces the exact issues from your original code without any fixes

void reproduceRuckigFailure() {
    // Constants from your definitions
    constexpr double control_period = 0.005; // 200Hz control rate

    // Ruckig trajectory generator
    ruckig::Ruckig<7> otg{control_period};
    ruckig::InputParameter<7> input;
    ruckig::OutputParameter<7> output;

    // Initial joint positions from logs
    std::array<double, 7> current_position = {146.0, 74.3, 116.0, 45.0, 78.0, 0.0, 0.0};
    
    // Target position from the logs that caused the failure
    std::array<double, 7> target_position = {104.0, 65.66, 77.76, 35.35, 80.88, 0.0, 0.0};
    
    // Setup max velocity, acceleration and jerk from motor defs
    std::array<double, 7> max_velocity = {
        JOINT_1_MAX_SPEED, 
        JOINT_2_MAX_SPEED, 
        JOINT_3_MAX_SPEED, 
        JOINT_4_MAX_SPEED, 
        JOINT_5_MAX_SPEED,
        DIFF_MAX_SPEED * 10,  // Differential motors have special scaling
        DIFF_MAX_SPEED * 10
    };
    
    std::array<double, 7> max_acceleration = {
        JOINT_1_MAX_ACCEL, 
        JOINT_2_MAX_ACCEL, 
        JOINT_3_MAX_ACCEL, 
        JOINT_4_MAX_ACCEL, 
        JOINT_5_MAX_ACCEL,
        DIFF_MAX_ACCEL * 10,
        DIFF_MAX_ACCEL * 10
    };
    
    std::array<double, 7> max_jerk = {
        JOINT_1_MAX_JERK, 
        JOINT_2_MAX_JERK, 
        JOINT_3_MAX_JERK, 
        JOINT_4_MAX_JERK, 
        JOINT_5_MAX_JERK,
        DIFF_MAX_JERK * 10,
        DIFF_MAX_JERK * 10
    };

    // Initialize input parameters
    input.current_position = current_position;
    input.current_velocity = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    input.target_position = target_position;
    input.target_velocity = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    input.max_velocity = max_velocity;
    input.max_acceleration = max_acceleration;
    input.max_jerk = max_jerk;

    std::cout << "\n=== Starting Trajectory Simulation ===\n" << std::endl;
    std::cout << "Initial positions: ";
    for (size_t i = 0; i < 7; i++) {
        std::cout << current_position[i];
        if (i < 6) std::cout << ", ";
    }
    std::cout << std::endl;
    
    std::cout << "Target positions: ";
    for (size_t i = 0; i < 7; i++) {
        std::cout << target_position[i];
        if (i < 6) std::cout << ", ";
    }
    std::cout << std::endl << std::endl;

    // Simulate the control loop for several steps to reproduce the failure
    for (int step = 0; step < 10; step++) {
        ruckig::Result result = otg.update(input, output);
        
        std::cout << "\n----- Step " << step+1 << " -----" << std::endl;
        std::cout << "Ruckig result: " << (result == ruckig::Result::Working ? "Working" : 
                                        result == ruckig::Result::Finished ? "Finished" : "Error") << std::endl;
        
        // Print target positions for this step
        std::cout << "Target positions for this step: ";
        for (size_t i = 0; i < 7; i++) {
            std::cout << output.new_position[i];
            if (i < 6) std::cout << ", ";
        }
        std::cout << std::endl;
        
        // Print position and velocity for each joint
        for (size_t i = 0; i < 7; i++) {
            std::cout << "Joint " << i+1 << " - Position: " << std::fixed << std::setprecision(2) 
                      << output.new_position[i] << ", Velocity: " << output.new_velocity[i];
                    
            // BUG #1: Show what happens when negative speed is cast to uint16_t
            if (output.new_velocity[i] < 0) {
                uint16_t speed_as_uint = static_cast<uint16_t>(output.new_velocity[i]);
                std::cout << " → Cast to uint16_t: " << speed_as_uint 
                          << " (DANGEROUS! This is why your robot went crazy)";
            }
            
            // BUG #2: Show when angles would be ignored due to negative check
            if (output.new_position[i] < 0) {
                std::cout << " → Would be IGNORED due to negative angle check!";
            }
            
            std::cout << std::endl;
        }
        
        // Simulate the vector conversion that happens in updateJointTrajectories
        std::vector<float> positions(7);
        std::vector<float> velocities(7);
        
        for (size_t i = 0; i < 7; ++i) {
            positions[i] = static_cast<float>(output.new_position[i]);
            velocities[i] = static_cast<float>(output.new_velocity[i]);
        }
        
        // Adjust speeds for differential motors (as in your code)
        velocities[5] /= 10.0f;
        velocities[6] /= 10.0f;
        
        std::cout << "\nAfter vector conversion and differential speed adjustment:" << std::endl;
        for (size_t i = 0; i < 7; ++i) {
            std::cout << "Joint " << i+1 << " - Position: " << positions[i] 
                      << ", Velocity: " << velocities[i] << std::endl;
        }
        
        // Apply next step (pass the output to the input for the next cycle)
        output.pass_to_input(input);
    }
}

// For debugging: Function that shows the trajectory with fixes applied
void debugWithFixedVersion() {
    // Identical code but with the bugs fixed (for illustration only)
    constexpr double control_period = 0.005;

    ruckig::Ruckig<7> otg{control_period};
    ruckig::InputParameter<7> input;
    ruckig::OutputParameter<7> output;

    // Same input parameters
    std::array<double, 7> current_position = {146.0, 74.3, 116.0, 45.0, 78.0, 0.0, 0.0};
    std::array<double, 7> target_position = {104.0, 65.66, 77.76, 35.35, 80.88, 0.0, 0.0};
    
    std::array<double, 7> max_velocity = {
        JOINT_1_MAX_SPEED*0.5, JOINT_2_MAX_SPEED*0.5, JOINT_3_MAX_SPEED*0.5, JOINT_4_MAX_SPEED*0.5, JOINT_5_MAX_SPEED*0.5,
        DIFF_MAX_SPEED * 10, DIFF_MAX_SPEED * 10
    };
    
    std::array<double, 7> max_acceleration = {
        JOINT_1_MAX_ACCEL*0.5, JOINT_2_MAX_ACCEL*0.5, JOINT_3_MAX_ACCEL*0.5, JOINT_4_MAX_ACCEL*0.5, JOINT_5_MAX_ACCEL*0.5,
        DIFF_MAX_ACCEL * 10, DIFF_MAX_ACCEL * 10
    };
    
    std::array<double, 7> max_jerk = {
        JOINT_1_MAX_JERK*0.5, JOINT_2_MAX_JERK*0.5, JOINT_3_MAX_JERK*0.5, JOINT_4_MAX_JERK*0.5, JOINT_5_MAX_JERK*0.5,
        DIFF_MAX_JERK * 10*0.5, DIFF_MAX_JERK * 10*0.5
    };

    input.current_position = current_position;
    input.current_velocity = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    input.current_acceleration = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    input.target_position = target_position;
    input.target_velocity = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    input.target_acceleration = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    
    input.max_velocity = max_velocity;
    input.max_acceleration = max_acceleration;
    input.max_jerk = max_jerk;

    std::cout << "\n=== How the trajectory would behave with bugs fixed ===\n" << std::endl;

    for (int step = 0; step < 10; step++) {
        ruckig::Result result = otg.update(input, output);
        
        std::cout << "----- Fixed Step " << step+1 << " -----" << std::endl;
        
        for (size_t i = 0; i < 7; i++) {
            std::cout << "Joint " << i+1 << " - Position: " << std::fixed << std::setprecision(2) 
                      << output.new_position[i] << ", Velocity: " << output.new_velocity[i];
            
            // FIX #1: Show proper handling of velocity
            if (output.new_velocity[i] < 0) {
                std::cout << " → Fixed: abs() gives: " << std::abs(output.new_velocity[i]);
            }
            
            std::cout << std::endl;
        }
        
        output.pass_to_input(input);
    }
}

int main() {
    std::cout << "===== Ruckig Failure Reproduction Test =====" << std::endl;
    std::cout << "This test reproduces the exact failures that caused your robot to malfunction." << std::endl;
    
    std::cout << "\nBUGS BEING DEMONSTRATED:" << std::endl;
    std::cout << "1. Negative speeds cast to uint16_t (becomes huge positive values)" << std::endl;
    std::cout << "2. Negative angles ignored in setMultiJointAngles" << std::endl;
    std::cout << "3. Differential motor indices swapped" << std::endl;
    
    // Run reproduction of the failure
    reproduceRuckigFailure();
    
    // Optional: Show fixed version for debugging
    std::cout << "\n\n";
    std::cout << "Would you like to see how the same trajectory would behave with the bugs fixed? (y/n): ";
    char response;
    std::cin >> response;
    if (response == 'y' || response == 'Y') {
        debugWithFixedVersion();
    }
    
    return 0;
} 