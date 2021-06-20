
include_directories(PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/contrib"
  "${PROJECT_SOURCE_DIR}")

set(app_LIBRARIES
  "${tb_core_LIBRARY}"
  "${ft_core_LIBRARY}"
)
if(PCAP_FOUND)
set(app_LIBRARIES "${app_LIBRARIES}" 
  "${PCAP_LIBRARY}")
endif()
if(CLICKHOUSECPP_FOUND)
set(app_LIBRARIES "${app_LIBRARIES}"
  "${CLICKHOUSECPP_LIBRARY}")
endif()

add_executable(mdserv MdServApp.cpp)
target_link_libraries(mdserv ${app_LIBRARIES})

