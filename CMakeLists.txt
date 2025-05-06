cmake_minimum_required(VERSION 3.10)
project(capsule_comm)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Set include directories
include_directories(${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/examples
)

# Add TinyFrame sources
set(TINYFRAME_SOURCES
    TinyFrame.c
)

# Add executable for main application
add_executable(capsule_app
    ${TINYFRAME_SOURCES}
    examples/main.cpp
    examples/capsule.cpp
    examples/transceiver.cpp
    examples/tf_transport.cpp
)

# Link with required libraries
target_link_libraries(capsule_app
    PRIVATE
    rt              # POSIX real-time extensions (message queues)
    Threads::Threads # pthreads
)

# Add simulator executable
add_executable(simulator
    examples/simulator.cpp
)

# Link simulator with required libraries
target_link_libraries(simulator
    PRIVATE
    rt              # POSIX real-time extensions (message queues)
    Threads::Threads # pthreads
)

# Install targets
install(TARGETS capsule_app simulator
    RUNTIME DESTINATION bin
)

# Create a 'run' target for convenience
add_custom_target(run
    COMMAND capsule_app
    DEPENDS capsule_app
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

# Create a 'run_simulator' target
add_custom_target(run_simulator
    COMMAND simulator
    DEPENDS simulator
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

# Output helpful information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")