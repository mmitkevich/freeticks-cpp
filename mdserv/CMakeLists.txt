set(app_NAME mdserv)

include_directories(PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/contrib"
  "${PROJECT_SOURCE_DIR}")

set(app_SOURCES
    MdServ.cpp)

add_executable(${app_NAME}
  ${app_SOURCES})

target_link_libraries(${app_NAME}
  ${tb_core_LIBRARY}
  ${ft_core_LIBRARY}
  ${PCAP_LIBRARY})
