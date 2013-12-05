add_executable (compiz_discover_gtest_tests
		EXCLUDE_FROM_ALL
		${CMAKE_CURRENT_SOURCE_DIR}/compiz_discover_gtest_tests.cpp)

if (BUILD_XORG_GTEST)
    add_dependencies (compiz_discover_gtest_tests xorg_gtest_wrapper)
endif (BUILD_XORG_GTEST)

set_target_properties (compiz_discover_gtest_tests PROPERTIES
		       RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/compiz_gtest)
