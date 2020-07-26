cmake_minimum_required(VERSION 3.12)
project(freeticks-cpp)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
# Emits -std=gnu++1z on OSX which is not working as expected.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)


include_directories(
  "${PROJECT_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/include"
  "/usr/local/include"
)

add_subdirectory(../toolbox-cpp toolbox-cpp EXCLUDE_FROM_ALL)
include_directories(../toolbox-cpp)
set(tb_core_LIBRARY tb-core-static)

find_package(Boost 1.67 REQUIRED COMPONENTS unit_test_framework)
if(NOT "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY_RELEASE}" MATCHES "\\.a$")
  add_definitions(-DBOOST_TEST_DYN_LINK)
endif()
message(STATUS "Boost_INCLUDE_DIR=${Boost_INCLUDE_DIR}")
include_directories(SYSTEM ${Boost_INCLUDE_DIR})

add_subdirectory(ft)
add_subdirectory(mdconv)


