
set (_files
    bcop.xslt
)

compiz_configure_file (compiz_gsettings_schemas.xslt.in compiz_gsettings_schemas.xslt)

if (USE_GCONF)
    list (APPEND _files compiz_gconf_schemas.xslt)
    list (APPEND _files ${CMAKE_CURRENT_BINARY_DIR}/compiz_gsettings_schemas.xslt)
endif ()

if (USE_GSETTINGS)
    
endif ()

install (
    FILES ${_files}
    DESTINATION ${COMPIZ_DESTDIR}${CMAKE_INSTALL_PREFIX}/share/compiz/xslt
)
