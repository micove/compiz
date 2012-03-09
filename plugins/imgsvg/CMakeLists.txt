find_package (Compiz REQUIRED)

include (CompizPlugin)

compiz_plugin(imgsvg PLUGINDEPS composite opengl PKGDEPS cairo>=1.0 cairo-xlib librsvg-2.0>=2.14.0)