# XorgGTest.cmake
#
# build_xorg_gtest_locally (dir) specifies a subdirectory to
# build xorg-gtest in locally

function (build_xorg_gtest_locally build_directory)

    if (XORG_GTEST_FOUND)

	add_subdirectory (${XORG_SERVER_GTEST_ROOT} ${build_directory})

    endif (XORG_GTEST_FOUND)

endfunction ()
