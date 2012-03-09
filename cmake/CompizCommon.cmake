cmake_minimum_required (VERSION 2.6)

if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
    message (SEND_ERROR "Building in the source directory is not supported.")
    message (FATAL_ERROR "Please remove the created \"CMakeCache.txt\" file, the \"CMakeFiles\" directory and create a build directory and call \"${CMAKE_COMMAND} <path to the sources>\".")
endif ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")

#### policies

cmake_policy (SET CMP0000 OLD)
cmake_policy (SET CMP0002 OLD)
cmake_policy (SET CMP0003 NEW)
cmake_policy (SET CMP0005 OLD)
cmake_policy (SET CMP0011 OLD)

set (CMAKE_SKIP_RPATH FALSE)

option (COMPIZ_BUILD_WITH_RPATH "Leave as ON unless building packages" ON)
option (COMPIZ_RUN_LDCONFIG "Leave OFF unless you need to run ldconfig after install")
option (COMPIZ_PACKAGING_ENABLED "Enable to manually set prefix, exec_prefix, libdir, includedir, datadir" OFF)
set (COMPIZ_DESTDIR "${DESTDIR}" CACHE STRING "Leave blank unless building packages")

if (NOT COMPIZ_DESTDIR)
    set (COMPIZ_DESTDIR $ENV{DESTDIR})
endif ()

set (COMPIZ_DATADIR ${CMAKE_INSTALL_PREFIX}/share)
set (COMPIZ_METADATADIR ${CMAKE_INSTALL_PREFIX}/share/compiz)
set (COMPIZ_IMAGEDIR ${CMAKE_INSTALL_PREFIX}/share/compiz/images)
set (COMPIZ_PLUGINDIR ${libdir}/compiz)
set (COMPIZ_SYSCONFDIR ${sysconfdir})

set (
    VERSION ${VERSION} CACHE STRING
    "Package version that is added to a plugin pkg-version file"
)

set (
    COMPIZ_I18N_DIR ${COMPIZ_I18N_DIR} CACHE PATH "Translation file directory"
)

option (COMPIZ_SIGN_WARNINGS "Should compiz use -Wsign-conversion during compilation." OFF)

if (COMPIZ_SIGN_WARNINGS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wsign-conversion")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wsign-conversion")
else ()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif ()

function (compiz_ensure_linkage)
    find_program (LDCONFIG_EXECUTABLE ldconfig)
    mark_as_advanced (FORCE LDCONFIG_EXECUTABLE)

    if (LDCONFIG_EXECUTABLE AND ${COMPIZ_RUN_LDCONFIG})

    install (
        CODE "message (\"Running \" ${LDCONFIG_EXECUTABLE} \" \" ${CMAKE_INSTALL_PREFIX} \"/lib\")
	      exec_program (${LDCONFIG_EXECUTABLE} ARGS \"-v\" ${CMAKE_INSTALL_PREFIX}/lib)"
        )

    endif (LDCONFIG_EXECUTABLE AND ${COMPIZ_RUN_LDCONFIG})
endfunction ()

macro (compiz_add_git_dist)
    set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${VERSION})
    add_custom_target(dist
	COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
		| bzip2 > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endmacro ()

# unsets the given variable
macro (compiz_unset var)
    set (${var} "" CACHE INTERNAL "")
endmacro ()

# sets the given variable
macro (compiz_set var value)
    set (${var} ${value} CACHE INTERNAL "")
endmacro ()


macro (compiz_format_string str length return)
    string (LENGTH "${str}" _str_len)
    math (EXPR _add_chr "${length} - ${_str_len}")
    set (${return} "${str}")
    while (_add_chr GREATER 0)
	set (${return} "${${return}} ")
	math (EXPR _add_chr "${_add_chr} - 1")
    endwhile ()
endmacro ()

string (ASCII 27 _escape)
function (compiz_color_message _str)
    if (CMAKE_COLOR_MAKEFILE)
	message (${_str})
    else ()
	string (REGEX REPLACE "${_escape}.[0123456789;]*m" "" __str ${_str})
	message (${__str})
    endif ()
endfunction ()

function (compiz_configure_file _src _dst)
    foreach (_val ${ARGN})
        set (_${_val}_sav ${${_val}})
        set (${_val} "")
	foreach (_word ${_${_val}_sav})
	    set (${_val} "${${_val}}${_word} ")
	endforeach (_word ${_${_val}_sav})
    endforeach (_val ${ARGN})

    configure_file (${_src} ${_dst} @ONLY)

    foreach (_val ${ARGN})
	set (${_val} ${_${_val}_sav})
        set (_${_val}_sav "")
    endforeach (_val ${ARGN})
endfunction ()

function (compiz_add_plugins_in_folder folder)
    file (
        GLOB _plugins_in
        RELATIVE "${folder}"
        "${folder}/*/CMakeLists.txt"
    )

    foreach (_plugin ${_plugins_in})
        get_filename_component (_plugin_dir ${_plugin} PATH)
        add_subdirectory (${folder}/${_plugin_dir})
    endforeach ()
endfunction ()

#### pkg-config handling

include (FindPkgConfig)

function (compiz_pkg_check_modules _var _req)
    if (NOT ${_var})
        pkg_check_modules (${_var} ${_req} ${ARGN})
	if (${_var}_FOUND)
	    set (${_var} 1 CACHE INTERNAL "" FORCE)
	endif ()
	set(__pkg_config_checked_${_var} 0 CACHE INTERNAL "" FORCE)
    endif ()
endfunction ()

#### translations

# translate metadata file
function (compiz_translate_xml _src _dst)
    find_program (INTLTOOL_MERGE_EXECUTABLE intltool-merge)
    mark_as_advanced (FORCE INTLTOOL_MERGE_EXECUTABLE)

    if (INTLTOOL_MERGE_EXECUTABLE
	AND COMPIZ_I18N_DIR
	AND EXISTS ${COMPIZ_I18N_DIR})
	add_custom_command (
	    OUTPUT ${_dst}
	    COMMAND ${INTLTOOL_MERGE_EXECUTABLE} -x -u -c
		    ${CMAKE_BINARY_DIR}/.intltool-merge-cache
		    ${COMPIZ_I18N_DIR}
		    ${_src}
		    ${_dst}
	    DEPENDS ${_src}
	)
    else ()
    	add_custom_command (
	    OUTPUT ${_dst}
	    COMMAND cat ${_src} |
		    sed -e 's;<_;<;g' -e 's;</_;</;g' > 
		    ${_dst}
	    DEPENDS ${_src}
	)
    endif ()
endfunction ()

function (compiz_translate_desktop_file _src _dst)
    find_program (INTLTOOL_MERGE_EXECUTABLE intltool-merge)
    mark_as_advanced (FORCE INTLTOOL_MERGE_EXECUTABLE)

    if (INTLTOOL_MERGE_EXECUTABLE
	AND COMPIZ_I18N_DIR
	AND EXISTS ${COMPIZ_I18N_DIR})
	add_custom_command (
	    OUTPUT ${_dst}
	    COMMAND ${INTLTOOL_MERGE_EXECUTABLE} -d -u -c
		    ${CMAKE_BINARY_DIR}/.intltool-merge-cache
		    ${COMPIZ_I18N_DIR}
		    ${_src}
		    ${_dst}
	    DEPENDS ${_src}
	)
    else ()
    	add_custom_command (
	    OUTPUT ${_dst}
	    COMMAND cat ${_src} |
		    sed -e 's;^_;;g' >
		    ${_dst}
	    DEPENDS ${_src}
	)
    endif ()
endfunction ()

#### optional file install

function (compiz_opt_install_file _src _dst)
    install (CODE
        "message (\"-- Installing: ${_dst}\")
         execute_process (
	    COMMAND ${CMAKE_COMMAND} -E copy_if_different \"${_src}\" \"${COMPIZ_DESTDIR}${_dst}\"
	    RESULT_VARIABLE _result
	    OUTPUT_QUIET ERROR_QUIET
	 )
	 if (_result)
	     message (\"-- Failed to install: ${_dst}\")
	 endif ()
        "
    )
endfunction ()

#### uninstall

macro (compiz_add_uninstall)
   if (NOT _compiz_uninstall_rule_created)
	compiz_set(_compiz_uninstall_rule_created TRUE)

	set (_file "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")

	file (WRITE  ${_file} "if (NOT EXISTS \"${CMAKE_BINARY_DIR}/install_manifest.txt\")\n")
	file (APPEND ${_file} "  message (FATAL_ERROR \"Cannot find install manifest: \\\"${CMAKE_BINARY_DIR}/install_manifest.txt\\\"\")\n")
	file (APPEND ${_file} "endif (NOT EXISTS \"${CMAKE_BINARY_DIR}/install_manifest.txt\")\n\n")
	file (APPEND ${_file} "file (READ \"${CMAKE_BINARY_DIR}/install_manifest.txt\" files)\n")
	file (APPEND ${_file} "string (REGEX REPLACE \"\\n\" \";\" files \"\${files}\")\n")
	file (APPEND ${_file} "foreach (file \${files})\n")
	file (APPEND ${_file} "  message (STATUS \"Uninstalling \\\"\${file}\\\"\")\n")
	file (APPEND ${_file} "  if (EXISTS \"\${file}\")\n")
	file (APPEND ${_file} "    exec_program(\n")
	file (APPEND ${_file} "      \"${CMAKE_COMMAND}\" ARGS \"-E remove \\\"\${file}\\\"\"\n")
	file (APPEND ${_file} "      OUTPUT_VARIABLE rm_out\n")
	file (APPEND ${_file} "      RETURN_VALUE rm_retval\n")
	file (APPEND ${_file} "      )\n")
	file (APPEND ${_file} "    if (\"\${rm_retval}\" STREQUAL 0)\n")
	file (APPEND ${_file} "    else (\"\${rm_retval}\" STREQUAL 0)\n")
	file (APPEND ${_file} "      message (FATAL_ERROR \"Problem when removing \\\"\${file}\\\"\")\n")
	file (APPEND ${_file} "    endif (\"\${rm_retval}\" STREQUAL 0)\n")
	file (APPEND ${_file} "  else (EXISTS \"\${file}\")\n")
	file (APPEND ${_file} "    message (STATUS \"File \\\"\${file}\\\" does not exist.\")\n")
	file (APPEND ${_file} "  endif (EXISTS \"\${file}\")\n")
	file (APPEND ${_file} "endforeach (file)\n")

	add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")

    endif ()
endmacro ()

#posix 2008 scandir check
include (CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES (
  "# include <dirent.h>
   int func (const char *d, dirent ***list, void *sort)
   {
     int n = scandir(d, list, 0, (int(*)(const dirent **, const dirent **))sort);
     return n;
   }

   int main (int, char **)
   {
     return 0;
   }
  "
  HAVE_SCANDIR_POSIX)

if (HAVE_SCANDIR_POSIX)
  add_definitions (-DHAVE_SCANDIR_POSIX)
endif ()
