include (FindPkgConfig)

if (BUILD_XORG_GTEST)

    include_directories (${compiz_SOURCE_DIR}/tests/shared
                         ${COMPIZ_XORG_SYSTEM_TEST_INCLUDE_DIR}
                         ${X11_INCLUDE_DIRS}
                         ${XORG_SERVER_INCLUDE_XORG_GTEST}
                         ${XORG_SERVER_GTEST_SRC}
                         ${GTEST_INCLUDE_DIRS}
                         ${COMPIZ_DECOR_PIXMAP_PROTOCOL_INTEGRATION_INCLUDE_DIRS})

    link_directories (${X11_XI_LIBRARY_DIRS}
                      ${COMPIZ_COMPOSITE_DAMAGETRACKING_INTEGRATION_LIBRARY_DIRS})

    add_executable (compiz_test_decor_pixmap_protocol_integration
                    ${CMAKE_CURRENT_SOURCE_DIR}/compiz_test_decor_pixmap_protocol_integration.cpp)

    set (COMPIZ_DECOR_PIXMAP_PROTOCOL_XORG_INTEGRATION_TEST_LIBRARIES
         xorg_gtest_all
         compiz_xorg_gtest_main
         compiz_xorg_gtest_system_test
         compiz_decor_pixmap_requests
         compiz_decor_pixmap_requests_mock
         decoration
         ${GMOCK_LIBRARY}
         ${GMOCK_MAIN_LIBRARY}
         ${GTEST_BOTH_LIBRARIES}
         ${XORG_SERVER_LIBRARIES}
         ${X11_XI_LIBRARIES}
         ${COMPIZ_DECOR_PIXMAP_PROTOCOL_INTEGRATION_LIBRARIES})

    target_link_libraries (compiz_test_decor_pixmap_protocol_integration
                           ${COMPIZ_DECOR_PIXMAP_PROTOCOL_XORG_INTEGRATION_TEST_LIBRARIES})

    compiz_discover_tests (compiz_test_decor_pixmap_protocol_integration WITH_XORG_GTEST COVERAGE
                           compiz_decor_pixmap_requests)

endif (BUILD_XORG_GTEST)

