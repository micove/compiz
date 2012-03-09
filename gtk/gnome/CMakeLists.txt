if (BUILD_GNOME)
    set (desktop_file ${CMAKE_CURRENT_BINARY_DIR}/compiz.desktop)
    compiz_translate_desktop_file (
        ${CMAKE_CURRENT_SOURCE_DIR}/compiz.desktop.in
        ${desktop_file}
    )
    install (
	FILES ${desktop_file}
	DESTINATION ${COMPIZ_DESTDIR}${datadir}/applications
    )

    add_custom_target (compiz.desktop ALL SOURCES ${desktop_file})
endif (BUILD_GNOME)
