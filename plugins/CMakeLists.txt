set (COMPIZ_CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${COMPIZ_CMAKE_MODULE_PATH})
include (CompizDefaults)
include (CompizCommon)

set (COMPIZ_FOUND "true")

add_definitions ( -DHAVE_CONFIG_H)

compiz_add_plugins_in_folder (${CMAKE_CURRENT_SOURCE_DIR})
