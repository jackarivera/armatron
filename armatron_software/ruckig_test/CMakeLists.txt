cmake_minimum_required(VERSION 3.10)
project(ruckig_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Ruckig library
find_package(ruckig REQUIRED)

# Include directories
include_directories(include)

# Add executable
add_executable(ruckig_test src/main.cpp)

# Link libraries
target_link_libraries(ruckig_test ruckig::ruckig) 