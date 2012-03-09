
set (_images
    icon.png
    freedesktop.png
)

install (
    FILES ${_images}
    DESTINATION ${COMPIZ_DESTDIR}${compiz_imagedir}
)
