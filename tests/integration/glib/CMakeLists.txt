include (FindPkgConfig)

pkg_check_modules (COMPIZ_GLIB_INTEGRATION_TEST glibmm-2.4)

if (COMPIZ_GLIB_INTEGRATION_TEST_FOUND)

    include_directories (${compiz_SOURCE_DIR}/src/)
    include_directories (${compiz_SOURCE_DIR}/src/timer/include)
    include_directories (${compiz_SOURCE_DIR}/src/timer/src)
    include_directories (${compiz_SOURCE_DIR}/include)
    include_directories (${COMPIZ_GLIB_INTEGRATION_TEST_INCLUDE_DIRS})

    link_directories (${compiz_BINARY_DIR}/src)
    link_directories (${COMPIZ_GLIB_INTEGRATION_TEST_LIBRARY_DIRS})

    add_executable (compiz_glib_integration_test
		    ${CMAKE_CURRENT_SOURCE_DIR}/glib_integration_test.cpp)

    target_link_libraries (compiz_glib_integration_test
		           compiz_core
			   ${COMPIZ_GLIB_INTEGRATION_TEST_LIBRARIES}
			   ${GTEST_BOTH_LIBRARIES}
			   ${GMOCK_LIBRARY}
			   ${GMOCK_MAIN_LIBRARY})

    compiz_discover_tests (compiz_glib_integration_test COVERAGE compiz_core)

endif (COMPIZ_GLIB_INTEGRATION_TEST_FOUND)
