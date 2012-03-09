compiz_configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/CompizDefaults.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CompizDefaults.cmake
)

set (_files
    CompizCommon.cmake
    CompizPlugin.cmake
    CompizPackage.cmake
    CompizBcop.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CompizDefaults.cmake
)

list (APPEND _PluginExtensionFiles
      plugin_extensions/CompizGenInstallData.cmake)
list (APPEND _PluginExtensionFiles
      plugin_extensions/CompizGenInstallImages.cmake)

if (USE_GCONF)
    list (APPEND _files CompizGconf.cmake)
    list (APPEND _PluginExtensionFiles plugin_extensions/CompizGenGconf.cmake)
endif (USE_GCONF)

install (
    FILES ${_files}
    DESTINATION ${COMPIZ_DESTDIR}${CMAKE_INSTALL_PREFIX}/share/compiz/cmake
)

install (
    FILES ${_PluginExtensionFiles}
    DESTINATION ${COMPIZ_DESTDIR}${CMAKE_INSTALL_PREFIX}/share/compiz/cmake/plugin_extensions
)
