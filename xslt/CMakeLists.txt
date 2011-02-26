
set (_files
    bcop.xslt
)

if (USE_GCONF)
    list (APPEND _files compiz_gconf_schemas.xslt)
endif ()

install (
    FILES ${_files}
    DESTINATION ${COMPIZ_DESTDIR}${CMAKE_INSTALL_PREFIX}/share/compiz/xslt
)
