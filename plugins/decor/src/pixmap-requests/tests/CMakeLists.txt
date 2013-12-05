include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library (compiz_decor_pixmap_requests_mock STATIC
             ${CMAKE_CURRENT_SOURCE_DIR}/compiz_decor_pixmap_requests_mock.cpp)

target_link_libraries (compiz_decor_pixmap_requests_mock
                       compiz_decor_pixmap_requests)

add_executable (compiz_test_decor_pixmap_requests
                ${CMAKE_CURRENT_SOURCE_DIR}/test-decor-pixmap-requests.cpp)

target_link_libraries (compiz_test_decor_pixmap_requests
		       compiz_decor_pixmap_requests
                       compiz_decor_pixmap_requests_mock
		       decoration
                       ${GTEST_BOTH_LIBRARIES}
		       ${GMOCK_LIBRARY}
		       ${GMOCK_MAIN_LIBRARY})

compiz_discover_tests (compiz_test_decor_pixmap_requests COVERAGE compiz_decor_pixmap_requests)

add_subdirectory (integration)
