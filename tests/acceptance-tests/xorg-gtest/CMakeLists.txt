if (BUILD_XORG_GTEST)

    include_directories (${compiz_SOURCE_DIR}/tests/shared
                         ${COMPIZ_XORG_SYSTEM_TEST_INCLUDE_DIR}
			 ${X11_INCLUDE_DIRS}
			 ${XORG_SERVER_INCLUDE_XORG_GTEST}
			 ${XORG_SERVER_GTEST_SRC}
			 ${GTEST_INCLUDE_DIRS})
    add_subdirectory (tests)

endif (BUILD_XORG_GTEST)
