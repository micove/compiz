set (_headers
    compiz.h
    decoration.h
)

install (
    FILES ${_headers}
    DESTINATION ${COMPIZ_DESTDIR}${includedir}/compiz
)

add_subdirectory (core)
