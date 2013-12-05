set (COMPIZ_CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${COMPIZ_CMAKE_MODULE_PATH})
include (CompizDefaults)
include (CompizCommon)

set (COMPIZ_FOUND "true")

include_directories (${COMPIZ_INTERNAL_INCLUDES})

add_subdirectory (testhelper)
