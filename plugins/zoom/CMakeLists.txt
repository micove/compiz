find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin(zoom PLUGINDEPS composite opengl)