find_package (Compiz REQUIRED)
include (CompizPlugin)

add_subdirectory (src/click_threshold)
include_directories (src/click_threshold/include)
add_subdirectory (src/wall_offset)
include_directories (src/wall_offset/include)

compiz_plugin (expo
    PLUGINDEPS composite opengl
    LIBRARIES compiz_expo_click_threshold compiz_expo_wall_offset
)
