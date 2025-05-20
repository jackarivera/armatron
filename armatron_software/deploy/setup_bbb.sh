#!/bin/bash

# Exit on error
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get the SSH password from environment
SSH_PASSWORD="$SSH_PASSWORD"
if [ -z "$SSH_PASSWORD" ]; then
    echo -e "${RED}Error: SSH_PASSWORD environment variable not set${NC}"
    exit 1
fi

echo -e "${BLUE}Setting up BeagleBone Black environment...${NC}"

# Update package lists only (skip full upgrade which takes too long)
echo -e "${BLUE}Updating package lists...${NC}"
echo "$SSH_PASSWORD" | sudo -S apt-get update

# Install required packages if not already installed
echo -e "${BLUE}Installing required packages (if not already installed)...${NC}"
for pkg in nodejs npm build-essential cmake git python3 python3-pip; do
    if ! dpkg -l | grep -q "ii  $pkg "; then
        echo -e "${YELLOW}Installing $pkg...${NC}"
        echo "$SSH_PASSWORD" | sudo -S apt-get install -y $pkg
    else
        echo -e "${GREEN}$pkg is already installed.${NC}"
    fi
done

# Set up Ruckig library
echo -e "${BLUE}Setting up Ruckig library...${NC}"
if [ ! -f "/usr/lib/libruckig.so" ]; then
    echo -e "${YELLOW}Ruckig library not found, installing...${NC}"
    echo "$SSH_PASSWORD" | sudo -S cp /home/debian/armatron_software/libruckig.so /usr/lib/
    echo "$SSH_PASSWORD" | sudo -S chmod 644 /usr/lib/libruckig.so
    echo "$SSH_PASSWORD" | sudo -S ldconfig
else
    echo -e "${GREEN}Ruckig library already installed.${NC}"
fi

# Create socket directory
echo -e "${BLUE}Creating socket directory...${NC}"
mkdir -p /home/debian/.armatron
rm -f /home/debian/.armatron/robot_socket  # Remove existing socket if present
chmod 777 /home/debian/.armatron  # Ensure directory is fully accessible

# Set up permissions
echo -e "${BLUE}Setting up permissions...${NC}"
echo "$SSH_PASSWORD" | sudo -S chown -R debian:debian /home/debian/armatron_software
echo "$SSH_PASSWORD" | sudo -S chown -R debian:debian /home/debian/.armatron

# Create necessary symbolic links if needed
echo -e "${BLUE}Setting up any necessary symbolic links...${NC}"
# Add any symlinks here if needed

echo -e "${GREEN}BeagleBone Black setup complete!${NC}"
echo -e "${YELLOW}Note: Service setup is handled by the main deployment script${NC}" 