cmake_minimum_required (VERSION 2.6)

include (FindPkgConfig)
pkg_check_modules (COMPIZ_UNTEST REQUIRED gtk+-3.0 gl x11)

add_executable (untest untest.c)
include_directories (${COMPIZ_UNTEST_INCLUDE_DIRS})
link_directories (${COMPIZ_UNTEST_LIBRARY_DIRS})
target_link_libraries (untest ${COMPIZ_UNTEST_LIBRARIES})
