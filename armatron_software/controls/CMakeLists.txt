cmake_minimum_required(VERSION 3.10)
project(armatron_controls)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# DEBUGGING
# Global Debug Flag
#add_definitions(-DDEBUG)

# File-Specific Debug Flags
add_definitions(-DCAN_DEBUG)       # Enable debugging for CANHandler.cpp
add_definitions(-DREALTIME_DEBUG)     # Enable debugging for MotorInterface.cpp

set(CMAKE_CXX_STANDARD 17)

# Include headers
include_directories(include)

# Add external dependencies
set(DEPENDENCIES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies)

# Find Eigen3 first (required by KDL)
find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})

# Simply link to TinyXML2 directly - requires libtinyxml2-dev to be installed
# sudo apt-get install libtinyxml2-dev
set(TINYXML2_LIBRARIES tinyxml2)

# Orocos KDL
set(OROCOS_KDL_DIR ${DEPENDENCIES_DIR}/orocos_kinematics_dynamics)
add_subdirectory(${OROCOS_KDL_DIR}/orocos_kdl)

# Ruckig
set(RUCKIG_DIR ${DEPENDENCIES_DIR}/ruckig)
add_subdirectory(${RUCKIG_DIR})

# Core source files shared by both executables
set(CORE_SOURCES
    src/can_handler.cpp
    src/motor_interface.cpp
    src/robot_interface.cpp
    src/real_time_daemon.cpp
    src/kinematics_interface.cpp
    src/kdl_parser.cpp
)

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# ============ Debug Executable ============

#add_executable(armatron_debug main_debug.cpp ${CORE_SOURCES})

#target_link_libraries(armatron_debug Threads::Threads rt ${JSONCPP_LIBRARIES})

# ============ Real-Time Daemon ============

add_executable(realtime_daemon
    main_realtime.cpp
    ${CORE_SOURCES}
)

target_link_libraries(realtime_daemon 
    Threads::Threads 
    rt 
    ${JSONCPP_LIBRARIES}
    ${TINYXML2_LIBRARIES}
    orocos-kdl
    ruckig
)

# Set capabilities for real-time scheduling
install(TARGETS realtime_daemon
    RUNTIME DESTINATION bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Set capabilities after installation
install(CODE "
    execute_process(COMMAND setcap 'cap_sys_nice=eip' \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/realtime_daemon)
")
