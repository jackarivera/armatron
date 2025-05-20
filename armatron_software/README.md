# Armatron 7-DOF Robot Arm Software

## Project Overview
Armatron is a 7-degree-of-freedom robotic arm project, featuring a web-based control interface and real-time control system. The robot is controlled via a BeagleBone Black (BBB) running Debian Linux, providing a robust and flexible platform for robotics development.

### Key Features
- 7-DOF robotic arm control
- Real-time control daemon
- Web-based user interface
- Cross-compilation support for ARM architecture
- Automated deployment system
- Systemd service management

## Project Structure
```
armatron_software/
├── controls/                 # C++ control system
│   ├── src/                 # Source files
│   ├── include/             # Header files
│   └── build/               # Build directory
├── web/                     # Web interface
│   ├── public/              # Static files
│   ├── src/                 # Source files
│   └── node_modules/        # Node.js dependencies
├── deploy/                  # Deployment files
├── setup_dev.sh            # Development environment setup
├── deploy.sh               # Deployment script
└── toolchain.cmake         # CMake toolchain configuration
```

## Development Environment Setup

### Prerequisites
- WSL2 with Ubuntu (recommended)
- Python 3.x
- Node.js
- Git
- CMake
- Cross-compilation toolchain

### Initial Setup
1. Clone the repository:
```bash
git clone <repository-url>
cd armatron_software
```

2. Run the setup script:
```bash
./setup_dev.sh
```

This will:
- Install required packages
- Set up cross-compilation toolchain
- Create Python virtual environment
- Generate deployment scripts
- Configure CMake toolchain

### Configuration
Edit the following files to set your BBB's IP and credentials:
1. `setup_dev.sh`
2. `deploy.sh`

## Deployment

### Regular Deployment
For normal development and updates:
```bash
sudo ./deploy.sh
```

This will:
- Build the project
- Deploy files to BBB
- Preserve node_modules
- Restart services

### Initial Setup on BBB
For first-time setup or when BBB environment needs reinitialization:
```bash
sudo ./deploy.sh --setup
```

This will:
- Run all regular deployment steps
- Execute setup_bbb.sh on the BBB
- Install system dependencies

### Node Modules Management
Node modules are automatically checked and only installed if missing or invalid. No need to manually manage them.

## Service Management
The system uses two systemd services:
- `armatron-control`: Real-time control daemon
- `armatron-web`: Web interface server

Services are automatically managed during deployment.

## Development Workflow
1. Make changes to source code
2. Run `sudo ./deploy.sh`
3. Changes are automatically deployed and services are restarted

## Troubleshooting
- If services fail to start, check the logs:
  ```bash
  sudo journalctl -u armatron-control
  sudo journalctl -u armatron-web
  ```
- If node modules are missing, the deployment script will automatically install them
- For permission issues, ensure you're running the deploy script with sudo

## Notes
- The deployment script preserves node_modules to speed up deployments
- Sudo password is automatically handled for BBB commands
- The web interface communicates with the control daemon via Unix domain socket
- Cross-compilation is configured for ARM architecture
