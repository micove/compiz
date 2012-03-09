set (
    COMPIZ_DEFAULT_PLUGINS "core" CACHE STRING
    "Default plugins list that will be added to core metadata file"
)

find_program (XSLTPROC_EXECUTABLE xsltproc)
mark_as_advanced (FORCE XSLTPROC_EXECUTABLE)

if (XSLTPROC_EXECUTABLE)

    add_custom_command (
	OUTPUT ${compiz_BINARY_DIR}/generated/core.xml.in
	COMMAND ${XSLTPROC_EXECUTABLE}
		-o ${compiz_BINARY_DIR}/generated/core.xml.in
		--param default_plugins "\"'${COMPIZ_DEFAULT_PLUGINS}'\""
		${CMAKE_SOURCE_DIR}/xslt/default_plugins.xslt
		${CMAKE_CURRENT_SOURCE_DIR}/core.xml.in
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/core.xml.in
    )
    set (_core_file "${CMAKE_BINARY_DIR}/generated/core.xml.in")
else ()
    set (_core_file "${CMAKE_CURRENT_SOURCE_DIR}/core.xml.in")
endif ()


set (_xml_targets)
set (_schema_targets)

compiz_translate_xml (
    ${_core_file}
    ${CMAKE_BINARY_DIR}/generated/core.xml
)

install (
    FILES ${CMAKE_BINARY_DIR}/generated/core.xml
    DESTINATION ${COMPIZ_DESTDIR}${compiz_metadatadir}
)

add_custom_target (core-xml-file ALL
    DEPENDS ${CMAKE_BINARY_DIR}/generated/core.xml
)

if (USE_GCONF)
    compiz_gconf_schema (
	${CMAKE_BINARY_DIR}/generated/core.xml
	${CMAKE_BINARY_DIR}/generated/compiz-core.schemas
	${SCHEMADIR}
    )
    add_custom_target (core-gconf-schema ALL
	DEPENDS ${CMAKE_BINARY_DIR}/generated/compiz-core.schemas
    )
endif ()
if (USE_GSETTINGS)
    compiz_gsettings_schema (
	${CMAKE_BINARY_DIR}/generated/core.xml
	${CMAKE_BINARY_DIR}/generated/org.freedesktop.compiz.gschema.xml
	${datadir}/glib-2.0/schemas
    )
    add_custom_target (core-settings-schema ALL
	DEPENDS ${CMAKE_BINARY_DIR}/generated/org.freedesktop.compiz.gschema.xml
    )
endif ()
