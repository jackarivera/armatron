#include "robot_interface.hpp"
#include "motor_defs.hpp"
#include <stdexcept>
#include <iostream>

RobotInterface::RobotInterface(CANHandler& canRef, const std::string& urdf_path)
{
    // Create 7 motors with IDs 0 through 6 - NOTE THE NEGATIVE 1's NEED TO BE CHANGED TO TORQUE CONSTANTS
    // Joint 1 - MG8015 - Base Shoulder
    m_motors.emplace_back(static_cast<uint8_t>(1), canRef, MG8015_REDUCTION_RATIO, MG8015_SINGLE_TURN_DEG_SCALE_MAX, JOINT_1_NM_TO_IQ_M, JOINT_1_NM_TO_IQ_B, JOINT_1_ANGLE_LIMIT_LOW, JOINT_1_ANGLE_LIMIT_HIGH, JOINT_1_MAX_SPEED, JOINT_1_MAX_ACCEL, JOINT_1_MAX_JERK, false);
    // Joint 2 - MG8015 - Mid Shoulder 
    m_motors.emplace_back(static_cast<uint8_t>(2), canRef, MG8015_REDUCTION_RATIO, MG8015_SINGLE_TURN_DEG_SCALE_MAX, JOINT_2_NM_TO_IQ_M, JOINT_2_NM_TO_IQ_B, JOINT_2_ANGLE_LIMIT_LOW, JOINT_2_ANGLE_LIMIT_HIGH, JOINT_2_MAX_SPEED, JOINT_2_MAX_ACCEL, JOINT_2_MAX_JERK, false);
    // // Joint 3 - MG8008 - Brachium Shoulder
    m_motors.emplace_back(static_cast<uint8_t>(3), canRef, MG8008_REDUCTION_RATIO, MG8008_SINGLE_TURN_DEG_SCALE_MAX, JOINT_3_NM_TO_IQ_M, JOINT_3_NM_TO_IQ_B, JOINT_3_ANGLE_LIMIT_LOW, JOINT_3_ANGLE_LIMIT_HIGH, JOINT_3_MAX_SPEED, JOINT_3_MAX_ACCEL, JOINT_3_MAX_JERK, false);
    // // Joint 4 - MG8008 - Elbow Flexor
    m_motors.emplace_back(static_cast<uint8_t>(4), canRef, MG8008_REDUCTION_RATIO, MG8008_SINGLE_TURN_DEG_SCALE_MAX, JOINT_4_NM_TO_IQ_M, JOINT_4_NM_TO_IQ_B, JOINT_4_ANGLE_LIMIT_LOW, JOINT_4_ANGLE_LIMIT_HIGH, JOINT_4_MAX_SPEED, JOINT_4_MAX_ACCEL, JOINT_4_MAX_JERK, false);
    // // Joint 5 - MG4010 - Forearm Rotator
    m_motors.emplace_back(static_cast<uint8_t>(5), canRef, MG4010_REDUCTION_RATIO, MG4010_SINGLE_TURN_DEG_SCALE_MAX, JOINT_5_NM_TO_IQ_M, JOINT_5_NM_TO_IQ_B, JOINT_5_ANGLE_LIMIT_LOW, JOINT_5_ANGLE_LIMIT_HIGH, JOINT_5_MAX_SPEED, JOINT_5_MAX_ACCEL, JOINT_5_MAX_JERK, false);
    // // Joint 6 - MG4005 - Wrist Differential #1 
    m_motors.emplace_back(static_cast<uint8_t>(6), canRef, MG4005_REDUCTION_RATIO, MG4005_SINGLE_TURN_DEG_SCALE_MAX, JOINT_6_NM_TO_IQ_M, JOINT_6_NM_TO_IQ_B, JOINT_6_ANGLE_LIMIT_LOW, JOINT_6_ANGLE_LIMIT_HIGH, DIFF_MAX_SPEED, DIFF_MAX_ACCEL, DIFF_MAX_JERK, true);
    // // Joint 7 - MG4005 - Wrist Differential #2
    m_motors.emplace_back(static_cast<uint8_t>(7), canRef, MG4005_REDUCTION_RATIO, MG4005_SINGLE_TURN_DEG_SCALE_MAX, JOINT_7_NM_TO_IQ_M, JOINT_7_NM_TO_IQ_B, JOINT_7_ANGLE_LIMIT_LOW, JOINT_7_ANGLE_LIMIT_HIGH, DIFF_MAX_SPEED, DIFF_MAX_ACCEL, DIFF_MAX_JERK, true);

    // Initialize kinematics if URDF path is provided
    if (!urdf_path.empty()) {
        if (!m_kinematics.loadURDF(urdf_path)) {
            std::cerr << "Failed to load URDF file: " << urdf_path << std::endl;
        }
    }
}

Motor& RobotInterface::getMotor(int i)
{
    if (i < 1 || i > (int)m_motors.size()) {
        throw std::out_of_range("Motor index out of range");
    }
    return m_motors[i - 1];
}

// Called from real-time daemon every CONTROL_PERIOD (200Hz default)
void RobotInterface::updateAll()
{
    updateJointStates();
    updateDifferentialMotors();
    updateJointTrajectories();


    // Update Cartesian state if kinematics is initialized
    // if (m_kinematics.getChain().getNrOfJoints() > 0) {
    //     std::array<double, 7> current_joints;
    //     for (size_t i = 0; i < m_motors.size(); ++i) {
    //         current_joints[i] = m_motors[i].getState().multiTurnRad_Mapped;
    //     }
    //     m_kinematics.getForwardKinematics(current_joints, m_state.current_pose);
    // }
}

// Sets joint angles for motors 1 through n - input angles in deg (they are converted to raw units after)
void RobotInterface::setMultiJointAngles(std::vector<float> joint_angles, std::vector<float> joint_speeds) {
    for (int i = 1; i <= joint_angles.size(); i++){
        float ang_target_deg = joint_angles.at(i-1);
        //std::cout << "[RobotInterface] Multi Joint Command Received | Motor: " << i << " | Target (deg): " << ang_target_deg << "\n";
        if ((i <= 5 && ang_target_deg >= 0.0) || (i == 6 || i == 7)) {
            auto &curr_mot = m_motors[i-1];
            float ang_target_raw = joint_angles.at(i-1);
            float maxSpeed = abs(joint_speeds.at(i-1));
            m_motors[i-1].setMultiAngleWithSpeed(static_cast<int32_t>(ang_target_raw), static_cast<uint16_t>(maxSpeed));
            //std::cout << "                 Target Angle Raw: " << ang_target_raw << "\n";
            //std::cout << "                 MaxSpeed: " << maxSpeed << "\n";
        }
    }
}

void RobotInterface::setMultiJointSpeeds(std::vector<float> joint_speeds) {
    for (int i = 1; i <= joint_speeds.size(); i++){
        auto &m = m_motors[i-1];
        float speed_target_deg_s = std::clamp(joint_speeds.at(i-1), 
                                      -m.getMaxSpeed()*m.getMaxSpeedModifier(),
                                      m.getMaxSpeed()*m.getMaxSpeedModifier());
        m_motors[i-1].setSpeed(speed_target_deg_s);
    }
}

void RobotInterface::updateDifferentialMotors() {
    m_state.differential_motors.right_motor_angle_rad = (m_motors[5].getState().multiTurnPosition / 10) * (M_PI / 180);
    m_state.differential_motors.left_motor_angle_rad = (m_motors[6].getState().multiTurnPosition / 10) * (M_PI / 180);
    double left_angle = m_state.differential_motors.left_motor_angle_rad;
    double right_angle = m_state.differential_motors.right_motor_angle_rad;
    
    double roll     = -0.5 * (left_angle + right_angle);
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

void RobotInterface::updateTwinDifferentialAnglesRad() {
    double left_angle = (m_state.twin_joint_angles_deg[6] / 10) * (M_PI / 180);
    double right_angle = (m_state.twin_joint_angles_deg[5] / 10) * (M_PI / 180);
    
    double roll     = -0.5 * (left_angle + right_angle);
    double rawPitch = 0.5 * (left_angle - right_angle);
    // Wrap pitch to [-π, π]
    double pitch = std::fmod(rawPitch + M_PI, 2*M_PI);
    if (pitch < 0) pitch += 2*M_PI;
    pitch -= M_PI;
    
    m_state.twin_diff_roll_rad = roll;
    m_state.twin_diff_pitch_rad = pitch;    
}

std::pair<double, double> RobotInterface::getTwinDifferentialAnglesRad(double target_roll_rad, double target_pitch_rad) {
    // Get current motor angles in raw units (degrees * 10)
    double current_left = m_state.twin_joint_angles_deg[6];
    double current_right = m_state.twin_joint_angles_deg[5];
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
    // From the forward kinematics equations:
    // roll = -0.5 * (left + right)
    // pitch = 0.5 * (left - right)
    // Solving for left and right:
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
    
    return {final_left, final_right};
}

// Calculate differential motor positions for given roll and pitch targets
std::pair<int32_t, int32_t> RobotInterface::getDifferentialAngles(double target_roll_rad, double target_pitch_rad) {
    // Get current motor angles in raw units (degrees * 10)
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
    // From the forward kinematics equations:
    // roll = -0.5 * (left + right)
    // pitch = 0.5 * (left - right)
    // Solving for left and right:
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
    
    return {final_left, final_right};
}

void RobotInterface::setDifferentialAngles(double target_roll_rad, double target_pitch_rad, double max_motor_speed) {
    // Get target motor positions
    auto [final_left, final_right] = getDifferentialAngles(target_roll_rad, target_pitch_rad);
    
    // Set the motor positions
    m_motors[6].setMultiAngleWithSpeed(final_left, max_motor_speed);
    m_motors[5].setMultiAngleWithSpeed(final_right, max_motor_speed);

    std::cerr << "[RobotInterface::setDifferentialAngles] Command Summary:" << std::endl
              << "  Inputs:" << std::endl
              << "    Target Roll: " << target_roll_rad << " rad (" << target_roll_rad * 180.0/M_PI << " deg)" << std::endl
              << "    Target Pitch: " << target_pitch_rad << " rad (" << target_pitch_rad * 180.0/M_PI << " deg)" << std::endl
              << "    Wrapped Pitch: " << std::fmod(target_pitch_rad + M_PI, 2*M_PI) - M_PI << " rad (" 
              << (std::fmod(target_pitch_rad + M_PI, 2*M_PI) - M_PI) * 180.0/M_PI << " deg)" << std::endl
              << "  Final Commands:" << std::endl
              << "    Left Motor (ID 7): Position=" << final_left << " (0.1 deg)" << std::endl
              << "    Right Motor (ID 6): Position=" << final_right << " (0.1 deg)" << std::endl;
}

// Move to joint position along ruckig generated joint trajectories
// Joints 6+7 are differential angles NOT motor angles.
// i = 5 -> Roll Position in Degrees
// i = 6 -> Pitch Position in Degrees
void RobotInterface::moveToJointPosition(const std::array<double, 7>& target_position)
{   
    if (m_state.trajectory_active) {
        std::cerr << "[RobotInterface::moveToJointPosition] Trajectory already active. Resetting ruckig state and running new trajectory." << std::endl;
        resetRuckigState();
    }
    std::cerr << "[RobotInterface::moveToJointPosition] Target Position: ";
    for (size_t i = 0; i < 7; ++i) {
        std::cerr << target_position[i] << " ";
    }
    std::cerr << std::endl;
    // Set target positions in state
    for (size_t i = 0; i < 5; ++i) {
        m_state.target_joint_angles_deg[i] = target_position[i];
        m_state.target_joint_speeds_deg_s[i] = 0.0; // Stop at target
        m_state.target_joint_accelerations_deg_s2[i] = 0.0;
        m_state.twin_joint_angles_deg[i] = m_state.joint_angles_deg[i]; // match on twin
    }
    // Calculate differential motor positions
    double target_roll_rad = target_position[5] * M_PI / 180.0;
    double target_pitch_rad = target_position[6] * M_PI / 180.0;

    std::cerr << "[RobotInterface::moveToJointPosition] DIFFERENTIAL CONVERSION:" << std::endl;
    std::cerr << "  Target Roll: " << target_position[5] << " deg (" << target_roll_rad << " rad)" << std::endl;
    std::cerr << "  Target Pitch: " << target_position[6] << " deg (" << target_pitch_rad << " rad)" << std::endl;
    
    // Log current motor states
    std::cerr << "  Current Left Motor (7): " << m_motors[6].getState().multiTurnPosition << " (raw)" << std::endl;
    std::cerr << "  Current Right Motor (6): " << m_motors[5].getState().multiTurnPosition << " (raw)" << std::endl;

    auto [final_left, final_right] = getDifferentialAngles(target_roll_rad, target_pitch_rad);
    
    std::cerr << "  Calculated Left Motor (7): " << final_left << " (raw) - diff: " << (final_left - m_motors[6].getState().multiTurnPosition) << std::endl;
    std::cerr << "  Calculated Right Motor (6): " << final_right << " (raw) - diff: " << (final_right - m_motors[5].getState().multiTurnPosition) << std::endl;
    
    m_state.target_joint_angles_deg[6] = final_left;
    m_state.target_joint_angles_deg[5] = final_right;

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

    // Log the actual input to Ruckig
    std::cerr << "[RobotInterface::moveToJointPosition] RUCKIG INPUT VERIFICATION:" << std::endl;
    std::cerr << "  Current Position: ";
    for (size_t i = 0; i < 7; ++i) {
        std::cerr << m_state.ruckig_input.current_position[i] << (i < 6 ? ", " : "");
    }
    std::cerr << std::endl;
    std::cerr << "  Target Position: ";
    for (size_t i = 0; i < 7; ++i) {
        std::cerr << m_state.ruckig_input.target_position[i] << (i < 6 ? ", " : "");
    }
    std::cerr << std::endl;

    // Activate trajectory
    m_state.trajectory_active = true;
    m_state.trajectory_progress = 0.0;
}

void RobotInterface::updateJointTrajectories()
{
    if (!m_state.trajectory_active) return;

    ruckig::Result r = m_state.ruckig_otg.update(m_state.ruckig_input, m_state.ruckig_output);
    if (r == ruckig::Result::Working) {
        // Debug the raw Ruckig output
        // std::cerr << "[RobotInterface::updateJointTrajectories] RAW RUCKIG OUTPUT ANALYSIS:" << std::endl;
        // for (size_t i = 0; i < 7; ++i) {
        //     std::cerr << "  Joint " << i+1 
        //             << " - Current: " << m_state.ruckig_input.current_position[i]
        //             << ", Target: " << m_state.ruckig_input.target_position[i] 
        //             << ", Ruckig New: " << m_state.ruckig_output.new_position[i]
        //             << ", Delta: " << (m_state.ruckig_output.new_position[i] - m_state.ruckig_input.current_position[i])
        //             << std::endl;
        // }
        
        // Convert arrays to vectors with proper type conversion
        std::vector<float> positions(7);
        std::vector<float> velocities(7);
        
        for (size_t i = 0; i < 7; ++i) {
            positions[i] = static_cast<float>(m_state.ruckig_output.new_position[i]);
            velocities[i] = static_cast<float>(m_state.ruckig_output.new_velocity[i]);
        }

        setTwinJointAngles(positions);
        std::vector<float> twin_joint_accelerations(7);
        for (size_t i = 0; i < 7; ++i) {
            twin_joint_accelerations[i] = static_cast<float>(m_state.ruckig_output.new_acceleration[i]);
        }
        setTwinJointAccelerations(twin_joint_accelerations);
        setTwinJointSpeeds(velocities);

        // Adjust speeds for differential motors
        velocities[5] /= 10.0f;
        velocities[6] /= 10.0f;

        // Apply PI controller to adjust velocities
        for (size_t i = 0; i < 7; ++i) {
            // Calculate position error (current Ruckig position - actual motor position)
            // Note: m_state.joint_angles_deg[i] was updated in updateAll() at 200Hz
            m_state.pi_controller.position_error[i] = positions[i] - m_state.joint_angles_deg[i];
            
            // Update integral error (with anti-windup)
            float new_integral = m_state.pi_controller.integral_error[i] + 
                               m_state.pi_controller.position_error[i] * (1.0f/200.0f); // dt = 1/200 for 200Hz
            
            // Anti-windup: limit integral term to prevent excessive accumulation
            m_state.pi_controller.integral_error[i] = std::clamp(new_integral, 
                                                               -m_state.pi_controller.max_integral, 
                                                               m_state.pi_controller.max_integral);
            
            // Calculate velocity correction
            float velocity_correction = m_state.pi_controller.Kp * m_state.pi_controller.position_error[i] +
                                     m_state.pi_controller.Ki * m_state.pi_controller.integral_error[i];
            
            // Apply correction to velocity
            velocities[i] += velocity_correction;
        
        }

        // Command adjusted velocities
        setMultiJointSpeeds(velocities);

        // Update Ruckig input for next cycle using current states
        for (size_t i = 0; i < 7; ++i) {
            m_state.ruckig_input.current_position[i] = m_state.joint_angles_deg[i];
            m_state.ruckig_input.current_velocity[i] = m_state.joint_speeds_deg_s[i];
            m_state.ruckig_input.current_acceleration[i] = m_state.joint_accelerations_deg_s2[i];
        }
        m_state.ruckig_output.pass_to_input(m_state.ruckig_input);
    } else if (r == ruckig::Result::Finished) {
        m_state.trajectory_active = false;
        std::cout << "Trajectory duration: " << m_state.ruckig_output.trajectory.get_duration() << " [s]. Setting hold position." << std::endl;
        setHoldPosition(); // THIS IS A HACK TO STOP ROBOT WHEN RUCKIG FINISHES, WILL NEED TO CHANGE WHEN DIRECTLY MOVING INTO ANOTHER TRAJECTORY.
    } else {
        // Handle errors by stopping trajectory and setting robot to hold position
        setHoldPosition();
        std::cout << "Trajectory error: " << r << std::endl;
    }
}

// New function: Move to Cartesian pose using inverse kinematics
// bool RobotInterface::moveToCartesianPose(const KDL::Frame& target_pose)
// {
//     if (m_kinematics.getChain().getNrOfJoints() == 0) {
//         std::cerr << "Kinematics not initialized" << std::endl;
//         return false;
//     }

//     // Get current joint angles
//     std::array<double, 7> current_joints;
//     for (size_t i = 0; i < m_motors.size(); ++i) {
//         current_joints[i] = m_motors[i].getState().multiTurnRad_Mapped;
//     }

//     // Calculate inverse kinematics
//     std::array<double, 7> target_joints;
//     if (!m_kinematics.getInverseKinematics(target_pose, current_joints, target_joints)) {
//         std::cerr << "Failed to calculate inverse kinematics" << std::endl;
//         return false;
//     }

//     // Convert to vector<float> for compatibility with existing interface
//     std::vector<float> joint_angles(target_joints.begin(), target_joints.end());
//     std::vector<float> joint_speeds(m_motors.size(), 1.0f); // Default speed of 1 rad/s
//     setMultiJointAngles(joint_angles, joint_speeds);

//     return true;
// }

void RobotInterface::setHoldPosition() { 
    resetRuckigState();
    for (auto &m : m_motors) { 
        m.setSpeed(0.0f); 
    } 
}

void RobotInterface::resetRuckigState()
{
    // Reset trajectory state
    m_state.trajectory_active = false;
    m_state.trajectory_duration = 0.0;
    m_state.trajectory_progress = 0.0;

    // Reset Ruckig input parameters to current state
    for (size_t i = 0; i < 7; ++i) {
        m_state.ruckig_input.current_position[i] = m_state.joint_angles_deg[i];
        m_state.ruckig_input.current_velocity[i] = 0.0;  // Reset velocity to 0
        m_state.ruckig_input.current_acceleration[i] = 0.0;  // Reset acceleration to 0
        
        // Reset target parameters to current position
        m_state.ruckig_input.target_position[i] = m_state.joint_angles_deg[i];
        m_state.ruckig_input.target_velocity[i] = 0.0;
        m_state.ruckig_input.target_acceleration[i] = 0.0;
        
        // Keep the limits unchanged
        m_state.ruckig_input.max_velocity[i] = m_state.joint_max_speeds_deg_s[i];
        m_state.ruckig_input.max_acceleration[i] = m_state.joint_max_accelerations_deg_s2[i];
        m_state.ruckig_input.max_jerk[i] = m_state.joint_max_jerks_deg_s3[i];
    }

    // Reset Ruckig output parameters
    m_state.ruckig_output = ruckig::OutputParameter<7>();
    std::cout << "[RobotInterface::resetRuckigState] Ruckig state reset." << std::endl;
}

void RobotInterface::setESTOP() { 
    std::cout << "\n\n[RobotInterface::setESTOP] EMERGENCY STOP ACTIVATING - trajectory_active=" << (m_state.trajectory_active ? "true" : "false") << "\n\n";
    resetRuckigState(); 
    for (auto &m : m_motors) { 
        m.motorStop(); 
    } 
}

void RobotInterface::setTwinJointAngles(std::vector<float> angles)
{
    for (size_t i = 0; i < 7; ++i) {
        m_state.twin_joint_angles_deg[i] = angles[i];
    }

}

void RobotInterface::setTwinJointSpeeds(std::vector<float> speeds)
{
    for (size_t i = 0; i < 7; ++i) {
        m_state.twin_joint_speeds_deg_s[i] = speeds[i];
    }
}

void RobotInterface::setTwinJointAccelerations(std::vector<float> accelerations)
{
    for (size_t i = 0; i < 7; ++i) {
        m_state.twin_joint_accelerations_deg_s2[i] = accelerations[i];
    }
}

void RobotInterface::updateJointStates() {
    int i = 0;
    for(auto &m : m_motors) {
        // Get current speed before reading new state
        m_state.prev_joint_speeds_deg_s[i] = m_state.joint_speeds_deg_s[i];
        m_state.prev_joint_angles_deg[i] = m_state.joint_angles_deg[i];

        // Read new state after updating previous states
        m.readState2();
        m.readSingleAngle();
        m.readMultiAngle();
        
        if (i == 5 || i == 6) {
            m_state.joint_angles_deg[i] = m.getState().multiTurnPosition; // Stay in raw units for differential motors
            m_state.joint_max_speeds_deg_s[i] = m.getMaxSpeed()*m.getMaxSpeedModifier()*10;
            m_state.joint_max_accelerations_deg_s2[i] = m.getMaxAcceleration()*m.getMaxSpeedModifier()*10;
            m_state.joint_max_jerks_deg_s3[i] = m.getMaxJerk()*m.getMaxSpeedModifier()*10;
        } else {
            m_state.joint_angles_deg[i] = m.getState().multiTurnDeg_Mapped;
            m_state.joint_max_speeds_deg_s[i] = m.getMaxSpeed()*m.getMaxSpeedModifier();
            m_state.joint_max_accelerations_deg_s2[i] = m.getMaxAcceleration()*m.getMaxSpeedModifier();
            m_state.joint_max_jerks_deg_s3[i] = m.getMaxJerk()*m.getMaxSpeedModifier();
        }
        // Calculate velocity and acceleration using previous and new speed with 200Hz control rate (0.005s timestep)
        m_state.joint_speeds_deg_s[i] = (m_state.joint_angles_deg[i] - m_state.prev_joint_angles_deg[i]) / 0.005;
        m_state.joint_accelerations_deg_s2[i] = (m_state.joint_speeds_deg_s[i] - m_state.prev_joint_speeds_deg_s[i]) / 0.005;
        updateTwinDifferentialAnglesRad();
        
        i++;
    }

}