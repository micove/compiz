find_package (Compiz REQUIRED)

include (CompizPlugin)

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/src/pixmapbinding/include)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/src/backbuffertracking/include)

link_directories (${CMAKE_CURRENT_BINARY_DIR}/src/pixmapbinding)
link_directories (${CMAKE_CURRENT_BINARY_DIR}/src/backbuffertracking)

compiz_plugin (composite LIBRARIES compiz_composite_pixmapbinding compiz_composite_backbuffertracking)

add_subdirectory (src/pixmapbinding)
add_subdirectory (src/backbuffertracking)
