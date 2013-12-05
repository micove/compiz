link_directories (${X11_XI_LIBRARY_DIRS}
                  ${compiz_BINARY_DIR}/tests/shared/src)

add_executable (compiz_acceptance_test_xorg_replace_current_wm
                ${CMAKE_CURRENT_SOURCE_DIR}/compiz_acceptance_replace_current_wm.cpp)

set (COMPIZ_XORG_ACCEPTANCE_TEST_LIBRARIES
     compiz_xorg_gtest_system_test
     xorg_gtest_all
     compiz_xorg_gtest_main
     ${GTEST_BOTH_LIBRARIES}
     ${XORG_SERVER_LIBRARIES}
     ${X11_XI_LIBRARIES})

target_link_libraries (compiz_acceptance_test_xorg_replace_current_wm
                       ${COMPIZ_XORG_ACCEPTANCE_TEST_LIBRARIES}
                       compiz_gtest_shared_async_task)

# Not autodiscovering tests here, these tests are
# inherently flakey. See README
