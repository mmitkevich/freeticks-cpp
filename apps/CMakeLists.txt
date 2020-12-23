
include_directories(PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/contrib"
  "${PROJECT_SOURCE_DIR}")

set(app_LIBRARIES
  ${tb_core_LIBRARY}
  ${ft_core_LIBRARY}
  ${PCAP_LIBRARY})

add_executable(mdserv MdServApp.cpp)
target_link_libraries(mdserv ${app_LIBRARIES})

