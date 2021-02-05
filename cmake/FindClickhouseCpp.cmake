# - Try to find libclickhouse-cpp include dirs and libraries
#
# Usage of this module as follows:
#
#     find_package(ClickhouseCpp)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  CLICKHOUSECPP_ROOT_DIR             Set this variable to the root installation of
#                            libclickhouse-cpp if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  CLICKHOUSECPP_FOUND                System has libclickhouse-cpp, include and library dirs found
#  CLICKHOUSECPP_INCLUDE_DIR          The libclickhouse-cpp include directories.
#  CLICKHOUSECPP_LIBRARY              The libclickhouse-cpp library

find_path(CLICKHOUSECPP_ROOT_DIR
    NAMES include/clickhouse/client.h
)

find_path(CLICKHOUSECPP_INCLUDE_DIR
    NAMES clickhouse/client.h
    HINTS ${CLICKHOUSECPP_ROOT_DIR}/include
)

set (HINT_DIR ${CLICKHOUSECPP_ROOT_DIR}/lib)

# On x64 windows, we should look also for the .lib at /lib/x64/
# as this is the default path for the ClickHouse developer's pack
if (${CMAKE_SIZEOF_VOID_P} EQUAL 8 AND WIN32)
    set (HINT_DIR ${CLICKHOUSECPP_ROOT_DIR}/lib/x64/ ${HINT_DIR})
endif ()

find_library(CLICKHOUSECPP_LIBRARY
    NAMES clickhouse-cpp-lib
    HINTS ${HINT_DIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CLICKHOUSECPP DEFAULT_MSG
    CLICKHOUSECPP_LIBRARY
    CLICKHOUSECPP_INCLUDE_DIR
)

mark_as_advanced(
    CLICKHOUSECPP_ROOT_DIR
    CLICKHOUSECPP_INCLUDE_DIR
    CLICKHOUSECPP_LIBRARY
)