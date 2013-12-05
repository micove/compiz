# determinate installation directories
macro (compiz_data_prepare_dirs)
    if ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "package")
	set (PLUGIN_DATADIR   ${datadir}/compiz/${COMPIZ_CURRENT_PLUGIN})

    elseif ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "compiz" OR
	    "$ENV{BUILD_GLOBAL}" STREQUAL "true")
	set (PLUGIN_DATADIR   ${COMPIZ_PREFIX}/share/compiz/${COMPIZ_CURRENT_PLUGIN})

    else ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "compiz" OR
	  "$ENV{BUILD_GLOBAL}" STREQUAL "true")
	set (PLUGIN_DATADIR   $ENV{HOME}/.compiz-1/${COMPIZ_CURRENT_PLUGIN})

    endif ("${COMPIZ_PLUGIN_INSTALL_TYPE}" STREQUAL "package")
endmacro (compiz_data_prepare_dirs)

# install plugin data files
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
    compiz_data_prepare_dirs ()
    if (_install_plugin_${COMPIZ_CURRENT_PLUGIN})
	install (
	    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
	    DESTINATION ${PLUGIN_DATADIR}
	)
    endif (_install_plugin_${COMPIZ_CURRENT_PLUGIN})
    list (APPEND COMPIZ_DEFINITIONS_ADD "-DDATADIR='\"${PLUGIN_DATADIR}\"'")
endif ()
