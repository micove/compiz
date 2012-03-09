find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin(resize PLUGINDEPS composite opengl)
