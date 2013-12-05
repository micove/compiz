set (COMPIZ_LD_LIBRARY_PATH ${CMAKE_BINARY_DIR}/src CACHE STRING "" FORCE)
set (COMPIZ_BINARY ${CMAKE_BINARY_DIR}/src/compiz CACHE STRING "" FORCE)

set (COMPIZ_XORG_GTEST_COMMUNICATOR_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/communicator CACHE PATH "" FORCE)
set (COMPIZ_XORG_GTEST_COMMUNICATOR_LIBRARY_DIRS ${CMAKE_CURRENT_BINARY_DIR}/communicator CACHE PATH "" FORCE)
set (COMPIZ_XORG_GTEST_COMMUNICATOR_LIBRARY compiz_xorg_gtest_communicator CACHE STRING "" FORCE)
set (COMPIZ_TEST_ONLY_PLUGIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/plugins/)
set (COMPIZ_REAL_PLUGIN_PATH ${CMAKE_BINARY_DIR}/plugins/)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/src/compiz-xorg-gtest-config.h.in
		${CMAKE_CURRENT_BINARY_DIR}/src/compiz-xorg-gtest-config.h
		@ONLY)

include_directories (${XORG_SERVER_INCLUDE_XORG_GTEST}
		     ${XORG_SERVER_GTEST_INCLUDES}
                     ${COMPIZ_XORG_SYSTEM_TEST_INCLUDE_DIR}
                     ${CMAKE_CURRENT_BINARY_DIR}/src
                     ${compiz_SOURCE_DIR}/tests/shared
		     ${COMPIZ_XORG_GTEST_COMMUNICATOR_INCLUDE_DIR})

link_directories (${XORG_SERVER_LIBRARY_DIRS})

add_definitions (${XORG_SERVER_GTEST_CFLAGS})

set (_xorg_gtest_all_srcs
     ${XORG_SERVER_GTEST_SRC}/src/xorg-gtest-all.cpp)

add_library (xorg_gtest_all STATIC
	     ${_xorg_gtest_all_srcs})

add_library (compiz_xorg_gtest_main STATIC
             ${CMAKE_CURRENT_SOURCE_DIR}/src/compiz_xorg_gtest_main.cpp)

add_library (compiz_xorg_gtest_system_test STATIC
	     ${CMAKE_CURRENT_SOURCE_DIR}/src/compiz-xorg-gtest.cpp)

add_executable (xorg_gtest_wrapper
		${CMAKE_CURRENT_SOURCE_DIR}/src/xorg_gtest_wrapper.cpp)

target_link_libraries (xorg_gtest_all
                       ${X11_XI_LIBRARIES}
		       ${GTEST_BOTH_LIBRARIES}
		       ${XORG_SERVER_GTEST_LIBRARIES})

target_link_libraries (compiz_xorg_gtest_main
		       ${GTEST_BOTH_LIBRARIES}
		       ${XORG_SERVER_GTEST_LIBRARIES})

target_link_libraries (compiz_xorg_gtest_system_test
		       xorg_gtest_all
		       compiz_xorg_gtest_main
		       ${GTEST_BOTH_LIBRARIES}
		       ${XORG_SERVER_LIBRARIES}
		       ${COMPIZ_XORG_GTEST_COMMUNICATOR_LIBRARY}
		       ${X11_XI_LIBRARIES})

add_executable (xorg_gtest_wrapper
		${CMAKE_CURRENT_SOURCE_DIR}/src/xorg_gtest_wrapper.cpp)

add_subdirectory (communicator)
add_subdirectory (plugins)
