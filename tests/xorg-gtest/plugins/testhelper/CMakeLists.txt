find_package (Compiz REQUIRED)

include (CompizPlugin)

include_directories (${COMPIZ_XORG_GTEST_COMMUNICATOR_INCLUDE_DIR})

compiz_plugin (testhelper NOINSTALL LIBRARIES ${COMPIZ_XORG_GTEST_COMMUNICATOR_LIBRARY})
