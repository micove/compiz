include (FindPkgConfig)

pkg_check_modules (X11_XI x11 xi xext)

if (BUILD_XORG_GTEST AND X11_XI_FOUND)

    include_directories (${compiz_SOURCE_DIR}/tests/shared
                         ${COMPIZ_XORG_SYSTEM_TEST_INCLUDE_DIR}
                         ${X11_INCLUDE_DIRS}
                         ${XORG_SERVER_INCLUDE_XORG_GTEST}
                         ${XORG_SERVER_GTEST_SRC}
                         ${GTEST_INCLUDE_DIRS})

    link_directories (${X11_XI_LIBRARY_DIRS}
                      ${COMPIZ_COMPOSITE_DAMAGETRACKING_INTEGRATION_LIBRARY_DIRS})

    add_executable (compiz_test_decor_acceptance
                    ${CMAKE_CURRENT_SOURCE_DIR}/compiz_decor_acceptance_tests.cpp)

    set (COMPIZ_DECOR_ACCEPTANCE_TEST_LIBRARIES
         xorg_gtest_all
         compiz_xorg_gtest_main
         compiz_xorg_gtest_system_test
         decoration
         compiz_point
         compiz_size
         compiz_rect
         ${GMOCK_LIBRARY}
         ${GMOCK_MAIN_LIBRARY}
         ${GTEST_BOTH_LIBRARIES}
         ${XORG_SERVER_LIBRARIES}
         ${X11_XI_LIBRARIES})

    target_link_libraries (compiz_test_decor_acceptance
                           ${COMPIZ_DECOR_ACCEPTANCE_TEST_LIBRARIES})

    # Disabled until the tests can be run without opengl
    #compiz_discover_tests (compiz_test_decor_acceptance WITH_XORG_GTEST)

endif (BUILD_XORG_GTEST AND X11_XI_FOUND)

