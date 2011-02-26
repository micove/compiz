find_package (Compiz REQUIRED)

include (CompizPlugin)
include (CompizCommon)

compiz_plugin(decor PLUGINDEPS composite opengl LIBRARIES decoration)

if (COMPIZ_BUILD_WITH_RPATH AND NOT COMPIZ_DISABLE_PLUGIN_DECOR)

    set_target_properties (
	decor PROPERTIES
	INSTALL_RPATH "${COMPIZ_LIBDIR}"
    )
endif (COMPIZ_BUILD_WITH_RPATH AND NOT COMPIZ_DISABLE_PLUGIN_DECOR)
