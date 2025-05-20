#ifndef ROBOT_INTERFACE_HPP
#define ROBOT_INTERFACE_HPP

#include "motor_interface.hpp"
#include "kinematics_interface.hpp"
#include <kdl/frames.hpp>
#include <ruckig/ruckig.hpp>
#include <vector>
#include <array>

struct DifferentialMotorState
{
    double left_motor_angle_rad = 0.0;
    double right_motor_angle_rad = 0.0;
    double roll_angle_rad = 0.0;
    double pitch_angle_rad = 0.0;
    double roll_angle_deg = 0.0;
    double pitch_angle_deg = 0.0;
};

/* 
 * @brief Holds the current state of the robot.
 *        
 */
struct RobotState
{
    // State Currents (radians and degrees for convenience)
    double joint_angles_rad[7] = {0.0};
    double joint_speeds_rad_s[7] = {0.0};
    double joint_accelerations_rad_s2[7] = {0.0};
    // Degrees
    double joint_angles_deg[7] = {0.0};
    double joint_speeds_deg_s[7] = {0.0};
    double joint_accelerations_deg_s2[7] = {0.0};
    double prev_joint_angles_deg[7] = {0.0};    // Previous cycle positions for velocity calculation
    double prev_joint_speeds_deg_s[7] = {0.0};  // Previous cycle speeds for acceleration calculation
    DifferentialMotorState differential_motors;

    // State Targets (radians and degrees for convenience)
    double target_joint_angles_rad[7] = {0.0};
    double target_joint_speeds_rad_s[7] = {0.0};
    double target_joint_accelerations_rad_s2[7] = {0.0};
    // Degrees
    double target_joint_angles_deg[7] = {0.0};
    double target_joint_speeds_deg_s[7] = {0.0};
    double target_joint_accelerations_deg_s2[7] = {0.0};

    // State Limits 
    double joint_max_speeds_deg_s[7] = {0.0};
    double joint_max_accelerations_deg_s2[7] = {0.0};
    double joint_max_jerks_deg_s3[7] = {0.0};

    // Cartesian State
    KDL::Frame current_pose;
    KDL::Frame target_pose;
    KDL::Twist current_twist;
    KDL::Twist target_twist;

    // Ruckig State
    ruckig::InputParameter<7> ruckig_input;
    ruckig::OutputParameter<7> ruckig_output;
    ruckig::Ruckig<7> ruckig_otg{0.005}; // otg = online trajectory generation lol (SCREW ACRONYMS)

    // Trajectory State
    bool trajectory_active{false};
    double trajectory_duration{0.0};
    double trajectory_progress{0.0};

    // Max Speed Modifier
    float max_speed_modifier = 0.16666666667;

    // Digital Twin States
    double twin_joint_angles_deg[7] = {0.0};
    double twin_diff_roll_rad = 0.0;
    double twin_diff_pitch_rad = 0.0;
    double twin_joint_speeds_deg_s[7] = {0.0};
    double twin_joint_accelerations_deg_s2[7] = {0.0};
    bool twin_active = true;

    // PI Controller State
    struct {
        float position_error[7] = {0.0f};      // Current position error for each joint
        float integral_error[7] = {0.0f};      // Accumulated integral error for each joint
        float Kp = 8.0f;                       // Proportional gain - Tuned manually NOT mathematically derived
        float Ki = 0.0f;                       // Integral gain
        float max_integral = 10.0f;            // Maximum integral term to prevent windup
    } pi_controller;
};

/**
 * @brief RobotInterface manages multiple MG motors and kinematics.
 *        It can easily scale from 1 to 7 or more joints.
 */
class RobotInterface
{
public:
    /**
     * @param canRef A reference to an already-initialized CANHandler
     * @param urdf_path Path to the URDF file describing the robot
     */
    RobotInterface(CANHandler& canRef, const std::string& urdf_path);

    /**
     * @brief Get a reference to motor i [1..numMotors].
     */
    Motor& getMotor(int i);

    /**
     * @brief Example update: read each motor's state for logging or safety checks.
     */
    void updateAll();

    /**
     * @brief Set joint angles for multiple joints
     * @param joint_angles Target joint angles in radians
     * @param joint_speeds Target joint speeds in rad/s
     */
    void setMultiJointAngles(std::vector<float> joint_angles, std::vector<float> joint_speeds);

    /**
     * @brief Set joint speeds for multiple joints
     * @param joint_speeds Target joint speeds 
     */
    void setMultiJointSpeeds(std::vector<float> joint_speeds);

    /**
     * @brief Move the end-effector to a specific Cartesian pose
     * @param target_pose Desired end-effector pose
     * @return true if successful, false otherwise
     */
    bool moveToCartesianPose(const KDL::Frame& target_pose);

    /**
     * @brief Move joints to specific positions
     * @param target_position Target joint positions in degrees
     */
    void moveToJointPosition(const std::array<double, 7>& target_position);

    /**
     * @brief Update the trajectory state
     */
    void updateJointTrajectories();

    /**
     * @brief Get the current robot state
     * @return Reference to the current robot state
     */
    const RobotState& getState() const { return m_state; }

    void setDifferentialAngles(double target_roll_rad, double target_pitch_rad, double max_motor_speed);

    /**
     * @brief Set the max speed modifier
     */
    void setMaxSpeedModifier(float modifier) { m_state.max_speed_modifier = modifier; for (auto &m : m_motors) { m.setMaxSpeedModifier(modifier); } }

    /**
     * @brief Reset the Ruckig state to prepare for a new trajectory
     * This method resets the Ruckig input/output parameters and trajectory state
     * without affecting motor commands (which are handled by setHoldPosition/setESTOP)
     */
    void resetRuckigState();

    /**
     * @brief Set the hold position
     */
    void setHoldPosition();

    /**
     * @brief Set the ESTOP
     */
    void setESTOP();

    /**
     * @brief Set the digital twin joint angles
     * @param angles Array of 7 joint angles in degrees
     */
    void setTwinJointAngles(std::vector<float> angles);

    /**
     * @brief Set the digital twin joint speeds
     * @param speeds Array of 7 joint speeds in degrees per second
     */
    void setTwinJointSpeeds(std::vector<float> speeds);

    /**
     * @brief Set the digital twin joint accelerations
     * @param accelerations Array of 7 joint accelerations in degrees per second squared
     */
    void setTwinJointAccelerations(std::vector<float> accelerations);

    /**
     * @brief Set whether the digital twin is active
     * @param active Whether the twin should be active
     */
    void setTwinActive(bool active) { m_state.twin_active = active; }

    /**
     * @brief Get the target differential angles in radians
     * @return A pair of double values representing the target differential angles in radians
     */
    std::pair<double, double> getTwinDifferentialAnglesRad(double target_roll_rad, double target_pitch_rad);

    void updateTwinDifferentialAnglesRad();

    void updateJointStates();
private:
    std::vector<Motor> m_motors;
    RobotState m_state;
    KinematicsInterface m_kinematics;
    void updateDifferentialMotors();
    std::pair<int32_t, int32_t> getDifferentialAngles(double target_roll_rad, double target_pitch_rad);
    static double wrap180(double deg) {double w = std::fmod(deg + 180.0, 360.0); if (w < 0) w += 360.0; return w - 180.0;}
    double m_pi = 3.14159265359;
};

#endif // ROBOT_INTERFACE_HPP
