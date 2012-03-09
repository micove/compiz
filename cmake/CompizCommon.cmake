cmake_minimum_required (VERSION 2.6)

if ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
    message (SEND_ERROR "Building in the source directory is not supported.")
    message (FATAL_ERROR "Please remove the created \"CMakeCache.txt\" file, the \"CMakeFiles\" directory and create a build directory and call \"${CMAKE_COMMAND} <path to the sources>\".")
endif ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")

#### CTest
enable_testing()

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
option (COMPIZ_BUILD_TESTING "Build Unit Tests" ON)
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

# Almost everything is a shared library now, so almost everything needs -fPIC
set (COMMON_FLAGS "-fPIC -Wall")

option (COMPIZ_DEPRECATED_WARNINGS "Warn about declarations marked deprecated" OFF)
if (NOT COMPIZ_DEPRECATED_WARNINGS)
    set (COMMON_FLAGS "${COMMON_FLAGS} -Wno-deprecated-declarations")
endif ()

option (COMPIZ_SIGN_WARNINGS "Should compiz use -Wsign-conversion during compilation." ON)
if (NOT COMPIZ_SIGN_WARNINGS)
    set (COMMON_FLAGS "${COMMON_FLAGS} -Wno-sign-conversion")
endif ()

if (${CMAKE_PROJECT_NAME} STREQUAL "compiz")
    set (COMPIZ_WERROR_DEFAULT ON)
else ()
    set (COMPIZ_WERROR_DEFAULT OFF)
endif ()
option (COMPIZ_WERROR "Treat warnings as errors" ${COMPIZ_WERROR_DEFAULT})
if (COMPIZ_WERROR)
    set (COMMON_FLAGS "${COMMON_FLAGS} -Werror")
endif ()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")

if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.bzr)
    set(IS_BZR_REPO 1)
elseif (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.bzr)
    set(IS_BZR_REPO 0)
endif (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.bzr)

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

	add_custom_target (dist
			   COMMAND bzr export --root=${CMAKE_PROJECT_NAME}-${VERSION} ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-${VERSION}.tar.bz2
			   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

endmacro ()

macro (compiz_add_distcheck)
	add_custom_target (distcheck 
			   COMMAND mkdir -p ${CMAKE_BINARY_DIR}/dist-build
			   && cp ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-${VERSION}.tar.bz2 ${CMAKE_BINARY_DIR}/dist-build
			   && cd ${CMAKE_BINARY_DIR}/dist-build
			   && tar xvf ${CMAKE_BINARY_DIR}/dist-build/${CMAKE_PROJECT_NAME}-${VERSION}.tar.bz2
			   && mkdir -p ${CMAKE_BINARY_DIR}/dist-build/${CMAKE_PROJECT_NAME}-${VERSION}/build
			   && cd ${CMAKE_BINARY_DIR}/dist-build/${CMAKE_PROJECT_NAME}-${VERSION}/build
			   && cmake -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/dist-build/buildroot -DCOMPIZ_PLUGIN_INSTALL_TYPE='package' .. -DCMAKE_MODULE_PATH=/usr/share/cmake -DCOMPIZ_DISABLE_PLUGIN_KDE=ON -DBUILD_KDE4=OFF
			   && make
			   && make test
			   && make install
			   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	add_dependencies (distcheck dist)
endmacro ()

macro (compiz_add_release_signoff)

	set (AUTO_VERSION_UPDATE "" CACHE STRING "Automatically update VERSION to this number")

	if (AUTO_VERSION_UPDATE)
		message ("-- Next version will be " ${AUTO_VERSION_UPDATE})
	endif (AUTO_VERSION_UPDATE)

	add_custom_target (release-signoff)

	add_custom_target (release-update-working-tree
			   COMMAND cp NEWS ${CMAKE_SOURCE_DIR} && bzr add ${CMAKE_SOURCE_DIR}/NEWS &&
				   cp AUTHORS ${CMAKE_SOURCE_DIR} && bzr add ${CMAKE_SOURCE_DIR}/AUTHORS
			   COMMENT "Updating working tree"
			   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}) 

	# TODO
	add_custom_target (release-commits)
	add_custom_target (release-tags)
	add_custom_target (release-branch)
	add_custom_target (release-update-dist)
	add_custom_target (release-version-bump)

	add_custom_target (release-sign-tarballs
		   COMMAND gpg --armor --sign --detach-sig ${CMAKE_PROJECT_NAME}-${VERSION}.tar.bz2
	   COMMENT "Signing tarball"
	   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
	add_custom_target (release-sha1-tarballs
		   COMMAND sha1sum ${CMAKE_PROJECT_NAME}-${VERSION}.tar.bz2 > ${CMAKE_PROJECT_NAME}-${VERSION}.tar.bz2.sha1
		   COMMENT "SHA1Summing tarball"
		   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
	add_custom_target (release-sign-sha1-tarballs
		   COMMAND gpg --armor --sign --detach-sig ${CMAKE_PROJECT_NAME}-${VERSION}.tar.bz2.sha1
		   COMMENT "Signing SHA1Sum checksum"
		   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

	add_dependencies (release-commits release-update-working-tree)
	add_dependencies (release-tags release-commits)
	add_dependencies (release-branch release-tags)
	add_dependencies (release-update-dist release-branch)
	add_dependencies (release-version-bump release-update-dist)
	add_dependencies (release-sign-tarballs release-version-bump)
	add_dependencies (release-sha1-tarballs release-sign-tarballs)
	add_dependencies (release-sign-sha1-tarballs release-sha1-tarballs)

	add_dependencies (release-signoff release-sign-sha1-tarballs)

	# Actually pushes the release
	add_custom_target (push-master)
	add_custom_target (push-release-branch)
	add_custom_target (push-tag)

	add_custom_target (release-push)

	add_dependencies (release-push push-release-branch)
	add_dependencies (push-release-branch push-tag)
	add_dependencies (push-tag push-master)

	# Push the tarball to releases.compiz.org

	# Does nothing for now
	add_custom_target (release-upload-component)
	add_custom_target (release-upload)

	add_dependencies (release-upload-component release-upload-version)
	add_dependencies (release-upload release-upload-component)

endmacro ()

macro (compiz_add_release)

	set (AUTO_NEWS_UPDATE "" CACHE STRING "Value to insert into NEWS file, leave blank to get an editor when running make news-update")

	if (AUTO_NEWS_UPDATE)
		message ("-- Using auto news update: " ${AUTO_NEWS_UPDATE})
	endif (AUTO_NEWS_UPDATE)

	if (NOT EXISTS ${CMAKE_SOURCE_DIR}/.AUTHORS.sed)
		file (WRITE ${CMAKE_SOURCE_DIR}/.AUTHORS.sed "")
	endif (NOT EXISTS ${CMAKE_SOURCE_DIR}/.AUTHORS.sed)

	add_custom_target (authors
			   COMMAND bzr log --long --levels=0 | grep -e "^\\s*author:" -e "^\\s*committer:" | cut -d ":" -f 2 | sed -r -f ${CMAKE_SOURCE_DIR}/.AUTHORS.sed  | sort -u > AUTHORS
			   COMMENT "Generating AUTHORS")

	if (AUTO_NEWS_UPDATE)

		add_custom_target (news-header echo > ${CMAKE_BINARY_DIR}/NEWS.update
				   COMMAND echo 'Release ${VERSION} ('`date +%Y-%m-%d`' '`bzr config email`')' > ${CMAKE_BINARY_DIR}/NEWS.update && seq -s "=" `cat ${CMAKE_BINARY_DIR}/NEWS.update | wc -c` | sed 's/[0-9]//g' >> ${CMAKE_BINARY_DIR}/NEWS.update && echo '${AUTO_NEWS_UPDATE}' >> ${CMAKE_BINARY_DIR}/NEWS.update && echo >> ${CMAKE_BINARY_DIR}/NEWS.update
				   COMMENT "Generating NEWS Header"
				   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	else (AUTO_NEWS_UPDATE)
		add_custom_target (news-header echo > ${CMAKE_BINARY_DIR}/NEWS.update
				   COMMAND echo 'Release ${VERSION} ('`date +%Y-%m-%d`' '`bzr config email`')' > ${CMAKE_BINARY_DIR}/NEWS.update && seq -s "=" `cat ${CMAKE_BINARY_DIR}/NEWS.update | wc -c` | sed 's/[0-9]//g' >> ${CMAKE_BINARY_DIR}/NEWS.update && $ENV{EDITOR} ${CMAKE_BINARY_DIR}/NEWS.update && echo >> ${CMAKE_BINARY_DIR}/NEWS.update
				   COMMENT "Generating NEWS Header"
				   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	endif (AUTO_NEWS_UPDATE)

	add_custom_target (news
			   COMMAND cat ${CMAKE_SOURCE_DIR}/NEWS > NEWS.old &&
				   cat NEWS.old >> NEWS.update &&
				   cat NEWS.update > NEWS
			   WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

	add_dependencies (news-header authors)
	add_dependencies (news news-header)

	add_custom_target (release-prep)
	add_dependencies (release-prep news)

endmacro (compiz_add_release)

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

macro (compiz_add_plugins_in_folder folder)
    set (COMPIZ_PLUGIN_PACK_BUILD 1)
    file (
        GLOB _plugins_in
        RELATIVE "${folder}"
        "${folder}/*/CMakeLists.txt"
    )

    foreach (_plugin ${_plugins_in})
        get_filename_component (_plugin_dir ${_plugin} PATH)
        add_subdirectory (${folder}/${_plugin_dir})
    endforeach ()
endmacro ()

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

#### modules / tests
macro (_get_parameters _prefix)
    set (_current_var _foo)
    set (_supported_var PKGDEPS PLUGINDEPS MODULES LDFLAGSADD CFLAGSADD LIBRARIES LIBDIRS INCDIRS DEFSADD)
    foreach (_val ${_supported_var})
	set (${_prefix}_${_val})
    endforeach (_val)
    foreach (_val ${ARGN})
	set (_found FALSE)
	foreach (_find ${_supported_var})
	    if ("${_find}" STREQUAL "${_val}")
		set (_found TRUE)
	    endif ("${_find}" STREQUAL "${_val}")
	endforeach (_find)

	if (_found)
	    set (_current_var ${_prefix}_${_val})
	else (_found)
	    list (APPEND ${_current_var} ${_val})
	endif (_found)
    endforeach (_val)
endmacro (_get_parameters)

macro (_check_pkg_deps _prefix)
    set (${_prefix}_HAS_PKG_DEPS TRUE)
    foreach (_val ${ARGN})
        string (REGEX REPLACE "[<>=\\.]" "_" _name ${_val})
	string (TOUPPER ${_name} _name)

	compiz_pkg_check_modules (_${_name} ${_val})

	if (_${_name}_FOUND)
	    list (APPEND ${_prefix}_PKG_LIBDIRS "${_${_name}_LIBRARY_DIRS}")
	    list (APPEND ${_prefix}_PKG_LIBRARIES "${_${_name}_LIBRARIES}")
	    list (APPEND ${_prefix}_PKG_INCDIRS "${_${_name}_INCLUDE_DIRS}")
	else (_${_name}_FOUND)
	    set (${_prefix}_HAS_PKG_DEPS FALSE)
	    compiz_set (${_prefix}_MISSING_DEPS "${${_prefix}_MISSING_DEPS} ${_val}")
	    set(__pkg_config_checked__${_name} 0 CACHE INTERNAL "" FORCE)
	endif (_${_name}_FOUND)
    endforeach ()
endmacro (_check_pkg_deps)

macro (_build_include_flags _prefix)
    foreach (_include ${ARGN})
	if (NOT ${_prefix}_INCLUDE_CFLAGS)
	    compiz_set (${_prefix}_INCLUDE_CFLAGS "" PARENT_SCOPE)
	endif (NOT ${_prefix}_INCLUDE_CFLAGS)
	list (APPEND ${_prefix}_INCLUDE_CFLAGS -I${_include})
    endforeach (_include)
endmacro (_build_include_flags)

macro (_build_definitions_flags _prefix)
    foreach (_def ${ARGN})
	if (NOT ${_prefix}_DEFINITIONS_CFLAGS)
	    compiz_set (${_prefix}_DEFINITIONS_CFLAGS "")
	endif (NOT ${_prefix}_DEFINITIONS_CFLAGS)
	list (APPEND ${_prefix}_DEFINITIONS_CFLAGS -D${_def})
    endforeach (_def)
endmacro (_build_definitions_flags)

macro (_build_link_dir_flags _prefix)
    foreach (_link_dir ${ARGN})
	if (NOT ${_prefix}_LINK_DIR_LDFLAGS)
	    compiz_set (${_prefix}_LINK_DIR_LDFLAGS "")
	endif (NOT ${_prefix}_LINK_DIR_LDFLAGS)
	list (APPEND ${_prefix}_LINK_DIR_LDFLAGS -L${_link_dir})
    endforeach (_link_dir)
endmacro (_build_link_dir_flags)

macro (_build_library_flags _prefix)
    foreach (_library ${ARGN})
	if (NOT ${_prefix}_LIBRARY_LDFLAGS)
	    compiz_set (${_prefix}_LIBRARY_LDFLAGS "")
	endif (NOT ${_prefix}_LIBRARY_LDFLAGS)
	list (APPEND ${_prefix}_LIBRARY_LDFLAGS -l${_library})
    endforeach (_library)
endmacro (_build_library_flags)

function (_build_compiz_module _prefix _name _full_prefix)

    if (${_full_prefix}_INCLUDE_DIRS)
	_build_include_flags (${_full_prefix} ${${_full_prefix}_INCLUDE_DIRS})
    endif (${_full_prefix}_INCLUDE_DIRS)
    _build_include_flags (${_full_prefix} ${${_full_prefix}_SOURCE_DIR})
    _build_include_flags (${_full_prefix} ${${_full_prefix}_INCLUDE_DIR})

    if (${_full_prefix}_DEFSADD)
	_build_definitions_flags (${_full_prefix} ${${_full_prefix}_DEFSADD})
    endif (${_full_prefix}_DEFSADD)

    if (${_full_prefix}_LIBRARY_DIRS)
	_build_link_dir_flags (${_full_prefix} ${${_full_prefix}_LIBRARY_DIRS})
    endif (${_full_prefix}_LIBRARY_DIRS)

    if (${_full_prefix}_LIBRARIES)
	_build_library_flags (${_full_prefix} ${${_full_prefix}_LIBRARIES})
    endif (${_full_prefix}_LIBRARIES)			      

    file (GLOB _cpp_files "${${_full_prefix}_SOURCE_DIR}/*.cpp")

    add_library (${_prefix}_${_name}_internal STATIC ${_cpp_files})

    target_link_libraries (${_prefix}_${_name}_internal
			   ${${_full_prefix}_LIBRARIES} m pthread dl)

    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_INCLUDE_CFLAGS})
    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_DEFINITIONS_CFLAGS})
    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_CFLAGSADD})

    set (${_full_prefix}_COMPILE_FLAGS_STR " ")
    foreach (_flag ${${_full_prefix}_COMPILE_FLAGS})
	set (${_full_prefix}_COMPILE_FLAGS_STR "${_flag} ${${_full_prefix}_COMPILE_FLAGS_STR}")
    endforeach (_flag)

    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LINK_LDFLAGS})
    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LDFLAGSADD})
    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LIBARY_FLAGS})

    set (${_full_prefix}_LINK_FLAGS_STR " ")
    foreach (_flag ${${_full_prefix}_LINK_FLAGS})
	set (${_full_prefix}_LINK_FLAGS_STR "${_flag} ${${_full_prefix}_LINK_FLAGS_STR}")
    endforeach (_flag)

    set_target_properties (${_prefix}_${_name}_internal PROPERTIES
			   COMPILE_FLAGS ${${_full_prefix}_COMPILE_FLAGS_STR}
			   LINK_FLAGS ${${_full_prefix}_LINK_FLAGS_STR})

    file (GLOB _h_files "${_full_prefix}_INCLUDE_DIR/*.h")

    foreach (_file ${_h_files})

	install (
	    FILES ${_file}
	    DESTINATION ${COMPIZ_DESTDIR}${includedir}/compiz/${_prefix}
	)

    endforeach (_file)

endfunction (_build_compiz_module)

macro (compiz_module _prefix _name)

    string (TOUPPER ${_prefix} _PREFIX)
    string (TOUPPER ${_name} _NAME)
    set (_FULL_PREFIX ${_PREFIX}_${_NAME})

    _get_parameters (${_FULL_PREFIX} ${ARGN})
    _check_pkg_deps (${_FULL_PREFIX} ${${_FULL_PREFIX}_PKGDEPS})

    if (${_FULL_PREFIX}_HAS_PKG_DEPS)

	list (APPEND ${_FULL_PREFIX}_LIBRARIES ${${_FULL_PREFIX}_PKG_LIBRARIES})
	list (APPEND ${_FULL_PREFIX}_INCLUDE_DIRS ${${_FULL_PREFIX}_INCDIRS})
	list (APPEND ${_FULL_PREFIX}_INCLUDE_DIRS ${${_FULL_PREFIX}_PKG_INCDIRS})
	list (APPEND ${_FULL_PREFIX}_LIBRARY_DIRS ${${_FULL_PREFIX}_LIBDIRS})
	list (APPEND ${_FULL_PREFIX}_LIBRARY_DIRS ${${_FULL_PREFIX}_PKG_LIBDIRS})

	# also add modules
	foreach (_module ${${_FULL_PREFIX}_MODULES})
	    string (TOUPPER ${_module} _MODULE)
	    list (APPEND ${_FULL_PREFIX}_INCLUDE_DIRS ${${_MODULE}_INCLUDE_DIR})
	    list (APPEND ${_FULL_PREFIX}_LIBRARY_DIRS ${${_MODULE}_BINARY_DIR})
	    list (APPEND ${_FULL_PREFIX}_LIBRARIES ${_module}_internal)
	endforeach (_module)

	compiz_set (${_FULL_PREFIX}_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/${_name})
	compiz_set (${_FULL_PREFIX}_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${_name}/src)
	compiz_set (${_FULL_PREFIX}_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${_name}/include)
	compiz_set (${_FULL_PREFIX}_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${_name}tests)

	# Need to abuse set_property here since set () with CACHE INTERNAL will save the 
	# value to the cache which we will just read right back (but we need to regenerate that)
	set_property (GLOBAL APPEND PROPERTY ${_PREFIX}_MOD_LIBRARY_DIRS ${${_FULL_PREFIX}_BINARY_DIR})
	set_property (GLOBAL APPEND PROPERTY ${_PREFIX}_MOD_INCLUDE_DIRS ${${_FULL_PREFIX}_INCLUDE_DIR})
	set_property (GLOBAL APPEND PROPERTY ${_PREFIX}_MOD_INCLUDE_DIRS ${${_FULL_PREFIX}_SOURCE_DIR})
	set_property (GLOBAL APPEND PROPERTY ${_PREFIX}_MOD_LIBRARIES ${_prefix}_${_name}_internal)

	_build_compiz_module (${_prefix} ${_name} ${_FULL_PREFIX})

	add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/${_name}/tests)

    else (${_FULL_PREFIX}_HAS_PKG_DEPS)
	message (STATUS "[WARNING] One or more dependencies for module ${_name} for ${_prefix} not found. Skipping module.")
	message (STATUS "Missing dependencies :${${_FULL_PREFIX}_MISSING_DEPS}")
	compiz_set (${_FULL_PREFIX}_BUILD FALSE)
    endif (${_FULL_PREFIX}_HAS_PKG_DEPS)

    
endmacro (compiz_module)

function (_build_compiz_test_base _prefix _module _full_prefix)

    file (GLOB _cpp_files "${${_FULL_TEST_BASE_PREFIX}_SOURCE_DIR}/*.cpp")

    if (${_full_prefix}_INCLUDE_DIRS)
	_build_include_flags (${_full_prefix} ${${_full_prefix}_INCLUDE_DIRS})
    endif (${_full_prefix}_INCLUDE_DIRS)
    _build_include_flags (${_full_prefix} ${${_full_prefix}_SOURCE_DIR})
    _build_include_flags (${_full_prefix} ${${_full_prefix}_INCLUDE_DIR})

    if (${_full_prefix}_DEFSADD)
	_build_definitions_flags (${_full_prefix} ${${_full_prefix}_DEFSADD})
    endif (${_full_prefix}_DEFSADD)

    if (${_full_prefix}_LIBRARY_DIRS)
	_build_link_dir_flags (${_full_prefix} ${${_full_prefix}_LIBRARY_DIRS})
    endif (${_full_prefix}_LIBRARY_DIRS)

    if (${_full_prefix}_LIBRARIES)
	_build_library_flags (${_full_prefix} ${${_full_prefix}_LIBRARIES})
    endif (${_full_prefix}_LIBRARIES)

    add_library (${_prefix}_${_module}_test_internal STATIC
		 ${_cpp_files})

    target_link_libraries (${_prefix}_${_module}_test_internal
			   ${${_full_prefix}_LIBRARIES}
			   ${_prefix}_${_module}_internal)


    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_INCLUDE_CFLAGS})
    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_DEFINITIONS_CFLAGS})
    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_CFLAGSADD})

    set (${_full_prefix}_COMPILE_FLAGS_STR "  ")
    foreach (_flag ${${_full_prefix}_COMPILE_FLAGS})
	set (${_full_prefix}_COMPILE_FLAGS_STR "${_flag} ${${_full_prefix}_COMPILE_FLAGS_STR}")
    endforeach (_flag)

    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LINK_LDFLAGS})
    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LDFLAGSADD})
    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LIBARY_FLAGS})

    set (${_full_prefix}_LINK_FLAGS_STR " ")
    foreach (_flag ${${_full_prefix}_LINK_FLAGS})
	set (${_full_prefix}_LINK_FLAGS_STR "${_flag} ${${_full_prefix}_LINK_FLAGS_STR}")
    endforeach (_flag)

    set_target_properties (${_prefix}_${_module}_test_internal PROPERTIES
			   COMPILE_FLAGS "${${_full_prefix}_COMPILE_FLAGS_STR}"
			   LINK_FLAGS "${${_full_prefix}_LINK_FLAGS_STR}")
endfunction (_build_compiz_test_base)

macro (compiz_test_base _prefix _module)

    string (TOUPPER ${_prefix} _PREFIX)
    string (TOUPPER ${_module} _MODULE)

    set (_FULL_MODULE_PREFIX ${_PREFIX}_${_NAME})
    set (_FULL_TEST_BASE_PREFIX ${_FULL_MODULE_PREFIX}_TEST_BASE)

    _get_parameters (${_FULL_TEST_BASE_PREFIX} ${ARGN})
    _check_pkg_deps (${_FULL_TEST_BASE_PREFIX} ${${_FULL_TEST_BASE_PREFIX}_PKGDEPS})

    if (${_FULL_TEST_BASE_PREFIX}_HAS_PKG_DEPS)

	list (APPEND ${_FULL_TEST_BASE_PREFIX}_LIBRARIES ${${_FULL_TEST_BASE_PREFIX}_PKG_LIBDIRS})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIRS ${${_FULL_TEST_BASE_PREFIX}_INCDIRS})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIRS ${${_FULL_TEST_BASE_PREFIX}_PKG_INCDIRS})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_LIBRARY_DIRS ${${_FULL_TEST_BASE_PREFIX}_LIBDIRS})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_LIBRARY_DIRS ${${_FULL_TEST_BASE_PREFIX}_PKG_LIBDIRS})

	compiz_set (${_FULL_TEST_BASE_PREFIX}_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
	compiz_set (${_FULL_TEST_BASE_PREFIX}_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
	compiz_set (${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

	list (APPEND ${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIRS ${${_FULL_MODULE_PREFIX}_INCLUDE_DIRS})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIRS ${${_FULL_MODULE_PREFIX}_INCLUDE_DIR})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIRS ${${_FULL_MODULE_PREFIX}_SOURCE_DIR})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_LIBRARY_DIRS ${${_FULL_MODULE_PREFIX}_LIBRARY_DIRS})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_LIBRARY_DIRS ${${_FULL_MODULE_PREFIX}_BINARY_DIR})
	list (APPEND ${_FULL_TEST_BASE_PREFIX}_LIBRARIES ${${_FULL_MODULE_PREFIX}_LIBRARIES})

	_build_compiz_test_base (${_prefix} ${_module} ${_FULL_TEST_BASE_PREFIX})
    else (${_FULL_TEST_BASE_PREFIX}_HAS_PKG_DEPS)
	message (STATUS "[WARNING] One or more dependencies for test base on module ${_module} for ${_prefix} not found. Skipping test base.")
	message (STATUS "Missing dependencies :${${_FULL_TEST_BASE_PREFIX}_MISSING_DEPS}")
	compiz_set (${_FULL_TEST_BASE_PREFIX}_BUILD FALSE)
    endif (${_FULL_TEST_BASE_PREFIX}_HAS_PKG_DEPS)
endmacro (compiz_test_base)

function (_build_compiz_test _prefix _module _test _full_prefix)
    file (GLOB _cpp_files "${${_FULL_TEST_PREFIX}_SOURCE_DIR}/*.cpp")

    if (${_full_prefix}_INCLUDE_DIRS)
	_build_include_flags (${_full_prefix} ${${_full_prefix}_INCLUDE_DIRS})
    endif (${_full_prefix}_INCLUDE_DIRS)
    _build_include_flags (${_full_prefix} ${${_full_prefix}_SOURCE_DIR})
    _build_include_flags (${_full_prefix} ${${_full_prefix}_INCLUDE_DIR})

    if (${_full_prefix}_DEFSADD)
	_build_definitions_flags (${_full_prefix} ${${_full_prefix}_DEFSADD})
    endif (${_full_prefix}_DEFSADD)

    if (${_full_prefix}_LIBRARY_DIRS)
	_build_link_dir_flags (${_full_prefix} ${${_full_prefix}_LIBRARY_DIRS})
    endif (${_full_prefix}_LIBRARY_DIRS)

    if (${_full_prefix}_LIBRARIES)
	_build_library_flags (${_full_prefix} ${${_full_prefix}_LIBRARIES})
    endif (${_full_prefix}_LIBRARIES)

    add_executable (${_prefix}_${_module}_${_test}_test
		    ${_cpp_files})

    target_link_libraries (${_prefix}_${_module}_${_test}_test
			   ${${_full_prefix}_LIBRARIES}
			   ${_prefix}_${_module}_internal
			   ${_prefix}_${_module}_test_internal)

    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_INCLUDE_CFLAGS})
    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_DEFINITIONS_CFLAGS})
    list (APPEND ${_full_prefix}_COMPILE_FLAGS ${${_full_prefix}_CFLAGSADD})

    set (${_full_prefix}_COMPILE_FLAGS_STR " ")
    foreach (_flag ${${_full_prefix}_COMPILE_FLAGS})
	set (${_full_prefix}_COMPILE_FLAGS_STR "${_flag} ${${_full_prefix}_COMPILE_FLAGS_STR}")
    endforeach (_flag)

    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LINK_LDFLAGS})
    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LDFLAGSADD})
    list (APPEND ${_full_prefix}_LINK_FLAGS ${${_full_prefix}_LIBARY_FLAGS})

    set (${_full_prefix}_LINK_FLAGS_STR " ")
    foreach (_flag ${${_full_prefix}_LINK_FLAGS})
	set (${_full_prefix}_LINK_FLAGS_STR "${_flag} ${${_full_prefix}_LINK_FLAGS_STR}")
    endforeach (_flag)

    set_target_properties (${_prefix}_${_module}_${_test}_test PROPERTIES
			   COMPILE_FLAGS "${${_full_prefix}_COMPILE_FLAGS_STR}"
			   LINK_FLAGS "${${_full_prefix}_LINK_FLAGS_STR}")

    add_test (test-${_prefix}-${_module}-${_test}
	      ${CMAKE_CURRENT_BINARY_DIR}/${_prefix}_${_module}_${_test}_test)
endfunction (_build_compiz_test)

macro (compiz_test _prefix _module _test)

    set (_supported_var PKGDEPS LDFLAGSADD CFLAGSADD LIBRARIES LIBDIRS INCDIRS DEFSADD)

    set (_FULL_TEST_PREFIX ${_FULL_MODULE_PREFIX}_TEST)

    _get_parameters (${_FULL_TEST_PREFIX} ${ARGN})
    _check_pkg_deps (${_FULL_TEST_PREFIX} ${${_FULL_TEST_PREFIX}_PKGDEPS})

    if (${_FULL_TEST_PREFIX}_HAS_PKG_DEPS)
	list (APPEND ${_FULL_TEST_PREFIX}_LIBRARIES ${${_FULL_TEST_PREFIX}_PKG_LIBDIRS})
	list (APPEND ${_FULL_TEST_PREFIX}_INCLUDE_DIRS ${${_FULL_TEST_PREFIX}_INCDIRS})
	list (APPEND ${_FULL_TEST_PREFIX}_INCLUDE_DIRS ${${_FULL_TEST_PREFIX}_PKG_INCDIRS})
	list (APPEND ${_FULL_TEST_PREFIX}_LIBRARY_DIRS ${${_FULL_TEST_PREFIX}_LIBDIRS})
	list (APPEND ${_FULL_TEST_PREFIX}_LIBRARY_DIRS ${${_FULL_TEST_PREFIX}_PKG_LIBDIRS})

	compiz_set (${_FULL_TEST_PREFIX}_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
	compiz_set (${_FULL_TEST_PREFIX}_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
	compiz_set (${_FULL_TEST_PREFIX}_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

	list (APPEND ${_FULL_TEST_PREFIX}_INCLUDE_DIRS ${${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIRS})
	list (APPEND ${_FULL_TEST_PREFIX}_INCLUDE_DIRS ${${_FULL_TEST_BASE_PREFIX}_INCLUDE_DIR})
	list (APPEND ${_FULL_TEST_PREFIX}_INCLUDE_DIRS ${${_FULL_TEST_BASE_PREFIX}_SOURCE_DIR})
	list (APPEND ${_FULL_TEST_PREFIX}_LIBRARY_DIRS ${${_FULL_TEST_BASE_PREFIX}_LIBRARY_DIRS})
	list (APPEND ${_FULL_TEST_PREFIX}_LIBRARY_DIRS ${${_FULL_TEST_BASE_PREFIX}_BINARY_DIR})
	list (APPEND ${_FULL_TEST_PREFIX}_LIBRARIES ${${_FULL_TEST_BASE_PREFIX}_LIBRARIES})

	_build_compiz_test (${_prefix} ${_module} ${_test} ${_FULL_TEST_PREFIX})

    else (${_FULL_TEST_PREFIX}_HAS_PKG_DEPS)
	message (STATUS "[WARNING] One or more dependencies for test ${_test} on module ${_name} for ${_prefix} not found. Skipping test.")
	message (STATUS "Missing dependencies :${${_FULL_TEST_PREFIX}_MISSING_DEPS}")
	compiz_set (${_FULL_TEST_PREFIX}_BUILD FALSE)
    endif (${_FULL_TEST_PREFIX}_HAS_PKG_DEPS)

endmacro (compiz_test)

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
if (CMAKE_CXX_COMPILER)
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
endif (CMAKE_CXX_COMPILER)

if (HAVE_SCANDIR_POSIX)
  add_definitions (-DHAVE_SCANDIR_POSIX)
endif ()
