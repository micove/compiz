add_executable (compiz_test_wall_offset_movement
                ${CMAKE_CURRENT_SOURCE_DIR}/test-wall-offset-movement.cpp)

target_link_libraries (compiz_test_wall_offset_movement
                       compiz_wall_offset_movement
                       ${GTEST_BOTH_LIBRARIES}
		       ${GMOCK_LIBRARY}
		       ${GMOCK_MAIN_LIBRARY})

compiz_discover_tests (compiz_test_wall_offset_movement COVERAGE compiz_wall_offset_movement)
