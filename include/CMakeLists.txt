set (_headers
    decoration.h
)

install (
    FILES ${_headers}
    DESTINATION ${COMPIZ_DESTDIR}${includedir}/compiz
)

add_subdirectory (core)
