add_executable (compiz_test_resize_logic
                test-logic.cpp
		resize-options-mock.cpp
		x11-mocks.cpp)

target_link_libraries (compiz_test_resize_logic
                       resize_logic
                       compiz_core
                       ${GTEST_BOTH_LIBRARIES}
                       ${GMOCK_LIBRARY}
                       ${GMOCK_MAIN_LIBRARY})

compiz_discover_tests (compiz_test_resize_logic COVERAGE resize_logic)
