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

# Toolbox options.
set(TOOLBOX_BUILD_ARCH   "${REACTIVE_BUILD_ARCH}")
set(TOOLBOX_BUILD_SHARED "${REACTIVE_BUILD_SHARED}")
set(TOOLBOX_VERSION      "${REACTIVE_VERSION}")
set(TOOLBOX_TOOLCHAIN    "${REACTIVE_TOOLCHAIN}")

if (NOT "${TOOLBOX_TOOLCHAIN}" STREQUAL "")
  get_filename_component(TOOLBOX_TOOLCHAIN "${TOOLBOX_TOOLCHAIN}" REALPATH)
  set(CMAKE_PREFIX_PATH "${TOOLBOX_TOOLCHAIN}")
  message(STATUS "TOOLBOX_TOOLCHAIN: ${TOOLBOX_TOOLCHAIN}")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
  add_definitions(-DTOOLBOX_BUILD_DEBUG=1)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${PROJECT_SOURCE_DIR}/etc/cmake")

# Optimise for target architecture.
set(CMAKE_C_FLAGS_RELEASE          "${CMAKE_C_FLAGS_RELEASE} -march=${TOOLBOX_BUILD_ARCH}")
set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE} -march=${TOOLBOX_BUILD_ARCH}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} -march=${TOOLBOX_BUILD_ARCH}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -march=${TOOLBOX_BUILD_ARCH}")
set(CMAKE_C_FLAGS_MINSIZEREL       "${CMAKE_C_FLAGS_MINSIZEREL} -march=${TOOLBOX_BUILD_ARCH}")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL} -march=${TOOLBOX_BUILD_ARCH}")

#set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

# Intended for use with Linux Perf.
set(CMAKE_C_FLAGS_PERF   "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_PERF "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")

# Coverage build
set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")



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
find_package(AERON)

add_subdirectory(ft)
set(ft_core_LIBRARY ft-core-static)
add_subdirectory(mdserv)



