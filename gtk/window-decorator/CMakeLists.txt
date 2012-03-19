if (USE_GTK)

    if (COMPIZ_BUILD_WITH_RPATH)
	set (CMAKE_INSTALL_RPATH ${libdir})
    endif (COMPIZ_BUILD_WITH_RPATH)

    add_subdirectory (local-menus)

    include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/local-menus/src
	${compiz_SOURCE_DIR}/include
	${CMAKE_BINARY_DIR}/gtk
	${GTK_WINDOW_DECORATOR_INCLUDE_DIRS}
	${METACITY_INCLUDE_DIRS}
	${GCONF_INCLUDE_DIRS}
	${DBUS_GLIB_INCLUDE_DIRS}
    )

    add_definitions (
	-DHAVE_CONFIG_H
	-DALL_LINGUAS=\"${ALL_LINGUAS}\"
	-DLOCALEDIR=\\\"${datadir}/locale\\\"
    )

    link_directories (
	${COMPIZ_LINK_DIRS}
    )

    if (USE_GCONF)
        set (gwd_schema ${CMAKE_CURRENT_BINARY_DIR}/gwd.schemas)
        compiz_translate_xml (
	    ${CMAKE_CURRENT_SOURCE_DIR}/gwd.schemas.in
	    ${gwd_schema}
	)
	compiz_install_gconf_schema (${CMAKE_CURRENT_BINARY_DIR}/gwd.schemas ${SCHEMADIR})
    endif (USE_GCONF)

    add_executable (gtk-window-decorator
	gtk-window-decorator.c
	blurprops.c
	decorprops.c
	cairo.c
	frames.c
	gdk.c
	switcher.c
	metacity.c
	events.c
	forcequit.c
	tooltip.c
	actionmenu.c
	settings.c
	util.c
	style.c
	wnck.c
	decorator.c
	${gwd_schema}
    )

    add_dependencies (gtk-window-decorator decoration)

    if (USE_METACITY)
	set (metacitylibs ${METACITY_LIBRARIES})
    endif (USE_METACITY)


    target_link_libraries (
	gtk-window-decorator
	decoration

	gtk_window_decorator_local_menus

	${GTK_WINDOW_DECORATOR_LIBRARIES}
	${GCONF_LIBRARIES}
	${DBUS_GLIB_LIBRARIES}
	${metacitylibs}
    )

    install (
	TARGETS gtk-window-decorator
	DESTINATION ${COMPIZ_DESTDIR}${exec_prefix}
    )


endif (USE_GTK)



