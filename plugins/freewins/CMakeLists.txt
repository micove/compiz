find_package (Compiz REQUIRED)

include (CompizPlugin)
include (FindOpenGL)

if (OPENGL_GLU_FOUND)
compiz_plugin (freewins PLUGINDEPS composite opengl PKGDEPS cairo-xlib cairo LIBRARIES ${OPENGL_glu_LIBRARY} INCDIRS ${OPENGL_INCLUDE_DIR} LDFLAGSADD)
endif (OPENGL_GLU_FOUND)
