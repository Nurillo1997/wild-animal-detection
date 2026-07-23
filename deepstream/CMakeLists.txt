cmake_minimum_required(VERSION 3.16)
project(WildAnimalDetection C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0)
find_package(CURL REQUIRED)

set(DEEPSTREAM_DIR "/opt/nvidia/deepstream/deepstream-9.1")

add_executable(main
    src/main.c
    src/animal_event.c
)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GSTREAMER_INCLUDE_DIRS}
    ${DEEPSTREAM_DIR}/sources/includes
    ${CURL_INCLUDE_DIRS}
)

target_link_directories(main PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
    ${DEEPSTREAM_DIR}/lib
)

target_compile_options(main PRIVATE ${GSTREAMER_CFLAGS_OTHER})

target_link_libraries(main PRIVATE
    ${GSTREAMER_LIBRARIES}
    nvdsgst_meta
    nvds_meta
    ${CURL_LIBRARIES}
)