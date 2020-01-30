cmake_minimum_required(VERSION 3.14.0 FATAL_ERROR)

project(dqcsim-cqasm
    VERSION 0.0.1
    DESCRIPTION "DQCsim frontend for cQASM files"
    LANGUAGES CXX
)

# Include DQCsim.
include(FetchContent)
FetchContent_Declare(dqcsim
    GIT_REPOSITORY  https://github.com/mbrobbel/dqcsim.git
    GIT_TAG         master
)
FetchContent_MakeAvailable(dqcsim)

# Include libqasm.
include(cmake/libqasm.cmake)

# Use C++11.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

# Main frontend executable.
add_executable(
    dqcsfecq
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(
    dqcsfecq PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(NOT APPLE)
  target_link_options(dqcsfecq PUBLIC -lrt)
endif()
target_link_libraries(dqcsfecq dqcsim libqasm)
