option (
    COMPIZ_DISABLE_SCHEMAS_INSTALL
    "Disables gsettings schema installation"
    OFF
)

option (
    USE_GSETTINGS
    "Generate GSettings schemas"
    ON
)

set (
    COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR ${COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR} CACHE PATH
    "Installation path of the gsettings schema file"
)

macro (compiz_gsettings_prepare_install_dirs)
    # package
    if ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "package")
	if (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
            set (PLUGIN_SCHEMADIR "${datadir}/glib-2.0/schemas/")
        else (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
	    set (PLUGIN_SCHEMADIR "${COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR}")
	endif (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
    # compiz
    elseif ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "compiz" OR
	    "$ENV{BUILD_GLOBAL}" STREQUAL "true")
	if (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
            set (PLUGIN_SCHEMADIR "${COMPIZ_PREFIX}/share/glib-2.0/schemas/")
        else (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
	    set (PLUGIN_SCHEMADIR "${COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR}")
	endif (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
    # local
    else ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "compiz" OR
	  "$ENV{BUILD_GLOBAL}" STREQUAL "true")

	if (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
            set (PLUGIN_SCHEMADIR "$ENV{HOME}/.config/compiz-1/gsettings/schemas")
        else (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)
	    set (PLUGIN_SCHEMADIR "${COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR}")
	endif (NOT COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR)

    endif ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "package")
endmacro (compiz_gsettings_prepare_install_dirs)

function (compiz_install_gsettings_schema _src _dst)
    find_program (PKG_CONFIG_TOOL pkg-config)
    find_program (GLIB_COMPILE_SCHEMAS glib-compile-schemas)
    mark_as_advanced (FORCE PKG_CONFIG_TOOL)

    # find out where schemas need to go if we are installing them systemwide
    execute_process (COMMAND ${PKG_CONFIG_TOOL} glib-2.0 --variable prefix  OUTPUT_VARIABLE GSETTINGS_GLIB_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    SET (GSETTINGS_GLOBAL_INSTALL_DIR "${GSETTINGS_GLIB_PREFIX}/share/glib-2.0/schemas/")

    if (USE_GSETTINGS AND
	PKG_CONFIG_TOOL AND
	GLIB_COMPILE_SCHEMAS AND NOT
	COMPIZ_DISABLE_SCHEMAS_INSTALL)
	install (CODE "
		if (\"$ENV{USER}\"\ STREQUAL \"root\")
		    message (\"-- Installing GSettings schemas ${GSETTINGS_GLOBAL_INSTALL_DIR}\"\)
		    file (INSTALL DESTINATION \"${GSETTINGS_GLOBAL_INSTALL_DIR}\"
			  TYPE FILE
			  FILES \"${_src}\")
		    message (\"-- Recompiling GSettings schemas in ${GSETTINGS_GLOBAL_INSTALL_DIR}\"\)
		    execute_process (COMMAND ${GLIB_COMPILE_SCHEMAS} ${GSETTINGS_GLOBAL_INSTALL_DIR})

		else (\"$ENV{USER}\"\ STREQUAL \"root\"\)
		    # It seems like this is only available in CMake > 2.8.5
		    # but hardly anybody has that, so comment out this warning for now
		    # string (FIND $ENV{XDG_DATA_DIRS} \"${COMPIZ_DESTDIR}${_dst}\" XDG_INSTALL_PATH)
		    # if (NOT XDG_INSTALL_PATH)
		    message (\"[WARNING]: Installing GSettings schemas to a custom location that might not be in XDG_DATA_DIRS, you need to add ${COMPIZ_DESTDIR}${_dst} to your XDG_DATA_DIRS in order for GSettings schemas to be found!\"\)
		    # endif (NOT XDG_INSTALL_PATH)
		    message (\"-- Installing GSettings schemas to ${COMPIZ_DESTDIR}${_dst}\"\)
		    file (INSTALL DESTINATION \"${COMPIZ_DESTDIR}${_dst}\"
			  TYPE FILE
			  FILES \"${_src}\")
		    message (\"-- Recompiling GSettings schemas in ${COMPIZ_DESTDIR}${_dst}\"\)
		    execute_process (COMMAND ${GLIB_COMPILE_SCHEMAS} ${COMPIZ_DESTDIR}${_dst})
		endif (\"$ENV{USER}\" STREQUAL \"root\"\)
		")
    endif ()
endfunction ()

# generate gsettings schema
find_program (XSLTPROC_EXECUTABLE xsltproc)
mark_as_advanced (FORCE XSLTPROC_EXECUTABLE)

if (XSLTPROC_EXECUTABLE AND USE_GSETTINGS)
    compiz_gsettings_prepare_install_dirs ()

    add_custom_command (
	OUTPUT "${CMAKE_BINARY_DIR}/generated/org.freedesktop.compiz.${COMPIZ_CURRENT_PLUGIN}.gschema.xml"
	COMMAND ${XSLTPROC_EXECUTABLE}
	        -o "${CMAKE_BINARY_DIR}/generated/org.freedesktop.compiz.${COMPIZ_CURRENT_PLUGIN}.gschema.xml"
	        ${COMPIZ_GSETTINGS_SCHEMAS_XSLT}
	        ${COMPIZ_CURRENT_XML_FILE}
	DEPENDS ${COMPIZ_CURRENT_XML_FILE}
    )
    compiz_install_gsettings_schema ("${CMAKE_BINARY_DIR}/generated/org.freedesktop.compiz.${COMPIZ_CURRENT_PLUGIN}.gschema.xml" ${PLUGIN_SCHEMADIR})
    list (APPEND COMPIZ_CURRENT_SOURCES_ADDS ${CMAKE_BINARY_DIR}/generated/org.freedesktop.compiz.${COMPIZ_CURRENT_PLUGIN}.gschema.xml)
endif ()
