# Armatron 7-DOF Robot Arm

## Project Overview
Armatron is an open-source 7-degree-of-freedom robotic arm project that combines hardware, software, and web-based control. This repository contains all the necessary files to build, program, and control your own Armatron robot arm.

## Repository Structure
```
.
├── armatron_software/     # Software implementation
│   ├── controls/         # C++ control system
│   ├── web/             # Web interface
├── CAD Files/           # CAD models and drawings
└── resources/           # Additional resources
```

## Hardware Requirements

### Electronics
- BeagleBone Black (BBB)
- 7x Fully Integrated BLDC Motors
- Armatron Electronics System (See BOM and Electronics CAD)

### Mechanical Parts
- 3D Printed Components (STL files and Fusion360 Project Provided)
- Bearings
- Fasteners
- Timing Belts

## Software Requirements
- WSL2 with Ubuntu (recommended)
- Python 3.x
- Node.js
- Git
- CMake
- Cross-compilation toolchain

## Getting Started

### 1. Hardware Assembly
1. Download the CAD files / Fusion 360 Project from the CAD files directory
2. Familiarize yourself with the mechanical structure of the project
3. Print or manufacture the structural pieces within the project (Recommended 3D Print Material: PA12 Nylon CF [Bambulabs PAHT-CF or similar])
4. Assemble mechanical pieces
5. Wire the motors. This is left up to you how you want to wire it. When I built armatron, I hand soldered a long wiring harness using the electrical raceway design I provided. This was extremely taxing and I recommend brainstorming new wiring methods that make it easier to assemble.

### 2. Software Setup
1. Clone this repository
2. Navigate to the software directory
3. Run the setup script:
   ```bash
   ./setup_dev.sh
   ```
4. The setup script should ideally set up everything for cross compilation and remote development. I recommend using WSL2 or a linux machine for this. This script is specifically for developing remotely and cross compiling with deployments. You can of course also just build both the C++ project and Web dashboard on whatever control computer you are using instead. I leave this up to you to decide.

### 3. Deployment - if remotely deploying
1. Configure your BBB's IP and credentials in the setup files
2. Run the deployment script:
   ```bash
   sudo ./deploy.sh
   ```

## CAD Files
The `CAD Files` directory contains:
- Complete Fusion 360 project file
- Individual component files in multiple formats - These aren't organized that great so I recommend pulling new STL files from the fusion project


## Disclaimer
While this project has been built up to an actual product that functions correctly and has a lot of potential, it does lack comprehensive documentation, assembly guides, and troubleshooting tips. I do apologize for this, however, I completed this project in my final semester of college at Purdue University. I am now transitioning into a full time position at SpaceX and do not have nearly enough time to smooth the rough edges of this project. I am releasing this fully open source to ensure that individuals, universities, and even companys have access to high performance robotics at an affordable cost. I welcome all contributors and am seeking people to help keep this project alive and prospering! If you are interested- please email me directly and we can set up a time to chat. Alternatively, if you are feeling ambitious, the best way to understand this project is to just dive in. Read the code, view the CAD, and gain a deep understanding of what I have set up so far. I believe the best engineers are made by working through the tough projects that have minimal documentation. While I would prefer to provide step-by-step manufacturing, assembly, and software documentation and tutorials- I just don't have the time right now to provide this. I apologize for this but I believe with the right support, this project can take off and help eager students and engineers around the world a great starting point for creating something awesome!

## Known bugs at release (IMPORTANT)-
Please note that this project is not free of bugs and ignoring some bugs or blindly running the software provided with this project can lead to catastrophic failures. The actuators used in this project are POWERFUL. Please proceed with extreme care and DO NOT ASSUME that everything will work out of the box.

Here is a list of known bugs at release:
- moveToJointPositon() in C++ controls (robot_interface.cpp)
    - DOES NOT PROPERLY HANDLE INTERUPPTING ACTIVE TRAJECTORIES. A trajectory currently must finish before calling moveToJointPosition again. Alternatively, you can call setHoldPosition to stop the trajectory and then move again. 


## Not implemented features yet
- Forward or Inverse Kinematics
- Cartesian Space Trajectory controls
- Dynamics Controls
- Open endpoints for trajectory streaming
- ROS2 Project
- Full URDF implementation - A partial implementation exists for the web visualization though. See the web/public/models directory

## Contributing
We welcome contributions! If you are interested in contributing to this project please contact me directly or open a pull request! I am currently working with Rover Robotics (https://roverrobotics.com/) to help continue supporting this project.

## License
This project uses a slightly modified MIT license. The only modifications go to include ALL files included in this repository including CAD, BOM, Documentation, etc instead of just software.

## Support
- Create an issue in the repository
- Join our community forum (link to be added)

## Acknowledgments
- Created by Jack Rivera as a passion project during my final semester at Purdue University. Shoutout Professor Hyun, Professor Sundaram, and Professor Pare for inspiring me and supporting throughout the development.

- Special thanks to...
- Inspired by...

## Roadmap
- [ ] Add detailed manufacturing instructions
- [ ] Add detailed assembly instructions
- [ ] Add detailed software development instructions
- [ ] Create video tutorials
- [ ] Expand simulation capabilities
- [ ] Implement additional control modes (Kinematics, Dynamics, Cartesian Space, Energy Based, State Space, etc.)
- [ ] Add support for different end effectors 
- [ ] Expand web dashboard
- [ ] Add ROS2 support
