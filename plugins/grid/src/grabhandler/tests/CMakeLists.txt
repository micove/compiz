add_executable (compiz_test_grid_grabhandler
                ${CMAKE_CURRENT_SOURCE_DIR}/test-grid-grab-handler.cpp)

target_link_libraries (compiz_test_grid_grabhandler
                       compiz_grid_grabhandler
                       ${GTEST_BOTH_LIBRARIES}
		       ${GMOCK_LIBRARY}
		       ${GMOCK_MAIN_LIBRARY})

compiz_discover_tests (compiz_test_grid_grabhandler COVERAGE compiz_grid_grabhandler)
