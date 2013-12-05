find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin (wizard PLUGINDEPS composite opengl mousepoll)
