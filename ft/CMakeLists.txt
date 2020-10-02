include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/contrib"
  "${PROJECT_SOURCE_DIR}/freeticks")

set(lib_NAME ft-core)

set(lib_SOURCES
    matching/OrderBook.cpp
    qsh/QshDecoder.cpp    
)

add_library(${lib_NAME}-static STATIC ${lib_SOURCES})
set_target_properties(${lib_NAME}-static PROPERTIES OUTPUT_NAME ${lib_NAME})
target_link_libraries(${lib_NAME}-static pthread)
install(TARGETS ${lib_NAME}-static DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT static)

set(ft_core_LIBRARY ft-core-static)

set(test_SOURCES
    matching/OrderBook.ut.cpp
    spb/SpbDecoder.ut.cpp
    utils/Pool.ut.cpp
  )

add_executable(${lib_NAME}-test
  ${test_SOURCES}
  Main.ut.cpp)

target_link_libraries(${lib_NAME}-test
  ${${lib_NAME}_LIBRARY} "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}" stdc++fs ${tb_core_LIBRARY} ${tb_bm_LIBRARY} ${ft_core_LIBRARY})

foreach(file ${test_SOURCES})
  get_filename_component(dir  "${file}" DIRECTORY)
  get_filename_component(name "${file}" NAME_WE)
  add_test(NAME ${dir}::${name}Suite COMMAND ${lib_NAME}-test -l error -t ${name}Suite)
endforeach()
