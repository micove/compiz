find_package (Compiz REQUIRED)

include (CompizPlugin)

find_package (OpenGL)
if (OPENGL_FOUND)
    compiz_plugin(opengl PLUGINDEPS composite LIBRARIES ${OPENGL_gl_LIBRARY} INCDIRS ${OPENGL_INCLUDE_DIR})
endif ()