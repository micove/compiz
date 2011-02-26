project (compiz)

set (CMAKE_MODULE_PATH_ORIG ${CMAKE_MODULE_PATH})
set (COMPIZ_CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${COMPIZ_CMAKE_MODULE_PATH})
include (CompizCommon)
include (CompizPackage)
include (cmake/base.cmake)
include (CheckFunctionExists)


set (CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRe" CACHE INTERNAL "" FORCE)
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug/Release/RelWithDebInfo/MinSizeRe)" FORCE)
endif (NOT CMAKE_BUILD_TYPE)

# compiz package version number
# An odd micro number indicates in-progress development.
# An even micro number indicates a released version.
set (COMPIZ_VERSION_MAJOR 0)
set (COMPIZ_VERSION_MINOR 9)
set (COMPIZ_VERSION_MICRO 2)
set (COMPIZ_VERSION_MACRO 1)
set (VERSION ${COMPIZ_VERSION_MAJOR}.${COMPIZ_VERSION_MINOR}.${COMPIZ_VERSION_MICRO}.${COMPIZ_VERSION_MACRO})

set (DECOR_INTERFACE_VERSION 20110130)

if (COMPIZ_PACKAGING_ENABLED)
    set (prefix ${CMAKE_INSTALL_PREFIX}                   CACHE PATH "prefix")
    set (exec_prefix ${CMAKE_INSTALL_PREFIX}/bin          CACHE PATH "bindir")
    set (libdir ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}  CACHE PATH "libdir")
    set (includedir ${CMAKE_INSTALL_PREFIX}/include       CACHE PATH "includedir")
    set (datadir ${CMAKE_INSTALL_PREFIX}/share            CACHE PATH "datadir")
else (COMPIZ_PACKAGING_ENABLED)
    set (prefix ${CMAKE_INSTALL_PREFIX}                 )
    set (exec_prefix ${CMAKE_INSTALL_PREFIX}/bin        )
    set (libdir ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})
    set (includedir ${CMAKE_INSTALL_PREFIX}/include     )
    set (datadir ${CMAKE_INSTALL_PREFIX}/share          )
endif (COMPIZ_PACKAGING_ENABLED)

set (compiz_plugindir ${libdir}/compiz)
set (compiz_imagedir ${datadir}/compiz/images)
set (compiz_metadatadir ${datadir}/compiz)
set (COMPIZ_I18N_DIR ${CMAKE_SOURCE_DIR}/po)

set (ALL_LINGUAS af ar bg bn bn_IN bs ca cs cy da de el en_GB en_US es eu et fi fr gl gu he hi hr hu id it ja ka km ko lo lt mk mr nb nl or pa pl pt pt_BR ro ru sk sl sr sv ta tr uk vi xh zh_CN zh_TW zu)
set (GETTEXT_PACKAGE compiz)

find_package (Boost 1.34.0 REQUIRED serialization)

set (COMPIZ_REQUIRES
    x11
    xext
    xdamage
    xcomposite
    x11-xcb
    xrandr
    xinerama
    xext
    ice
    sm
    libxml-2.0
    libxslt
    glib-2.0
    gio-2.0
    glibmm-2.4
    "libstartup-notification-1.0 >= 0.7"
)

set (DECORATION_REQUIRES xrender)

compiz_pkg_check_modules (COMPIZ REQUIRED ${COMPIZ_REQUIRES})
compiz_pkg_check_modules (LIBDECORATION REQUIRED ${DECORATION_REQUIRES})

list (APPEND COMPIZ_LIBRARIES ${Boost_LIBRARIES})

include (CompizDefaults)

set (COMPIZ_CFLAGS "${COMPIZ_CFLAGS} -I${Boost_INCLUDE_DIR}")

set (_compiz_package_string "Compiz ${VERSION}")
set (_compiz_package "Compiz")

compiz_configure_file (${compiz_SOURCE_DIR}/config.h.core.in ${compiz_BINARY_DIR}/generated/config.h)

compiz_configure_file (
    ${CMAKE_SOURCE_DIR}/include/compiz-common.h.in
    ${CMAKE_BINARY_DIR}/generated/compiz-common.h
)

install (
    FILES ${CMAKE_BINARY_DIR}/generated/compiz-common.h
    DESTINATION ${COMPIZ_DESTDIR}${includedir}/compiz
)

compiz_configure_file (
    ${CMAKE_SOURCE_DIR}/compiz.pc.in
    ${CMAKE_BINARY_DIR}/compiz.pc
    COMPIZ_REQUIRES
    COMPIZ_CFLAGS
)

install (
    FILES ${CMAKE_BINARY_DIR}/compiz.pc
    DESTINATION ${COMPIZ_DESTDIR}${libdir}/pkgconfig
)

add_subdirectory (cmake)
add_subdirectory (include)
add_subdirectory (images)
add_subdirectory (libdecoration)
add_subdirectory (gtk)
add_subdirectory (kde)
add_subdirectory (po)
add_subdirectory (metadata)
add_subdirectory (src)
add_subdirectory (xslt)
add_subdirectory (plugins)

compiz_ensure_linkage ()
compiz_package_generation ("Compiz")
compiz_add_uninstall ()
compiz_add_git_dist ()

_print_configure_results ()

_check_compiz_cmake_macro (${CMAKE_MODULE_PATH_ORIG})


