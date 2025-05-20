#include "kdl_parser.hpp"
#include <fstream>
#include <sstream>
#include <kdl/frames_io.hpp>

namespace kdl_parser {

KDL::Vector toKdl(const Vector3& v) {
    return KDL::Vector(v.x, v.y, v.z);
}

KDL::Rotation toKdl(const Rotation& r) {
    return KDL::Rotation::Quaternion(r.x, r.y, r.z, r.w);
}

KDL::Frame toKdl(const Pose& p) {
    return KDL::Frame(toKdl(p.rotation), toKdl(p.position));
}

KDL::Joint toKdl(Joint* jnt) {
    KDL::Frame origin = toKdl(jnt->origin);
    
    switch (jnt->type) {
        case Joint::FIXED:
            return KDL::Joint(jnt->name, KDL::Joint::None);
        case Joint::REVOLUTE:
        case Joint::CONTINUOUS:
            return KDL::Joint(jnt->name, origin.p, 
                             origin.M * toKdl(jnt->axis), 
                             KDL::Joint::RotAxis);
        case Joint::PRISMATIC:
            return KDL::Joint(jnt->name, origin.p, 
                             origin.M * toKdl(jnt->axis), 
                             KDL::Joint::TransAxis);
        default:
            std::cerr << "Unknown joint type for joint '" << jnt->name << "', using fixed joint" << std::endl;
            return KDL::Joint(jnt->name, KDL::Joint::None);
    }
}

KDL::RigidBodyInertia toKdl(const Inertial& i) {
    KDL::Frame origin = toKdl(i.origin);
    
    // Mass is frame independent
    double kdl_mass = i.mass;
    
    // Center of mass in link reference frame
    KDL::Vector kdl_com = origin.p;
    
    // Inertia matrix in inertia reference frame
    KDL::RotationalInertia inertia(i.ixx, i.iyy, i.izz, i.ixy, i.ixz, i.iyz);
    
    // Transform to link reference frame
    KDL::RigidBodyInertia kdl_inertia_wrt_com_workaround = 
        origin.M * KDL::RigidBodyInertia(0, KDL::Vector::Zero(), inertia);
    
    KDL::RotationalInertia kdl_inertia_wrt_com = 
        kdl_inertia_wrt_com_workaround.getRotationalInertia();
    
    return KDL::RigidBodyInertia(kdl_mass, kdl_com, kdl_inertia_wrt_com);
}

bool Model::initFile(const std::string& filename) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load URDF file: " << filename << std::endl;
        std::cerr << "Error: " << doc.ErrorStr() << std::endl;
        return false;
    }
    
    tinyxml2::XMLElement* robot_xml = doc.FirstChildElement("robot");
    if (!robot_xml) {
        std::cerr << "URDF file does not contain a <robot> element" << std::endl;
        return false;
    }
    
    // Parse all links
    for (tinyxml2::XMLElement* link_xml = robot_xml->FirstChildElement("link");
         link_xml; link_xml = link_xml->NextSiblingElement("link")) {
        if (!parseLink(link_xml)) {
            return false;
        }
    }
    
    // Parse all joints
    for (tinyxml2::XMLElement* joint_xml = robot_xml->FirstChildElement("joint");
         joint_xml; joint_xml = joint_xml->NextSiblingElement("joint")) {
        if (!parseJoint(joint_xml)) {
            return false;
        }
    }
    
    // Build parent-child link relationships
    for (auto& joint_pair : joints) {
        Joint& joint = joint_pair.second;
        Link* parent = getLink(joint.parent_link);
        Link* child = getLink(joint.child_link);
        
        if (!parent || !child) {
            std::cerr << "Joint '" << joint.name << "' references non-existent links" << std::endl;
            return false;
        }
        
        parent->child_links.push_back(child->name);
    }
    
    // Find root link (link with no parent)
    for (auto& link_pair : links) {
        bool has_parent = false;
        for (auto& joint_pair : joints) {
            if (joint_pair.second.child_link == link_pair.first) {
                has_parent = true;
                break;
            }
        }
        
        if (!has_parent) {
            root_link = &link_pair.second;
            break;
        }
    }
    
    if (!root_link) {
        std::cerr << "Could not find a root link in the URDF" << std::endl;
        return false;
    }
    
    return true;
}

Link* Model::getLink(const std::string& name) {
    auto it = links.find(name);
    if (it != links.end()) {
        return &it->second;
    }
    return nullptr;
}

Joint* Model::getJoint(const std::string& name) {
    auto it = joints.find(name);
    if (it != joints.end()) {
        return &it->second;
    }
    return nullptr;
}

bool Model::parseLink(tinyxml2::XMLElement* link_xml) {
    const char* name = link_xml->Attribute("name");
    if (!name) {
        std::cerr << "Link has no name attribute" << std::endl;
        return false;
    }
    
    Link link;
    link.name = name;
    
    // Parse inertial properties
    tinyxml2::XMLElement* inertial_xml = link_xml->FirstChildElement("inertial");
    if (inertial_xml) {
        if (!parseInertial(inertial_xml, link.inertial)) {
            return false;
        }
    }
    
    links[name] = link;
    return true;
}

bool Model::parseJoint(tinyxml2::XMLElement* joint_xml) {
    const char* name = joint_xml->Attribute("name");
    if (!name) {
        std::cerr << "Joint has no name attribute" << std::endl;
        return false;
    }
    
    Joint joint;
    joint.name = name;
    
    // Parse joint type
    const char* type_str = joint_xml->Attribute("type");
    if (!type_str) {
        std::cerr << "Joint '" << name << "' has no type attribute" << std::endl;
        return false;
    }
    
    std::string type(type_str);
    if (type == "revolute") joint.type = Joint::REVOLUTE;
    else if (type == "continuous") joint.type = Joint::CONTINUOUS;
    else if (type == "prismatic") joint.type = Joint::PRISMATIC;
    else if (type == "floating") joint.type = Joint::FLOATING;
    else if (type == "planar") joint.type = Joint::PLANAR;
    else if (type == "fixed") joint.type = Joint::FIXED;
    else {
        std::cerr << "Joint '" << name << "' has unknown type: " << type << std::endl;
        return false;
    }
    
    // Parse origin
    tinyxml2::XMLElement* origin_xml = joint_xml->FirstChildElement("origin");
    if (origin_xml) {
        if (!parseOrigin(origin_xml, joint.origin)) {
            return false;
        }
    }
    
    // Parse axis
    tinyxml2::XMLElement* axis_xml = joint_xml->FirstChildElement("axis");
    if (axis_xml) {
        const char* xyz_str = axis_xml->Attribute("xyz");
        if (xyz_str) {
            std::stringstream ss(xyz_str);
            ss >> joint.axis.x >> joint.axis.y >> joint.axis.z;
        }
    } else {
        // Default axis is Z
        joint.axis.x = 0;
        joint.axis.y = 0;
        joint.axis.z = 1;
    }
    
    // Parse parent and child links
    tinyxml2::XMLElement* parent_xml = joint_xml->FirstChildElement("parent");
    tinyxml2::XMLElement* child_xml = joint_xml->FirstChildElement("child");
    
    if (!parent_xml || !child_xml) {
        std::cerr << "Joint '" << name << "' is missing parent or child elements" << std::endl;
        return false;
    }
    
    const char* parent_link = parent_xml->Attribute("link");
    const char* child_link = child_xml->Attribute("link");
    
    if (!parent_link || !child_link) {
        std::cerr << "Joint '" << name << "' has invalid parent or child links" << std::endl;
        return false;
    }
    
    joint.parent_link = parent_link;
    joint.child_link = child_link;
    
    joints[name] = joint;
    return true;
}

bool Model::parseInertial(tinyxml2::XMLElement* inertial_xml, Inertial& inertial) {
    // Parse mass
    tinyxml2::XMLElement* mass_xml = inertial_xml->FirstChildElement("mass");
    if (mass_xml) {
        mass_xml->QueryDoubleAttribute("value", &inertial.mass);
    }
    
    // Parse origin
    tinyxml2::XMLElement* origin_xml = inertial_xml->FirstChildElement("origin");
    if (origin_xml) {
        if (!parseOrigin(origin_xml, inertial.origin)) {
            return false;
        }
    }
    
    // Parse inertia
    tinyxml2::XMLElement* inertia_xml = inertial_xml->FirstChildElement("inertia");
    if (inertia_xml) {
        inertia_xml->QueryDoubleAttribute("ixx", &inertial.ixx);
        inertia_xml->QueryDoubleAttribute("ixy", &inertial.ixy);
        inertia_xml->QueryDoubleAttribute("ixz", &inertial.ixz);
        inertia_xml->QueryDoubleAttribute("iyy", &inertial.iyy);
        inertia_xml->QueryDoubleAttribute("iyz", &inertial.iyz);
        inertia_xml->QueryDoubleAttribute("izz", &inertial.izz);
    }
    
    return true;
}

bool Model::parseOrigin(tinyxml2::XMLElement* origin_xml, Pose& pose) {
    // Parse xyz
    const char* xyz_str = origin_xml->Attribute("xyz");
    if (xyz_str) {
        std::stringstream ss(xyz_str);
        ss >> pose.position.x >> pose.position.y >> pose.position.z;
    }
    
    // Parse rpy
    const char* rpy_str = origin_xml->Attribute("rpy");
    if (rpy_str) {
        double roll, pitch, yaw;
        std::stringstream ss(rpy_str);
        ss >> roll >> pitch >> yaw;
        
        // Convert RPY to quaternion
        double sr = sin(roll/2.0);
        double cr = cos(roll/2.0);
        double sp = sin(pitch/2.0);
        double cp = cos(pitch/2.0);
        double sy = sin(yaw/2.0);
        double cy = cos(yaw/2.0);
        
        pose.rotation.x = sr*cp*cy - cr*sp*sy;
        pose.rotation.y = cr*sp*cy + sr*cp*sy;
        pose.rotation.z = cr*cp*sy - sr*sp*cy;
        pose.rotation.w = cr*cp*cy + sr*sp*sy;
    }
    
    return true;
}

bool addChildrenToTree(Link* root, KDL::Tree& tree, Model& model) {
    // Loop through all child links
    for (const std::string& child_name : root->child_links) {
        Link* child = model.getLink(child_name);
        if (!child) {
            std::cerr << "Could not find child link '" << child_name << "'" << std::endl;
            return false;
        }
        
        // Find the joint connecting parent to child
        Joint* joint = nullptr;
        for (auto& joint_pair : model.joints) {
            if (joint_pair.second.parent_link == root->name &&
                joint_pair.second.child_link == child_name) {
                joint = &joint_pair.second;
                break;
            }
        }
        
        if (!joint) {
            std::cerr << "Could not find joint connecting '" << root->name << "' to '" << child_name << "'" << std::endl;
            return false;
        }
        
        // Create KDL joint
        KDL::Joint kdl_joint = toKdl(joint);
        
        // Create inertia
        KDL::RigidBodyInertia inertia(0);
        if (child->inertial.mass > 0) {
            inertia = toKdl(child->inertial);
        }
        
        // Create segment
        KDL::Segment segment(child->name, kdl_joint, toKdl(joint->origin), inertia);
        
        // Add segment to tree
        tree.addSegment(segment, root->name);
        
        // Recursively add child's children
        if (!addChildrenToTree(child, tree, model)) {
            return false;
        }
    }
    
    return true;
}

bool treeFromFile(const std::string& filename, KDL::Tree& tree) {
    Model model;
    if (!model.initFile(filename)) {
        return false;
    }
    
    Link* root = model.getRoot();
    if (!root) {
        std::cerr << "Could not find root link" << std::endl;
        return false;
    }
    
    // Create tree
    tree = KDL::Tree(root->name);
    
    // Add all children
    for (const std::string& child_name : root->child_links) {
        Link* child = model.getLink(child_name);
        if (!child) {
            std::cerr << "Could not find child link '" << child_name << "'" << std::endl;
            return false;
        }
        
        if (!addChildrenToTree(child, tree, model)) {
            return false;
        }
    }
    
    return true;
}

} // namespace kdl_parser 