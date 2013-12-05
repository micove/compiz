include_directories (${GTEST_SHARED_INCLUDE_DIR})

add_library (compiz_gtest_shared_async_task STATIC
             gtest_shared_asynctask.cpp)

target_link_libraries (compiz_gtest_shared_async_task)
