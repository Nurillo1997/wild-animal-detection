cmake_minimum_required(VERSION 3.16)

project(WildAnimalDetection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find pkg-config
find_package(PkgConfig REQUIRED)

# Find GStreamer dependencies
pkg_check_modules(GSTREAMER REQUIRED
    gstreamer-1.0
    gstreamer-video-1.0
)

# DeepStream installation directory
set(DEEPSTREAM_DIR "/opt/nvidia/deepstream/deepstream-9.1")

# Create executable
add_executable(main
    src/main.c
)

# Include directories
target_include_directories(main PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${DEEPSTREAM_DIR}/sources/includes
)

# Library directories
target_link_directories(main PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
    ${DEEPSTREAM_DIR}/lib
)

# Compiler options
target_compile_options(main PRIVATE
    ${GSTREAMER_CFLAGS_OTHER}
)

# Link libraries
target_link_libraries(main PRIVATE
    ${GSTREAMER_LIBRARIES}
    nvdsgst_meta
    nvds_meta
)