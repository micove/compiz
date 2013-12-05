# Hack
set (CMAKE_PROJECT_NAME compizconfig-python)

# Get arch
execute_process (COMMAND uname -p OUTPUT_VARIABLE PROC_ARCH)

# Get Python Version
execute_process (COMMAND python -c "import sys; print str (sys.version_info[0]) + '.' + str (sys.version_info[1])"
		 OUTPUT_VARIABLE PY_VERSION)

include (FindPkgConfig)
pkg_check_modules (PYTHON python-2.7)

# This is a workaround for a broken FindPythonLibs always finding the
# wrong library version because PYTHON_LIBRARY is in the cache and
# can't be overwritten
string (REGEX REPLACE "^([0-9]+\\.[0-9]+).*" "\\1" PYTHON_CURRENT_VERSION ${PY_VERSION})
string (REPLACE "." "" PYTHON_CURRENT_VERSION_NO_DOTS ${PYTHON_CURRENT_VERSION})
find_library (PYTHON_LIBRARY_LOCAL
	      NAMES python${PYTHON_CURRENT_VERSION_NO_DOTS} python${PYTHON_CURRENT_VERSION}
	      PATH_SUFFIXES python${PYTHON_CURRENT_VERSION}/config)

set (COMPIZCONFIG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/compizconfig/libcompizconfig/include)

string (REPLACE "\n" "" PROC_ARCH ${PROC_ARCH})
string (REPLACE "\n" "" PY_VERSION ${PY_VERSION})

set (PY_BUILD_DIR lib.linux-${PROC_ARCH}-${PY_VERSION})
set (PY_SCRIPTS_DIR scripts-${PY_VERSION})
set (PY_COMPIZCONFIG_SRC ${CMAKE_CURRENT_BINARY_DIR}/compizconfig.c)
set (PY_COMPIZCONFIG_PYREX ${CMAKE_CURRENT_SOURCE_DIR}/src/compizconfig.pyx)

find_program(PYREXC_BIN pyrexc)
if(NOT PYREXC_BIN)
  message(FATAL_ERROR "Can not build because pyrexc binary not found.")
endif()

add_custom_command (OUTPUT ${PY_COMPIZCONFIG_SRC}
		    COMMAND ${PYREXC_BIN} -o ${PY_COMPIZCONFIG_SRC} ${PY_COMPIZCONFIG_PYREX}
		    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		    DEPENDS ${PY_COMPIZCONFIG_PYREX})

file (READ ${CMAKE_SOURCE_DIR}/VERSION COMPIZ_RELEASE_VERSION LIMIT 12 OFFSET 0)
string (STRIP ${COMPIZ_RELEASE_VERSION} COMPIZ_RELEASE_VERSION)
set (VERSION ${COMPIZ_RELEASE_VERSION})

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/compizconfig-python.pc.in
                ${CMAKE_CURRENT_BINARY_DIR}/compizconfig-python.pc)

include_directories (${PYTHON_INCLUDE_DIRS}
		     ${COMPIZCONFIG_INCLUDE_DIRS})

# pyrex generates code that does type-punning, this trips
# up -Werror
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-strict-aliasing")

# We need to remove the "lib"
set (CMAKE_SHARED_LIBRARY_PREFIX "")

# We must build the python module using CMake instead of distutils.
#
# The only way in which the python module can be built using distutils
# is by using add_custom_command to "generate" it into existence.
# Unfortunately this has drawbacks. Namely, it is not possible for
# add_custom_command to depend on library targets. Because of that, there's
# no way that we can guarantee that libcompizconfig.so will be linked
# before compizconfig.so (which is required). The best that we can do
# there is to use add_custom_target, and make that target depend on the
# file, 
add_library (compizconfig_python_module SHARED
	     ${PY_COMPIZCONFIG_SRC})

set_target_properties (compizconfig_python_module
		       PROPERTIES
		       OUTPUT_NAME
		       compizconfig)

target_link_libraries (compizconfig_python_module
		       ${PYTHON_LIBRARY_LOCAL}
		       compizconfig)

set (UNINSTALL_COMMAND "python ${CMAKE_CURRENT_SOURCE_DIR}/setup.py uninstall --prefix=${CMAKE_INSTALL_PREFIX} --version=${VERSION}")

compiz_add_code_to_uninstall_target (${UNINSTALL_COMMAND} ${CMAKE_CURRENT_BINARY_DIR})

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/compizconfig-python.pc
         DESTINATION ${libdir}/pkgconfig)

# This is probably not very portable, but we can't install using distutils
# as we cannot build with it safely

if ($ENV{COMPIZ_DEB_BUILD})
    set (INSTALL_LAYOUT_DIR dist-packages)
else ($ENV{COMPIZ_DEB_BUILD})
    set (INSTALL_LAYOUT_DIR site-packages)
endif ($ENV{COMPIZ_DEB_BUILD})

set (PYTHON_MODULE_DIR ${libdir}/python${PY_VERSION}/${INSTALL_LAYOUT_DIR})

install (TARGETS compizconfig_python_module
	 DESTINATION ${PYTHON_MODULE_DIR})

install (CODE
	 "message (\"Installing python files\")
	  execute_process (COMMAND cmake -DSETUP=${CMAKE_CURRENT_SOURCE_DIR}/setup.py -DPREFIX=${CMAKE_INSTALL_PREFIX} -DWDIR=${CMAKE_CURRENT_BINARY_DIR} -DVERSION=${VERSION} -P ${compiz_SOURCE_DIR}/compizconfig/cmake/exec_setup_py_with_destdir.cmake)")

if (COMPIZ_BUILD_TESTING)

        find_program (BASH_EXECUTABLE bash)
	find_program (PYTHON_EXECUTABLE python)
	mark_as_advanced (FORCE PYTHON_EXECUTABLE)

	file (GLOB PYTHON_TESTS "tests/test_*.py")

        if (PYTHON_EXECUTABLE AND
            BASH_EXECUTABLE)
		set (MOCK_XML_SRC ${CMAKE_CURRENT_SOURCE_DIR}/tests/mock.xml)
		set (MOCK_XML_FILE ${CMAKE_CURRENT_BINARY_DIR}/mock.xml)

		add_custom_command (OUTPUT ${MOCK_XML_FILE}
				    COMMAND cp ${MOCK_XML_SRC}
					    ${MOCK_XML_FILE}
				    COMMENT "Copying mock.xml file into testing directory"
				    DEPENDS ${MOCK_XML_SRC})

		add_custom_target (compizconfig_python_test_mock_xml_file ALL DEPENDS
				   ${MOCK_XML_FILE})

		# For testing purposes we need the ini backend
		add_dependencies (compizconfig_python_module ini compizconfig_python_test_mock_xml_file)

		foreach (PYTHON_TEST_FILE ${PYTHON_TESTS})
			get_filename_component (PYTHON_TEST ${PYTHON_TEST_FILE} NAME_WE)
			message (STATUS "Adding test " ${PYTHON_TEST})
                        add_test (NAME CompizConfigPython.${PYTHON_TEST}
                                  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                                  COMMAND ${BASH_EXECUTABLE}
                                  ${CMAKE_SOURCE_DIR}/tests/util/run_with_ld_library_path.sh
                                  ${CMAKE_BINARY_DIR}/compizconfig/libcompizconfig/src
                                  ${PYTHON_EXECUTABLE} ${PYTHON_TEST_FILE})
		endforeach (PYTHON_TEST_FILE)
        endif (PYTHON_EXECUTABLE AND
               BASH_EXECUTABLE)

endif (COMPIZ_BUILD_TESTING)
