option (
    USE_GSETTINGS
    "Generate GSettings schemas"
    ON
)

option (
    COMPIZ_DISABLE_GS_SCHEMAS_INSTALL
    "Disables gsettings schema installation"
    OFF
)

set (
    COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR ${COMPIZ_INSTALL_GSETTINGS_SCHEMA_DIR} CACHE PATH
    "Installation path of the gsettings schema file"
)

function (compiz_install_gsettings_schema _src _dst)
    find_program (PKG_CONFIG_TOOL pkg-config)
    find_program (GLIB_COMPILE_SCHEMAS glib-compile-schemas)
    mark_as_advanced (FORCE PKG_CONFIG_TOOL)

    # find out where schemas need to go if we are installing them systemwide
    execute_process (COMMAND ${PKG_CONFIG_TOOL} glib-2.0 --variable prefix  OUTPUT_VARIABLE GSETTINGS_GLIB_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    SET (GSETTINGS_GLOBAL_INSTALL_DIR "${GSETTINGS_GLIB_PREFIX}/share/glib-2.0/schemas/")

    if (PKG_CONFIG_TOOL AND
	GLIB_COMPILE_SCHEMAS AND NOT
	COMPIZ_DISABLE_SCHEMAS_INSTALL AND
	USE_GSETTINGS)
	install (CODE "
		message (\"$ENV{USER} is the username in use right now\")
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
		    #	message (\"[WARNING]: Installing GSettings schemas to directory that is not in XDG_DATA_DIRS, you need to add ${COMPIZ_DESTDIR}${_dst} to your XDG_DATA_DIRS in order for GSettings schemas to be found!\"\)
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

# generate gconf schema
function (compiz_gsettings_schema _src _dst _inst)
    find_program (XSLTPROC_EXECUTABLE xsltproc)
    mark_as_advanced (FORCE XSLTPROC_EXECUTABLE)

    if (XSLTPROC_EXECUTABLE AND USE_GSETTINGS)
	add_custom_command (
	    OUTPUT ${_dst}
	    COMMAND ${XSLTPROC_EXECUTABLE}
		    -o ${_dst}
		    ${COMPIZ_GSETTINGS_SCHEMAS_XSLT}
		    ${_src}
	    DEPENDS ${_src}
	)
	compiz_install_gsettings_schema (${_dst} ${_inst})
    endif ()
endfunction ()
