cmake_minimum_required(VERSION 3.12)
project(freeticks-cpp)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../toolbox-cpp/cmake)

set(CMAKE_C_STANDARD 11)
# Emits -std=gnu++1z on OSX which is not working as expected.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)


include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/include"
  "/usr/local/include"
)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)


add_subdirectory(../toolbox-cpp toolbox-cpp EXCLUDE_FROM_ALL)
include_directories(../toolbox-cpp)
set(tb_core_LIBRARY tb-core-static)
set(tb_bm_LIBRARY tb-bm-static)


message(STATUS "Build architecture: ${TOOLBOX_BUILD_ARCH}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build version: ${TOOLBOX_VERSION}")

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
  add_definitions(-DTOOLBOX_BUILD_DEBUG=1)
endif()

find_package(Boost 1.67 REQUIRED COMPONENTS unit_test_framework)
if(NOT "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}" MATCHES "\\.a$")
  add_definitions(-DBOOST_TEST_DYN_LINK)
endif()
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

find_package(PCAP)

add_subdirectory(ft)
add_subdirectory(mdconv)



