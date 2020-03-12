set(app_NAME mdconv)

include_directories(PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/contrib"
  "${PROJECT_SOURCE_DIR}")

set(app_SOURCES
    QScalp.cpp
    MdConv.cpp)

add_executable(${app_NAME}
  ${app_SOURCES})

target_link_libraries(${app_NAME}
  ${toolbox_LIBRARY})
