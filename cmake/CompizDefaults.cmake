set (COMPIZ_PREFIX ${CMAKE_INSTALL_PREFIX})
set (COMPIZ_INCLUDEDIR ${includedir})
set (COMPIZ_CORE_INCLUDE_DIR ${includedir}/compiz/core)
set (COMPIZ_LIBDIR ${libdir})

list (APPEND COMPIZ_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include)
list (APPEND COMPIZ_INCLUDE_DIRS ${CMAKE_BINARY_DIR})

set (COMPIZ_BCOP_XSLT ${CMAKE_SOURCE_DIR}/xslt/bcop.xslt)

set (COMPIZ_GCONF_SCHEMAS_SUPPORT ${USE_GCONF})
set (COMPIZ_GCONF_SCHEMAS_XSLT ${CMAKE_SOURCE_DIR}/xslt/compiz_gconf_schemas.xslt)
set (COMPIZ_GSETTINGS_SCHEMAS_XSLT ${CMAKE_BINARY_DIR}/xslt/compiz_gsettings_schemas.xslt)

set (COMPIZ_PLUGIN_INSTALL_TYPE "package")

set (_COMPIZ_INTERNAL 1)
