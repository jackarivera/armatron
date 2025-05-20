#include "kinematics_interface.hpp"
#include "kdl_parser.hpp"
#include <stdexcept>
#include <iostream>

KinematicsInterface::KinematicsInterface()
{
    // Initialize empty chain and tree
}

bool KinematicsInterface::loadURDF(const std::string& urdf_path)
{
    // Parse URDF file into KDL tree
    if (!kdl_parser::treeFromFile(urdf_path, tree)) {
        std::cerr << "[KinematicsInterface] Failed to parse URDF file: " << urdf_path << std::endl;
        return false;
    }

    // Extract chain from base to end-effector
    if (!tree.getChain("base_link", "end_effector", chain)) {
        std::cerr << "[KinematicsInterface] Failed to get chain from base_link to end_effector" << std::endl;
        return false;
    }

    // Initialize solvers
    fk_solver = std::make_unique<KDL::ChainFkSolverPos_recursive>(chain);
    ik_solver_vel = std::make_unique<KDL::ChainIkSolverVel_pinv>(chain);
    ik_solver_pos = std::make_unique<KDL::ChainIkSolverPos_NR>(chain, *fk_solver, *ik_solver_vel);

    std::cout << "[KinematicsInterface] Successfully loaded URDF and initialized kinematic chain" << std::endl;
    return true;
}

KDL::Frame KinematicsInterface::getForwardKinematics(const std::array<double, 7>& joint_angles)
{
    KDL::Frame result;
    getForwardKinematics(joint_angles, result);
    return result;
}

bool KinematicsInterface::getForwardKinematics(
    const std::array<double, 7>& joint_angles,
    KDL::Frame& end_effector_pose)
{
    // Convert array to KDL joint array
    KDL::JntArray kdl_joints(7);
    for(int i = 0; i < 7; i++) {
        kdl_joints(i) = joint_angles[i];
    }

    // Calculate forward kinematics
    int result = fk_solver->JntToCart(kdl_joints, end_effector_pose);
    if (result < 0) {
        std::cerr << "[KinematicsInterface] Forward kinematics calculation failed" << std::endl;
        return false;
    }

    return true;
}

bool KinematicsInterface::getInverseKinematics(
    const KDL::Frame& target_pose,
    const std::array<double, 7>& current_joints,
    std::array<double, 7>& joint_angles)
{
    // Create initial guess from current joint angles
    KDL::JntArray initial_guess(7);
    for(int i = 0; i < 7; i++) {
        initial_guess(i) = current_joints[i];
    }

    // Create output array
    KDL::JntArray result_joints(7);

    // Calculate inverse kinematics
    int result = ik_solver_pos->CartToJnt(initial_guess, target_pose, result_joints);
    if (result < 0) {
        std::cerr << "[KinematicsInterface] Inverse kinematics calculation failed" << std::endl;
        return false;
    }

    // Copy results back to output array
    for(int i = 0; i < 7; i++) {
        joint_angles[i] = result_joints(i);
    }

    return true;
} 