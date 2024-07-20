cmake_minimum_required(VERSION 3.29)
project(xvncd_cpp)

set(CMAKE_CXX_STANDARD 20)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else ()
    message(STATUS "Minimal optimization, debug info included")
    add_compile_options(-Og -g)
endif ()

# Check if targeting ARM64 architecture and set appropriate flags
if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -arch arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -arch arm64")
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

add_compile_options($<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libusb REQUIRED libusb-1.0)
find_package(LibUSB)

find_package(LibFTDI1 NO_MODULE REQUIRED)
include(${LIBFTDI_USE_FILE})

include_directories(${PROJECT_SOURCE_DIR}/../include ${LIBUSB_1_INCLUDE_DIRS})

set(SOURCES
        misc.h
        usb.cpp
        application.cpp
        xvncd.cpp
        misc.cpp
        server.cpp
        FTDI.cpp)

add_executable(xvcnd_cpp ${SOURCES})

target_link_libraries(xvcnd_cpp ${LIBFTDI_LIBRARIES} LibUSB::LibUSB)
