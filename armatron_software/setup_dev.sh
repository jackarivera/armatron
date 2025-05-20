#!/bin/bash

# Exit on error
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BBB_IP="192.168.0.189"  # Change this to your BBB's IP
BBB_USER="debian"      # Change this to your BBB's username
BBB_PASSWORD="XXXXXXXXXX"  # Change this to your BBB's password
TOOLCHAIN_DIR="/opt/arm-toolchain"
PROJECT_NAME="armatron_software"
VENV_DIR=".venv"

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if a directory exists
dir_exists() {
    [ -d "$1" ]
}

# Function to check if a file exists
file_exists() {
    [ -f "$1" ]
}

# Create necessary directories
echo -e "${BLUE}Creating necessary directories...${NC}"
mkdir -p $TOOLCHAIN_DIR
mkdir -p deploy
mkdir -p deploy/systemd  # Create systemd directory for service files

# Create systemd service files
echo -e "${BLUE}Creating systemd service files...${NC}"
cat > deploy/systemd/armatron-control.service << 'EOL'
[Unit]
Description=Armatron Real-time Control Daemon
After=network.target

[Service]
Type=simple
User=debian
ExecStart=/home/debian/armatron_software/real_time_daemon
Restart=on-failure
RestartSec=5
WorkingDirectory=/home/debian/armatron_software

[Install]
WantedBy=multi-user.target
EOL

cat > deploy/systemd/armatron-web.service << 'EOL'
[Unit]
Description=Armatron Web Server
After=network.target armatron-control.service
Requires=armatron-control.service

[Service]
Type=simple
User=debian
ExecStart=/usr/bin/node /home/debian/armatron_software/web/server.js
Restart=on-failure
RestartSec=5
WorkingDirectory=/home/debian/armatron_software/web

[Install]
WantedBy=multi-user.target
EOL

# Verify service files were created
if [ ! -f "deploy/systemd/armatron-control.service" ] || [ ! -f "deploy/systemd/armatron-web.service" ]; then
    echo -e "${RED}Error: Failed to create systemd service files${NC}"
    exit 1
fi

# Install required packages
echo -e "${BLUE}Installing required packages...${NC}"
sudo apt-get update

# First install basic build tools
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    python3-venv \
    sshpass \
    rsync \
    nodejs \
    npm

# Install cross-compilation toolchain if not already installed
if ! command_exists arm-linux-gnueabihf-gcc; then
    echo -e "${BLUE}Installing ARM cross-compilation toolchain...${NC}"
    sudo apt-get install -y \
        gcc-arm-linux-gnueabihf \
        g++-arm-linux-gnueabihf \
        binutils-arm-linux-gnueabihf
else
    echo -e "${GREEN}ARM cross-compilation toolchain already installed.${NC}"
fi

# Install development libraries for both host and target
echo -e "${BLUE}Installing development libraries...${NC}"
sudo apt-get install -y \
    pkg-config \
    libudev-dev \
    libusb-1.0-0-dev \
    libjsoncpp-dev

# Enable multiarch support
if ! dpkg --print-foreign-architectures | grep -q armhf; then
    echo -e "${BLUE}Enabling multiarch support...${NC}"
    sudo dpkg --add-architecture armhf
    sudo apt-get update
fi

# Install ARM-specific development libraries
echo -e "${BLUE}Installing ARM-specific libraries...${NC}"
sudo apt-get install -y \
    libudev-dev:armhf \
    libusb-1.0-0-dev:armhf \
    libjsoncpp-dev:armhf

# Build and install Ruckig library for ARM
echo -e "${BLUE}Building Ruckig library for ARM...${NC}"
if [ ! -d "controls/dependencies/ruckig/build" ]; then
    mkdir -p controls/dependencies/ruckig/build
    cd controls/dependencies/ruckig/build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../../../../toolchain.cmake -DCMAKE_INSTALL_PREFIX=/usr/arm-linux-gnueabihf
    make -j$(nproc)
    sudo make install
    cd ../../../..
fi

# Set up Python virtual environment
echo -e "${BLUE}Setting up Python virtual environment...${NC}"
if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv $VENV_DIR
fi

# Activate virtual environment and install packages
echo -e "${BLUE}Installing Python packages in virtual environment...${NC}"
source $VENV_DIR/bin/activate
pip install --upgrade pip
pip install paramiko

# Create deployment script
cat > deploy.sh << 'EOL'
#!/bin/bash

# Exit on error
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BBB_IP="192.168.0.189"  # Change this to your BBB's IP
BBB_USER="debian"      # Change this to your BBB's username
BBB_PASSWORD="4kmcdyzp"  # Change this to your BBB's password
PROJECT_NAME="armatron_software"
VENV_DIR=".venv"
BBB_PROJECT_DIR="/home/$BBB_USER/$PROJECT_NAME"
BBB_WEB_DIR="$BBB_PROJECT_DIR/web"
SOCKET_DIR="/home/$BBB_USER/.armatron"
SSH_CONFIG_DIR="$HOME/.ssh/armatron"
TIMESTAMP_FILE=".last_build_timestamp"

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if a directory exists
dir_exists() {
    [ -d "$1" ]
}

# Function to check if a file exists
file_exists() {
    [ -f "$1" ]
}

# Setup SSH connection multiplexing for faster commands
setup_ssh_multiplexing() {
    mkdir -p "$SSH_CONFIG_DIR"
    cat > "$SSH_CONFIG_DIR/config" << EOF
Host $BBB_IP
    ControlMaster auto
    ControlPath $SSH_CONFIG_DIR/control-%r@%h:%p
    ControlPersist 10m
    ServerAliveInterval 30
    ConnectTimeout 10
    StrictHostKeyChecking no
EOF
}

# Function to execute command on BBB with error handling
execute_on_bbb() {
    local cmd="$1"
    local error_msg="${2:-Failed to execute command on BBB}"
    local max_attempts=3
    local attempt=1
    
    while [ $attempt -le $max_attempts ]; do
        if command_exists sshpass; then
            if sshpass -p "$BBB_PASSWORD" ssh -F "$SSH_CONFIG_DIR/config" $BBB_USER@$BBB_IP "$cmd" 2>/dev/null; then
                return 0
            fi
        else
            if ssh -F "$SSH_CONFIG_DIR/config" $BBB_USER@$BBB_IP "$cmd" 2>/dev/null; then
                return 0
            fi
        fi
        
        echo -e "${YELLOW}Attempt $attempt/$max_attempts: $error_msg. Retrying...${NC}"
        attempt=$((attempt+1))
        sleep 2
    done
    
    echo -e "${RED}Error: $error_msg after $max_attempts attempts${NC}"
    echo "Command: $cmd"
    return 1
}

# Function to execute sudo command on BBB with error handling
execute_sudo_on_bbb() {
    local cmd="$1"
    local error_msg="${2:-Failed to execute sudo command on BBB}"
    local max_attempts=3
    local attempt=1
    
    while [ $attempt -le $max_attempts ]; do
        if command_exists sshpass; then
            if echo "$BBB_PASSWORD" | sshpass -p "$BBB_PASSWORD" ssh -F "$SSH_CONFIG_DIR/config" $BBB_USER@$BBB_IP "sudo -S $cmd" 2>/dev/null; then
                return 0
            fi
        else
            if echo "$BBB_PASSWORD" | ssh -F "$SSH_CONFIG_DIR/config" $BBB_USER@$BBB_IP "sudo -S $cmd" 2>/dev/null; then
                return 0
            fi
        fi
        
        echo -e "${YELLOW}Attempt $attempt/$max_attempts: $error_msg. Retrying...${NC}"
        attempt=$((attempt+1))
        sleep 2
    done
    
    echo -e "${RED}Error: $error_msg after $max_attempts attempts${NC}"
    echo "Command: $cmd"
    return 1
}

# Function to check if node modules need to be installed
check_node_modules() {
    echo -e "${BLUE}Checking if node modules need to be updated on BBB...${NC}"
    
    # Generate checksum of package.json
    local local_checksum=$(md5sum deploy/web/package.json | awk '{ print $1 }')
    
    # Check if remote package.json exists and compare checksums
    if execute_on_bbb "[ -f '$BBB_WEB_DIR/package.json' ] && [ -d '$BBB_WEB_DIR/node_modules' ] && echo '$local_checksum' | cmp -s - <(md5sum '$BBB_WEB_DIR/package.json' | awk '{ print \$1 }')" "Checking package.json"; then
        echo -e "${GREEN}Node modules are up to date on BBB${NC}"
        return 0
    else
        echo -e "${YELLOW}Node modules need to be installed or updated on BBB${NC}"
        return 1
    fi
}

# Function to install dependencies
install_dependencies() {
    echo -e "${BLUE}Installing production dependencies on BBB...${NC}"
    if ! execute_on_bbb "cd $BBB_WEB_DIR && npm ci --production" "Failed to install production dependencies"; then
        echo -e "${YELLOW}npm ci failed, falling back to npm install...${NC}"
        if ! execute_on_bbb "cd $BBB_WEB_DIR && npm install --production" "Failed to install production dependencies"; then
            echo -e "${RED}Error: Failed to install production dependencies${NC}"
            return 1
        fi
    fi
    echo -e "${GREEN}Production dependencies installed successfully${NC}"
}

# Check if a full build is needed
need_full_build() {
    local force=$1
    
    if [ "$force" = true ]; then
        echo -e "${YELLOW}Force rebuild requested${NC}"
        return 0
    fi
    
    # Check timestamp of last build
    if file_exists "$TIMESTAMP_FILE"; then
        local last_build=$(cat "$TIMESTAMP_FILE")
        local control_changed=$(find controls -type f -name "*.cpp" -o -name "*.h" -o -name "CMakeLists.txt" -newer "$TIMESTAMP_FILE" | wc -l)
        
        if [ $control_changed -eq 0 ]; then
            echo -e "${GREEN}No changes detected in C++ code since last build${NC}"
            return 1
        else
            echo -e "${YELLOW}Changes detected in C++ code, rebuild needed${NC}"
            return 0
        fi
    fi
    
    # No timestamp file, full build needed
    echo -e "${YELLOW}No previous build timestamp found, full build needed${NC}"
    return 0
}

# Function to check if web files need to be built
need_web_build() {
    local force=$1
    
    if [ "$force" = true ]; then
        echo -e "${YELLOW}Force web rebuild requested${NC}"
        return 0
    fi
    
    # Check timestamp of last build
    if file_exists "$TIMESTAMP_FILE"; then
        local web_changed=$(find web/src -type f -newer "$TIMESTAMP_FILE" 2>/dev/null | wc -l)
        
        if [ $web_changed -eq 0 ]; then
            echo -e "${GREEN}No changes detected in web code since last build${NC}"
            return 1
        else
            echo -e "${YELLOW}Changes detected in web code, rebuild needed${NC}"
            return 0
        fi
    fi
    
    # No timestamp file, full build needed
    echo -e "${YELLOW}No previous web build timestamp found, rebuild needed${NC}"
    return 0
}

# Function to rsync files to BBB with optimized settings
rsync_to_bbb() {
    local src="$1"
    local dest="$2"
    local error_msg="${3:-Failed to rsync files to BBB}"
    local exclude_opts="${4:-}"
    
    # Create parent directories on BBB first
    local parent_dir=$(dirname "$dest")
    execute_on_bbb "mkdir -p $parent_dir" "Failed to create directory structure on BBB"
    
    # Always exclude node_modules unless explicitly included
    if [[ ! "$exclude_opts" =~ "--include=node_modules" ]]; then
        exclude_opts="$exclude_opts --exclude=node_modules"
    fi
    
    if command_exists sshpass; then
        if ! sshpass -p "$BBB_PASSWORD" rsync -avz --checksum --compress $exclude_opts \
            "$src" "$BBB_USER@$BBB_IP:$dest" || [ "$?" = "23" -a "$exclude_opts" != "" ]; then
            # Exit code 23 means partial transfer, which is acceptable when excluding files
            if [ "$?" = "23" -a "$exclude_opts" != "" ]; then
                echo -e "${YELLOW}Warning: Some files were not transferred (excluded patterns)${NC}"
                return 0
            else
                echo -e "${RED}Error: $error_msg${NC}"
                return 1
            fi
        fi
    else
        if ! rsync -avz --checksum --compress $exclude_opts \
            "$src" "$BBB_USER@$BBB_IP:$dest" || [ "$?" = "23" -a "$exclude_opts" != "" ]; then
            # Exit code 23 means partial transfer, which is acceptable when excluding files
            if [ "$?" = "23" -a "$exclude_opts" != "" ]; then
                echo -e "${YELLOW}Warning: Some files were not transferred (excluded patterns)${NC}"
                return 0
            else
                echo -e "${RED}Error: $error_msg${NC}"
                return 1
            fi
        fi
    fi
    return 0
}

# Function to check if a service is running
is_service_running() {
    local service="$1"
    if execute_on_bbb "systemctl is-active $service" "Failed to check service status" >/dev/null; then
        return 0
    else
        return 1
    fi
}

# Parse command line arguments
SETUP_MODE=false
INSTALL_DEPS=false
FORCE_BUILD=false
WEB_ONLY=false
CONTROL_ONLY=false
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --setup) SETUP_MODE=true ;;
        --install-deps) INSTALL_DEPS=true ;;
        --force-build) FORCE_BUILD=true ;;
        --web-only) WEB_ONLY=true ;;
        --control-only) CONTROL_ONLY=true ;;
        --help) 
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --setup         Run initial setup on BBB (first-time setup)"
            echo "  --install-deps  Force reinstall of node dependencies"
            echo "  --force-build   Force rebuild of all components"
            echo "  --web-only      Deploy only web components"
            echo "  --control-only  Deploy only control daemon"
            echo "  --help          Show this help message"
            echo ""
            echo "Deployment Features:"
            echo "  - Incremental builds (only rebuilds when files change)"
            echo "  - SSH connection multiplexing for faster remote commands"
            echo "  - Parallel builds for C++ and web components"
            echo "  - Optimized file transfers with compression and checksums"
            echo "  - Smart dependency management"
            echo "  - Improved error handling and retry logic"
            exit 0
            ;;
        *) echo -e "${RED}Unknown parameter: $1${NC}"; exit 1 ;;
    esac
    shift
done

# Validate flags
if [ "$WEB_ONLY" = true ] && [ "$CONTROL_ONLY" = true ]; then
    echo -e "${RED}Error: Cannot use --web-only and --control-only together${NC}"
    exit 1
fi

start_time=$(date +%s)

# Setup SSH multiplexing
setup_ssh_multiplexing

# Test SSH connection first
echo -e "${BLUE}Testing SSH connection to BBB...${NC}"
if ! execute_on_bbb "echo 'SSH connection successful'" "Failed to connect to BBB"; then
    echo -e "${RED}Error: Failed to connect to BBB. Please check your network connection and BBB credentials.${NC}"
    exit 1
fi

# Activate virtual environment
source $VENV_DIR/bin/activate

# Build components in parallel if needed
if [ "$CONTROL_ONLY" = false ] || [ "$WEB_ONLY" = false ]; then
    # Use a flag to track build success
    build_success=true
    
    # Build C++ components if needed
    if [ "$WEB_ONLY" = false ] && (need_full_build "$FORCE_BUILD"); then
        echo -e "${BLUE}Building C++ project...${NC}"
        if ! dir_exists "controls/build"; then
            mkdir -p controls/build
        fi
        (
            cd controls/build
            if ! cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchain.cmake ..; then
                echo -e "${RED}Error: CMake configuration failed${NC}"
                exit 1
            fi
            if ! make -j$(nproc); then
                echo -e "${RED}Error: Make failed${NC}"
                exit 1
            fi
        ) &
        cpp_build_pid=$!
    fi
    
    # Build web components if needed
    if [ "$CONTROL_ONLY" = false ] && (need_web_build "$FORCE_BUILD"); then
        echo -e "${BLUE}Building web application...${NC}"
        (
            cd web
            if ! npm install; then
                echo -e "${RED}Error: Failed to install web dependencies${NC}"
                exit 1
            fi
            if ! npm run build; then
                echo -e "${RED}Error: Failed to build web application${NC}"
                exit 1
            fi
        ) &
        web_build_pid=$!
    fi
    
    # Wait for background builds if they were started
    if [ -n "${cpp_build_pid+x}" ]; then
        echo -e "${BLUE}Waiting for C++ build to complete...${NC}"
        if ! wait $cpp_build_pid; then
            echo -e "${RED}Error: C++ build failed${NC}"
            build_success=false
        fi
    fi
    
    if [ -n "${web_build_pid+x}" ]; then
        echo -e "${BLUE}Waiting for web build to complete...${NC}"
        if ! wait $web_build_pid; then
            echo -e "${RED}Error: Web build failed${NC}"
            build_success=false
        fi
    fi
    
    # Exit if any build failed
    if [ "$build_success" = false ]; then
        echo -e "${RED}Error: Build failed, stopping deployment${NC}"
        exit 1
    fi
fi

# Create deployment package
echo -e "${BLUE}Creating deployment package...${NC}"
if ! dir_exists "deploy"; then
    mkdir -p deploy
fi

# Ensure systemd directory exists
mkdir -p deploy/systemd

# Copy binary with correct name if control deployment is requested
if [ "$WEB_ONLY" = false ] && ([ "$FORCE_BUILD" = true ] || need_full_build false || ! file_exists "deploy/real_time_daemon"); then
    echo -e "${BLUE}Preparing control daemon for deployment...${NC}"
    if file_exists "controls/build/bin/realtime_daemon"; then
        cp controls/build/bin/realtime_daemon deploy/real_time_daemon
        # Copy Ruckig library
        cp /usr/arm-linux-gnueabihf/lib/libruckig.so deploy/
    else
        echo -e "${RED}Error: Binary not found at controls/build/bin/realtime_daemon${NC}"
        exit 1
    fi
fi

# Handle web server deployment if requested
if [ "$CONTROL_ONLY" = false ]; then
    echo -e "${BLUE}Preparing web server deployment...${NC}"
    if ! dir_exists "web"; then
        echo -e "${RED}Error: web directory not found${NC}"
        exit 1
    fi
    
    # Create web directory in deploy if it doesn't exist
    mkdir -p deploy/web
    
    # Copy only the built files and necessary server files
    echo -e "${BLUE}Copying web files...${NC}"
    rsync -av --checksum \
        --exclude='node_modules' \
        --exclude='.git' \
        --exclude='.env' \
        --exclude='*.log' \
        --exclude='npm-debug.log*' \
        --exclude='src' \
        --exclude='public' \
        web/dist/ deploy/web/dist/
    
    cp web/server.js deploy/web/
    cp web/package.json deploy/web/
    cp web/package-lock.json deploy/web/
fi

# Create timestamp file for incremental builds
date +%s > "$TIMESTAMP_FILE"

# Create necessary directories on BBB
echo -e "${BLUE}Setting up directories on BBB...${NC}"
if ! execute_on_bbb "mkdir -p $BBB_PROJECT_DIR $BBB_WEB_DIR $SOCKET_DIR" "Failed to create directories on BBB"; then
    echo -e "${RED}Error: Failed to create directories on BBB${NC}"
    exit 1
fi

# Deploy files based on deployment mode
deploy_exclusions="--exclude='.git' --exclude='.env' --exclude='*.log' --exclude='npm-debug.log*'"

if [ "$CONTROL_ONLY" = true ]; then
    echo -e "${BLUE}Deploying control daemon to BBB...${NC}"
    rsync_to_bbb "deploy/real_time_daemon" "$BBB_PROJECT_DIR/" "Failed to deploy control daemon to BBB"
    rsync_to_bbb "deploy/systemd/armatron-control.service" "/tmp/" "Failed to copy control service file to BBB"
elif [ "$WEB_ONLY" = true ]; then
    echo -e "${BLUE}Deploying web components to BBB...${NC}"
    # For web-only deployment, only sync the dist directory and server files
    rsync_to_bbb "deploy/web/dist/" "$BBB_WEB_DIR/dist/" "Failed to deploy web dist files to BBB" "$deploy_exclusions"
    rsync_to_bbb "deploy/web/server.js" "$BBB_WEB_DIR/" "Failed to deploy web server file to BBB" "$deploy_exclusions"
    rsync_to_bbb "deploy/web/package.json" "$BBB_WEB_DIR/" "Failed to deploy package.json to BBB" "$deploy_exclusions"
    rsync_to_bbb "deploy/systemd/armatron-web.service" "/tmp/" "Failed to copy web service file to BBB"
else
    echo -e "${BLUE}Deploying all components to BBB...${NC}"
    rsync_to_bbb "deploy/" "$BBB_PROJECT_DIR/" "Failed to deploy files to BBB" "$deploy_exclusions"
    # Copy each service file individually
    rsync_to_bbb "deploy/systemd/armatron-control.service" "/tmp/" "Failed to copy control service file to BBB"
    rsync_to_bbb "deploy/systemd/armatron-web.service" "/tmp/" "Failed to copy web service file to BBB"
fi

# Move service files to systemd directory
if [ "$CONTROL_ONLY" = true ]; then
    if ! execute_sudo_on_bbb "mv /tmp/armatron-control.service /etc/systemd/system/" "Failed to move control service file"; then
        echo -e "${RED}Error: Failed to move control service file to systemd directory${NC}"
        exit 1
    fi
elif [ "$WEB_ONLY" = true ]; then
    if ! execute_sudo_on_bbb "mv /tmp/armatron-web.service /etc/systemd/system/" "Failed to move web service file"; then
        echo -e "${RED}Error: Failed to move web service file to systemd directory${NC}"
        exit 1
    fi
else
    # Move each service file individually
    if ! execute_sudo_on_bbb "mv /tmp/armatron-control.service /etc/systemd/system/" "Failed to move control service file"; then
        echo -e "${RED}Error: Failed to move control service file to systemd directory${NC}"
        exit 1
    fi
    if ! execute_sudo_on_bbb "mv /tmp/armatron-web.service /etc/systemd/system/" "Failed to move web service file"; then
        echo -e "${RED}Error: Failed to move web service file to systemd directory${NC}"
        exit 1
    fi
fi

# Set proper permissions for the binary
if [ "$WEB_ONLY" = false ]; then
    echo -e "${BLUE}Setting binary permissions...${NC}"
    if ! execute_on_bbb "chmod +x $BBB_PROJECT_DIR/real_time_daemon" "Failed to set binary permissions"; then
        echo -e "${RED}Error: Failed to set binary permissions${NC}"
        exit 1
    fi
fi

# Run setup script only if --setup flag is provided
if [ "$SETUP_MODE" = true ]; then
    echo -e "${BLUE}Running setup script...${NC}"
    if file_exists "deploy/setup_bbb.sh"; then
        # Copy the setup script to BBB
        rsync_to_bbb "deploy/setup_bbb.sh" "$BBB_PROJECT_DIR/" "Failed to copy setup script to BBB"
        
        # Set executable permission and run with extended timeout
        echo -e "${BLUE}Setting executable permission on setup script...${NC}"
        if ! execute_on_bbb "chmod +x $BBB_PROJECT_DIR/setup_bbb.sh" "Failed to set executable permission on setup script"; then
            echo -e "${RED}Error: Failed to set executable permission on setup script${NC}"
            exit 1
        fi
        
        echo -e "${BLUE}Running setup script on BBB (this may take several minutes)...${NC}"
        # Use a longer timeout for this command since updates can take a while
        if command_exists sshpass; then
            if ! sshpass -p "$BBB_PASSWORD" ssh -F "$SSH_CONFIG_DIR/config" -o ConnectTimeout=30 -o ServerAliveInterval=10 $BBB_USER@$BBB_IP "cd $BBB_PROJECT_DIR && SSH_PASSWORD='$BBB_PASSWORD' ./setup_bbb.sh"; then
                echo -e "${RED}Error: Failed to run setup script${NC}"
                exit 1
            fi
        else
            if ! ssh -F "$SSH_CONFIG_DIR/config" -o ConnectTimeout=30 -o ServerAliveInterval=10 $BBB_USER@$BBB_IP "cd $BBB_PROJECT_DIR && SSH_PASSWORD='$BBB_PASSWORD' ./setup_bbb.sh"; then
                echo -e "${RED}Error: Failed to run setup script${NC}"
                exit 1
            fi
        fi
        echo -e "${GREEN}Setup script executed successfully${NC}"
    else
        echo -e "${YELLOW}Warning: setup_bbb.sh not found in deploy directory, skipping setup${NC}"
    fi
fi

# Check and install production dependencies only if needed
if [ "$CONTROL_ONLY" = false ]; then
    echo -e "${BLUE}Checking and installing web server dependencies...${NC}"
    if [ "$INSTALL_DEPS" = true ] || ! check_node_modules; then
        if ! install_dependencies; then
            echo -e "${RED}Error: Failed to install dependencies${NC}"
            exit 1
        fi
    else
        echo -e "${GREEN}Skipping node module installation - already installed${NC}"
    fi
fi

# Stop services based on deployment mode
echo -e "${BLUE}Managing services...${NC}"
if [ "$CONTROL_ONLY" = true ]; then
    # Only handle control service
    execute_sudo_on_bbb "systemctl stop armatron-control || true" "Failed to stop control service"
elif [ "$WEB_ONLY" = true ]; then
    # Only handle web service
    execute_sudo_on_bbb "systemctl stop armatron-web || true" "Failed to stop web service"
else
    # Handle all services
    execute_sudo_on_bbb "systemctl stop armatron-web armatron-control || true" "Failed to stop services"
fi

# Reload systemd daemon to pick up new service files
echo -e "${BLUE}Reloading systemd daemon...${NC}"
if ! execute_sudo_on_bbb "systemctl daemon-reload" "Failed to reload systemd daemon"; then
    echo -e "${RED}Error: Failed to reload systemd daemon${NC}"
    exit 1
fi

# Clean up any existing socket files if deploying control daemon
if [ "$WEB_ONLY" = false ]; then
    echo -e "${BLUE}Cleaning up socket files...${NC}"
    if ! execute_on_bbb "mkdir -p $SOCKET_DIR && rm -f $SOCKET_DIR/robot_socket && chmod 777 $SOCKET_DIR" "Failed to clean up socket files"; then
        echo -e "${RED}Error: Failed to clean up socket files${NC}"
        exit 1
    fi
fi

# Start services in correct order based on deployment mode
if [ "$CONTROL_ONLY" = true ]; then
    # Only start control service
    echo -e "${BLUE}Managing control service...${NC}"
    if is_service_running "armatron-control"; then
        echo -e "${YELLOW}Control service is already running, restarting...${NC}"
        if ! execute_sudo_on_bbb "systemctl restart armatron-control" "Failed to restart control service"; then
            echo -e "${RED}Error: Failed to restart control service${NC}"
            exit 1
        fi
    else
        echo -e "${BLUE}Starting control service...${NC}"
        if ! execute_sudo_on_bbb "systemctl start armatron-control && systemctl enable armatron-control" "Failed to start control service"; then
            echo -e "${RED}Error: Failed to start control service${NC}"
            exit 1
        fi
    fi
elif [ "$WEB_ONLY" = true ]; then
    # Only start web service (assumes control service is already running)
    echo -e "${BLUE}Managing web service...${NC}"
    if is_service_running "armatron-web"; then
        echo -e "${YELLOW}Web service is already running, restarting...${NC}"
        if ! execute_sudo_on_bbb "systemctl restart armatron-web" "Failed to restart web service"; then
            echo -e "${RED}Error: Failed to restart web service${NC}"
            exit 1
        fi
    else
        echo -e "${BLUE}Starting web service...${NC}"
        if ! execute_sudo_on_bbb "systemctl start armatron-web && systemctl enable armatron-web" "Failed to start web service"; then
            echo -e "${RED}Error: Failed to start web service${NC}"
            exit 1
        fi
    fi
else
    # Start services in correct order
    echo -e "${BLUE}Managing services...${NC}"
    if is_service_running "armatron-control"; then
        echo -e "${YELLOW}Control service is already running, restarting...${NC}"
        if ! execute_sudo_on_bbb "systemctl restart armatron-control" "Failed to restart control service"; then
            echo -e "${RED}Error: Failed to restart control service${NC}"
            exit 1
        fi
    else
        echo -e "${BLUE}Starting control service...${NC}"
        if ! execute_sudo_on_bbb "systemctl start armatron-control && systemctl enable armatron-control" "Failed to start control service"; then
            echo -e "${RED}Error: Failed to start control service${NC}"
            exit 1
        fi
    fi
    
    sleep 2  # Give the control daemon time to create the socket
    
    if is_service_running "armatron-web"; then
        echo -e "${YELLOW}Web service is already running, restarting...${NC}"
        if ! execute_sudo_on_bbb "systemctl restart armatron-web" "Failed to restart web service"; then
            echo -e "${RED}Error: Failed to restart web service${NC}"
            exit 1
        fi
    else
        echo -e "${BLUE}Starting web service...${NC}"
        if ! execute_sudo_on_bbb "systemctl start armatron-web && systemctl enable armatron-web" "Failed to start web service"; then
            echo -e "${RED}Error: Failed to start web service${NC}"
            exit 1
        fi
    fi
fi

# Verify service status
echo -e "${BLUE}Checking service status...${NC}"
if [ "$CONTROL_ONLY" = true ]; then
    execute_sudo_on_bbb "systemctl status armatron-control" "Failed to check control service status"
elif [ "$WEB_ONLY" = true ]; then
    execute_sudo_on_bbb "systemctl status armatron-web" "Failed to check web service status"
else
    execute_sudo_on_bbb "systemctl status armatron-control armatron-web" "Failed to check service status"
fi

# Calculate and display elapsed time
end_time=$(date +%s)
elapsed=$((end_time - start_time))
minutes=$((elapsed / 60))
seconds=$((elapsed % 60))

echo -e "${GREEN}Deployment complete in ${minutes}m ${seconds}s!${NC}"

# Print status message with deployed components
if [ "$CONTROL_ONLY" = true ]; then
    echo -e "${GREEN}Successfully deployed control daemon to BBB.${NC}"
elif [ "$WEB_ONLY" = true ]; then
    echo -e "${GREEN}Successfully deployed web application to BBB.${NC}"
else
    echo -e "${GREEN}Successfully deployed all components to BBB.${NC}"
fi

# Show help information
echo -e "${BLUE}Available deployment options for future use:${NC}"
echo -e "  --web-only     : Deploy only web components"
echo -e "  --control-only : Deploy only control daemon"
echo -e "  --force-build  : Force rebuild of all components"
echo -e "  --install-deps : Force reinstall of node dependencies"
echo -e "  --setup        : Run initial setup on BBB"
EOL

# Create CMake toolchain file
cat > toolchain.cmake << 'EOL'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Set pkg-config path for cross-compilation
set(ENV{PKG_CONFIG_PATH} "/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "/")

# Add compiler flags to suppress warnings
add_compile_options(-Wno-psabi)

# Enable build caching for faster incremental builds
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe")

# Optimize for target architecture
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mfpu=neon -mfloat-abi=hard")
EOL

# Make scripts executable
chmod +x deploy.sh

echo -e "${GREEN}Development environment setup complete!${NC}"
echo "Deployment script has been optimized for faster deployments."
echo "New features added:"
echo "  - Incremental builds (only rebuilds when files change)"
echo "  - SSH connection multiplexing (faster remote commands)"
echo "  - Parallel builds for C++ and web components"
echo "  - Optimized file transfers with compression and checksums"
echo "  - Targeted deployments (--web-only or --control-only)"
echo "  - Smart dependency management"
echo "  - Improved error handling and retry logic"

# Verify installations
echo -e "${BLUE}Verifying installations...${NC}"
if ! command_exists arm-linux-gnueabihf-gcc; then
    echo -e "${YELLOW}Warning: ARM toolchain not found. Please check the installation.${NC}"
fi
if ! command_exists cmake; then
    echo -e "${YELLOW}Warning: CMake not found. Please check the installation.${NC}"
fi
if ! command_exists node; then
    echo -e "${YELLOW}Warning: Node.js not found. Please check the installation.${NC}"
fi
if ! command_exists pkg-config; then
    echo -e "${YELLOW}Warning: pkg-config not found. Please check the installation.${NC}"
fi

echo "Note: The Python virtual environment is located in $VENV_DIR"
echo "To activate it manually, run: source $VENV_DIR/bin/activate"
echo ""
echo "To deploy your project to the BBB, run: ./deploy.sh"
echo "For targeted deployments, use: ./deploy.sh --web-only or ./deploy.sh --control-only"
echo "For first-time setup, use: ./deploy.sh --setup"