cmake_minimum_required(VERSION 3.16)

project(WildAnimalDetection C)

# ============================================================
# C Standard
# ============================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


# ============================================================
# Dependencies
# ============================================================

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED
    gstreamer-1.0
    gstreamer-video-1.0
)


# ============================================================
# DeepStream
# ============================================================

set(DEEPSTREAM_DIR
    "/opt/nvidia/deepstream/deepstream-9.1"
)


# ============================================================
# Executable
# ============================================================

add_executable(main
    src/main.c
    src/animal_event.c
)


# ============================================================
# Include Directories
# ============================================================

target_include_directories(main PRIVATE

    # Project headers
    ${CMAKE_CURRENT_SOURCE_DIR}/include

    # GStreamer headers
    ${GSTREAMER_INCLUDE_DIRS}

    # DeepStream headers
    ${DEEPSTREAM_DIR}/sources/includes
)


# ============================================================
# Library Directories
# ============================================================

target_link_directories(main PRIVATE

    # GStreamer libraries
    ${GSTREAMER_LIBRARY_DIRS}

    # DeepStream libraries
    ${DEEPSTREAM_DIR}/lib
)


# ============================================================
# Compiler Options
# ============================================================

target_compile_options(main PRIVATE
    ${GSTREAMER_CFLAGS_OTHER}
)


# ============================================================
# Link Libraries
# ============================================================

target_link_libraries(main PRIVATE

    # GStreamer
    ${GSTREAMER_LIBRARIES}

    # DeepStream
    nvdsgst_meta
    nvds_meta
)