add_executable (compiz_test_composite_pixmapbinding
                ${CMAKE_CURRENT_SOURCE_DIR}/test-composite-pixmapbinding.cpp)

target_link_libraries (compiz_test_composite_pixmapbinding
                       compiz_composite_pixmapbinding
                       ${GTEST_BOTH_LIBRARIES}
		       ${GMOCK_LIBRARY}
		       ${GMOCK_MAIN_LIBRARY})

compiz_discover_tests (compiz_test_composite_pixmapbinding COVERAGE compiz_composite_pixmapbinding)
