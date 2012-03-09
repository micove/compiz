if (USE_GNOME)

    include_directories (
	${CMAKE_BINARY_DIR}/gtk
	${GNOME_WINDOW_SETTINGS_INCLUDE_DIRS}
	${GCONF_INCLUDE_DIRS}
    )

    add_definitions (
	-DHAVE_CONFIG_H
	-DMETACITY_THEME_DIR=\\\"${windowsettingsdatadir}/themes\\\"
    )

    link_directories (
	${COMPIZ_LINK_DIRS}
    )

    set (desktop_file ${CMAKE_CURRENT_BINARY_DIR}/compiz.desktop)
    compiz_translate_desktop_file (
        ${CMAKE_CURRENT_SOURCE_DIR}/compiz.desktop.in
        ${desktop_file}
    )
    install (
	FILES ${desktop_file}
	DESTINATION ${COMPIZ_DESTDIR}${datadir}/applications
    )

    set (wmproperties_file ${CMAKE_CURRENT_BINARY_DIR}/compiz-wm.desktop)
    compiz_translate_desktop_file (
        ${CMAKE_CURRENT_SOURCE_DIR}/compiz-wm.desktop.in
        ${wmproperties_file}
    )
    compiz_opt_install_file (
	${wmproperties_file}
	${windowsettingsdatadir}/gnome/wm-properties/compiz-wm.desktop
    )


    if (USE_GNOME_KEYBINDINGS)
        compiz_translate_xml (
	    ${CMAKE_CURRENT_SOURCE_DIR}/50-compiz-desktop-key.xml.in
	    ${CMAKE_CURRENT_BINARY_DIR}/50-compiz-desktop-key.xml
	)
	compiz_translate_xml (
	    ${CMAKE_CURRENT_SOURCE_DIR}/50-compiz-key.xml.in
	    ${CMAKE_CURRENT_BINARY_DIR}/50-compiz-key.xml
	)
	set (key_xml
	    ${CMAKE_CURRENT_BINARY_DIR}/50-compiz-desktop-key.xml
	    ${CMAKE_CURRENT_BINARY_DIR}/50-compiz-key.xml
	)
	compiz_opt_install_file (
	    ${CMAKE_CURRENT_BINARY_DIR}/50-compiz-desktop-key.xml
	    ${keybindingsdir}/50-compiz-desktop-key.xml
	)
	compiz_opt_install_file (
	    ${CMAKE_CURRENT_BINARY_DIR}/50-compiz-key.xml
	    ${keybindingsdir}/50-compiz-key.xml
	)
    endif (USE_GNOME_KEYBINDINGS)

    add_library (gnome-compiz MODULE
	compiz-window-manager.c
	compiz-window-manager.h
	${key_xml}
	${desktop_file}
	${wmproperties_file}
    )


    set_target_properties (gnome-compiz
	PROPERTIES OUTPUT_NAME compiz
	SKIP_BUILD_RPATH 1
    )

    target_link_libraries (
	gnome-compiz ${GNOME_WINDOW_SETTINGS_LIBRARIES}
    )

    compiz_opt_install_file (
	${CMAKE_CURRENT_BINARY_DIR}/libcompiz.so
	${windowsettingslibdir}/window-manager-settings/libcompiz.so
    )
endif (USE_GNOME)
