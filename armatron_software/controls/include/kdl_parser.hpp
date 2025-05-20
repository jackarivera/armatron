#ifndef KDL_PARSER_HPP
#define KDL_PARSER_HPP

#include <string>
#include <kdl/tree.hpp>
#include <tinyxml2.h>
#include <iostream>
#include <vector>
#include <map>
#include <kdl/chain.hpp>
#include <kdl/joint.hpp>
#include <kdl/segment.hpp>
#include <kdl/rigidbodyinertia.hpp>
#include <kdl/rotationalinertia.hpp>

namespace kdl_parser {

/**
 * @brief Simple struct to represent a vector in 3D space
 */
struct Vector3 {
    double x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(double x, double y, double z) : x(x), y(y), z(z) {}
};

/**
 * @brief Simple struct to represent a rotation as a quaternion
 */
struct Rotation {
    double x, y, z, w;
    Rotation() : x(0), y(0), z(0), w(1) {}
    Rotation(double x, double y, double z, double w) : x(x), y(y), z(z), w(w) {}
};

/**
 * @brief Simple struct to represent a pose (position and orientation)
 */
struct Pose {
    Vector3 position;
    Rotation rotation;
    Pose() {}
};

/**
 * @brief Simple struct to represent a joint
 */
struct Joint {
    enum JointType {
        UNKNOWN, REVOLUTE, CONTINUOUS, PRISMATIC, FLOATING, PLANAR, FIXED
    };
    
    std::string name;
    JointType type;
    Pose origin;
    Vector3 axis;
    std::string parent_link;
    std::string child_link;
    
    Joint() : type(UNKNOWN) {}
};

/**
 * @brief Simple struct to represent a link's inertial properties
 */
struct Inertial {
    double mass;
    Pose origin;
    double ixx, ixy, ixz, iyy, iyz, izz;
    
    Inertial() : mass(0), ixx(0), ixy(0), ixz(0), iyy(0), iyz(0), izz(0) {}
};

/**
 * @brief Simple struct to represent a link
 */
struct Link {
    std::string name;
    Inertial inertial;
    std::vector<std::string> child_links;
    
    Link() {}
};

/**
 * @brief Simple URDF model
 */
class Model {
public:
    /**
     * @brief Initialize the model from a URDF file
     * @param filename Path to the URDF file
     * @return True if successful, false otherwise
     */
    bool initFile(const std::string& filename);
    
    /**
     * @brief Get a link by name
     * @param name Name of the link
     * @return Pointer to the link, or nullptr if not found
     */
    Link* getLink(const std::string& name);
    
    /**
     * @brief Get a joint by name
     * @param name Name of the joint
     * @return Pointer to the joint, or nullptr if not found
     */
    Joint* getJoint(const std::string& name);
    
    /**
     * @brief Get the root link
     * @return Pointer to the root link
     */
    Link* getRoot() { return root_link; }

    std::map<std::string, Joint> joints;
    
private:
    std::map<std::string, Link> links;
    Link* root_link;
    
    bool parseLink(tinyxml2::XMLElement* link_xml);
    bool parseJoint(tinyxml2::XMLElement* joint_xml);
    bool parseInertial(tinyxml2::XMLElement* inertial_xml, Inertial& inertial);
    bool parseOrigin(tinyxml2::XMLElement* origin_xml, Pose& pose);
    bool parseXYZ(tinyxml2::XMLElement* xyz_xml, Vector3& vector);
    bool parseRPY(tinyxml2::XMLElement* rpy_xml, Rotation& rotation);
};

/**
 * @brief Construct a KDL tree from a URDF file
 * @param filename Path to the URDF file
 * @param tree Output KDL tree
 * @return True if successful, false otherwise
 */
bool treeFromFile(const std::string& filename, KDL::Tree& tree);

/**
 * @brief Helper function to convert Vector3 to KDL::Vector
 */
KDL::Vector toKdl(const Vector3& v);

/**
 * @brief Helper function to convert Rotation to KDL::Rotation
 */
KDL::Rotation toKdl(const Rotation& r);

/**
 * @brief Helper function to convert Pose to KDL::Frame
 */
KDL::Frame toKdl(const Pose& p);

/**
 * @brief Helper function to convert Joint to KDL::Joint
 */
KDL::Joint toKdl(Joint* jnt);

/**
 * @brief Helper function to convert Inertial to KDL::RigidBodyInertia
 */
KDL::RigidBodyInertia toKdl(const Inertial& i);

/**
 * @brief Recursive function to add child links to KDL tree
 */
bool addChildrenToTree(Link* root, KDL::Tree& tree, Model& model);

} // namespace kdl_parser

#endif // KDL_PARSER_HPP 