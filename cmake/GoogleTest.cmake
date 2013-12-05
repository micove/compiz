find_package (Threads REQUIRED)

# Find the the Google Mock include directory
# by searching the system-wide include directory
# paths
find_path (GMOCK_INCLUDE_DIR
           gmock/gmock.h)

if (GMOCK_INCLUDE_DIR)
    set (GMOCK_INCLUDE_BASE "include/")
    string (LENGTH ${GMOCK_INCLUDE_BASE} GMOCK_INCLUDE_BASE_LENGTH)
    string (LENGTH ${GMOCK_INCLUDE_DIR} GMOCK_INCLUDE_DIR_LENGTH)

    math (EXPR
          GMOCK_INCLUDE_PREFIX_LENGTH
          "${GMOCK_INCLUDE_DIR_LENGTH} - ${GMOCK_INCLUDE_BASE_LENGTH}")
    string (SUBSTRING
            ${GMOCK_INCLUDE_DIR}
            0
            ${GMOCK_INCLUDE_PREFIX_LENGTH}
            GMOCK_INCLUDE_PREFIX)

    set (GMOCK_SRC_DIR ${GMOCK_INCLUDE_PREFIX}/src/gmock CACHE PATH "Path to Google Mock Sources")
    set (GMOCK_INCLUDE_DIR ${GMOCK_INCLUDE_DIR} CACHE PATH "Path to Google Mock Headers")
    set (GTEST_INCLUDE_DIR ${GMOCK_SRC_DIR}/gtest/include CACHE PATH "Path to Google Test Headers")

    set (GMOCK_LIBRARY "gmock" CACHE STRING "Name of the Google Mock library")
    set (GMOCK_MAIN_LIBRARY "gmock_main" CACHE STIRNG "Name of the Google Mock main () library")
    set (GTEST_BOTH_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} gtest gtest_main)

endif (GMOCK_INCLUDE_DIR)

if (NOT GTEST_BOTH_LIBRARIES)

    set (GTEST_FOUND FALSE)

else (NOT GTEST_BOTH_LIBRARIES)

    set (GTEST_FOUND TRUE)

endif (NOT GTEST_BOTH_LIBRARIES)

if (NOT GMOCK_LIBRARY OR NOT GMOCK_MAIN_LIBRARY OR NOT GTEST_FOUND)

    message ("Google Mock and Google Test not found - cannot build tests!")
    set (GOOGLE_TEST_AND_MOCK_FOUND FALSE)

else (NOT GMOCK_LIBRARY OR NOT GMOCK_MAIN_LIBRARY OR NOT GTEST_FOUND)

    set (GOOGLE_TEST_AND_MOCK_FOUND TRUE)

    add_definitions(-DGTEST_USE_OWN_TR1_TUPLE=0)
    add_subdirectory (${GMOCK_SRC_DIR} ${CMAKE_BINARY_DIR}/__gmock)
    include_directories (${GMOCK_INCLUDE_DIR}
                         ${GTEST_INCLUDE_DIR})


endif (NOT GMOCK_LIBRARY OR NOT GMOCK_MAIN_LIBRARY OR NOT GTEST_FOUND)
