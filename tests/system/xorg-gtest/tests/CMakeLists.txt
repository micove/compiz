pkg_check_modules (X11_XI x11 xi xext)

if (BUILD_XORG_GTEST AND X11_XI_FOUND)

    include_directories (${compiz_SOURCE_DIR}/tests/shared
                         ${COMPIZ_XORG_SYSTEM_TEST_INCLUDE_DIR}
			 ${X11_INCLUDE_DIRS}
			 ${XORG_SERVER_INCLUDE_XORG_GTEST}
			 ${XORG_SERVER_GTEST_SRC}
                         ${GTEST_INCLUDE_DIRS}
                         ${COMPIZ_XORG_GTEST_COMMUNICATOR_INCLUDE_DIR})

    link_directories (${X11_XI_LIBRARY_DIRS}
                      ${compiz_BINARY_DIR}/tests/shared/src)

    add_executable (compiz_xorg_gtest_test_window_stacking
		    ${CMAKE_CURRENT_SOURCE_DIR}/compiz_xorg_gtest_test_window_stacking.cpp)

    add_executable (compiz_xorg_gtest_test_configure_window
                    ${CMAKE_CURRENT_SOURCE_DIR}/compiz_xorg_gtest_configure_window.cpp)

    add_executable (compiz_xorg_gtest_test_icccm
		    ${CMAKE_CURRENT_SOURCE_DIR}/compiz_xorg_gtest_icccm.cpp)

    add_executable (compiz_xorg_gtest_test_ewmh
		    ${CMAKE_CURRENT_SOURCE_DIR}/compiz_xorg_gtest_ewmh.cpp)

    add_executable (compiz_xorg_gtest_test_shape_handling
		    ${CMAKE_CURRENT_SOURCE_DIR}/compiz_xorg_gtest_test_shape_handling.cpp)

    set (COMPIZ_XORG_GTEST_LIBRARIES
	 compiz_xorg_gtest_system_test
	 xorg_gtest_all
	 compiz_xorg_gtest_main
         ${GTEST_BOTH_LIBRARIES}
	 ${XORG_SERVER_LIBRARIES}
	 ${X11_XI_LIBRARIES})

    target_link_libraries (compiz_xorg_gtest_test_window_stacking
			   ${COMPIZ_XORG_GTEST_LIBRARIES})

    compiz_discover_tests (compiz_xorg_gtest_test_window_stacking WITH_XORG_GTEST)

    target_link_libraries (compiz_xorg_gtest_test_icccm
                           compiz_gtest_shared_async_task
			   ${COMPIZ_XORG_GTEST_LIBRARIES})

    compiz_discover_tests (compiz_xorg_gtest_test_icccm WITH_XORG_GTEST)

    target_link_libraries (compiz_xorg_gtest_test_ewmh
			   ${COMPIZ_XORG_GTEST_LIBRARIES})

    compiz_discover_tests (compiz_xorg_gtest_test_ewmh WITH_XORG_GTEST)

    target_link_libraries (compiz_xorg_gtest_test_shape_handling
			   ${COMPIZ_XORG_GTEST_LIBRARIES})

    compiz_discover_tests (compiz_xorg_gtest_test_shape_handling WITH_XORG_GTEST)

    target_link_libraries (compiz_xorg_gtest_test_configure_window
			   ${COMPIZ_XORG_GTEST_LIBRARIES})

    add_dependencies (compiz_xorg_gtest_test_configure_window testhelper)

    compiz_discover_tests (compiz_xorg_gtest_test_configure_window WITH_XORG_GTEST)

endif (BUILD_XORG_GTEST AND X11_XI_FOUND)
