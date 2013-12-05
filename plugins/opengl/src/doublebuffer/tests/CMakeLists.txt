add_executable (compiz_test_opengl_double_buffer
                ${CMAKE_CURRENT_SOURCE_DIR}/test-opengl-double-buffer.cpp)

target_link_libraries (compiz_test_opengl_double_buffer
                       compiz_opengl_double_buffer
                       ${GTEST_BOTH_LIBRARIES}
		       ${GMOCK_LIBRARY}
		       ${GMOCK_MAIN_LIBRARY})

compiz_discover_tests (compiz_test_opengl_double_buffer COVERAGE compiz_opengl_double_buffer)
