#ifndef KINEMATICS_INTERFACE_HPP
#define KINEMATICS_INTERFACE_HPP

#include <kdl/chain.hpp>
#include <kdl/tree.hpp>
#include <kdl/chainfksolverpos_recursive.hpp>
#include <kdl/chainiksolvervel_pinv.hpp>
#include <kdl/chainiksolverpos_nr.hpp>
#include <string>
#include <memory>
#include <array>

/**
 * @brief Handles all kinematics calculations for the robot using KDL
 */
class KinematicsInterface
{
public:
    KinematicsInterface();
    ~KinematicsInterface() = default;

    /**
     * @brief Loads the robot's URDF file and initializes the kinematic chain
     * @param urdf_path Path to the URDF file
     * @return true if successful, false otherwise
     */
    bool loadURDF(const std::string& urdf_path);

    /**
     * @brief Calculates the end-effector pose from joint angles
     * @param joint_angles Array of 7 joint angles in radians
     * @return The end-effector pose
     */
    KDL::Frame getForwardKinematics(const std::array<double, 7>& joint_angles);

    /**
     * @brief Calculates the end-effector pose from joint angles
     * @param joint_angles Array of 7 joint angles in radians
     * @param end_effector_pose Output parameter for the resulting pose
     * @return true if successful, false otherwise
     */
    bool getForwardKinematics(const std::array<double, 7>& joint_angles, KDL::Frame& end_effector_pose);

    /**
     * @brief Calculates joint angles for a desired end-effector pose
     * @param target_pose Desired end-effector pose
     * @param current_joints Current joint angles (used as initial guess)
     * @param joint_angles Output parameter for the resulting joint angles
     * @return true if successful, false otherwise
     */
    bool getInverseKinematics(const KDL::Frame& target_pose, 
                             const std::array<double, 7>& current_joints,
                             std::array<double, 7>& joint_angles);

    /**
     * @brief Gets the current kinematic chain
     * @return Reference to the KDL chain
     */
    const KDL::Chain& getChain() const { return chain; }

private:
    KDL::Chain chain;
    KDL::Tree tree;
    std::unique_ptr<KDL::ChainFkSolverPos_recursive> fk_solver;
    std::unique_ptr<KDL::ChainIkSolverVel_pinv> ik_solver_vel;
    std::unique_ptr<KDL::ChainIkSolverPos_NR> ik_solver_pos;
};

#endif // KINEMATICS_INTERFACE_HPP 