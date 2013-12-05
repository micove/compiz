#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

#include <ccs.h>
#include <ccs-backend.h>
#include <ccs-object.h>

#include <ccs_gsettings_interface.h>
#include <ccs_gsettings_interface_wrapper.h>

#include <gsettings_util.h>
#include <ccs_gsettings_wrapper_factory_interface.h>

#include "ccs_gnome_integration.h"
#include "ccs_gnome_integrated_setting.h"
#include "ccs_gnome_integration_constants.h"
#include "ccs_gnome_integration_types.h"
#include "ccs_gnome_integration_gsettings_integrated_setting.h"
#include "ccs_gnome_integration_gsettings_integrated_setting_factory.h"

char *
ccsGSettingsIntegratedSettingsTranslateNewGNOMEKeyForCCS (const char *key)
{
    char *newKey = translateKeyForCCS (key);

    if (g_strcmp0 (newKey, "screenshot") == 0)
    {
	free (newKey);
	newKey = strdup ("run_command_screenshot");
    }
    else if (g_strcmp0 (newKey, "window_screenshot") == 0)
    {
	free (newKey);
	newKey = strdup ("run_command_window_screenshot");
    }
    else if (g_strcmp0 (newKey, "terminal") == 0)
    {
	free (newKey);
	newKey = strdup ("run_command_terminal");
    }

    return newKey;
}

typedef struct _CCSGSettingsIntegratedSettingFactoryPrivate CCSGSettingsIntegratedSettingFactoryPrivate;

struct _CCSGSettingsIntegratedSettingFactoryPrivate
{
    CCSGSettingsWrapperFactory *wrapperFactory;
    GHashTable  *pluginsToSettingsGSettingsWrapperQuarksHashTable;
    GHashTable  *quarksToGSettingsWrappersHashTable;
    GHashTable  *pluginsToSettingsSpecialTypesHashTable;
    GHashTable  *pluginsToSettingNameGNOMENameHashTable;
    CCSGNOMEValueChangeData *valueChangeData;
};

static void
gnomeGSettingsValueChanged (GSettings *settings,
			    gchar     *key,
			    gpointer  user_data)
{
    CCSGNOMEValueChangeData *data = (CCSGNOMEValueChangeData *) user_data;
    char *baseName = ccsGSettingsIntegratedSettingsTranslateNewGNOMEKeyForCCS (key);

    /* We don't care if integration is not enabled */
    if (!ccsGetIntegrationEnabled (data->context))
	return;

    CCSIntegratedSettingList settingList = ccsIntegratedSettingsStorageFindMatchingSettingsByPredicate (data->storage,
													ccsGNOMEIntegrationFindSettingsMatchingPredicate,
													baseName);

    ccsIntegrationUpdateIntegratedSettings (data->integration,
					    data->context,
					    settingList);

    g_free (baseName);

}

GCallback
ccsGSettingsIntegratedSettingsChangeCallback ()
{
    return (GCallback) gnomeGSettingsValueChanged;
}

static CCSIntegratedSetting *
createNewGSettingsIntegratedSetting (CCSGSettingsWrapper *wrapper,
				     const char  *gnomeName,
				     const char  *pluginName,
				     const char  *settingName,
				     CCSSettingType type,
				     SpecialOptionType specialOptionType,
				     CCSObjectAllocationInterface *ai)
{
    CCSIntegratedSettingInfo *sharedIntegratedSettingInfo = ccsSharedIntegratedSettingInfoNew (pluginName, settingName, type, ai);

    if (!sharedIntegratedSettingInfo)
	return NULL;

    CCSGNOMEIntegratedSettingInfo *gnomeIntegratedSettingInfo = ccsGNOMEIntegratedSettingInfoNew (sharedIntegratedSettingInfo, specialOptionType, gnomeName, ai);

    if (!gnomeIntegratedSettingInfo)
    {
	ccsIntegratedSettingInfoUnref (sharedIntegratedSettingInfo);
	return NULL;
    }

    CCSIntegratedSetting *gsettingsIntegratedSetting = ccsGSettingsIntegratedSettingNew (gnomeIntegratedSettingInfo, wrapper, ai);

    if (!gsettingsIntegratedSetting)
    {
	ccsIntegratedSettingInfoUnref ((CCSIntegratedSettingInfo *) gnomeIntegratedSettingInfo);
	return NULL;
    }

    return gsettingsIntegratedSetting;
}

static void
ccsGSettingsWrapperUnrefWrapper (gpointer wrapper)
{
    ccsGSettingsWrapperUnref ((CCSGSettingsWrapper *) wrapper);
}

static GHashTable *
initializeGSettingsWrappers (CCSGSettingsWrapperFactory *factory)
{
    const CCSGSettingsWrapperIntegratedSchemasQuarks *quarks = ccsGNOMEGSettingsWrapperQuarks ();
    GHashTable *quarksToGSettingsWrappers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, ccsGSettingsWrapperUnrefWrapper);

    g_hash_table_insert (quarksToGSettingsWrappers, GINT_TO_POINTER (quarks->ORG_COMPIZ_INTEGRATED),
			 ccsGSettingsWrapperFactoryNewGSettingsWrapper (factory,
									g_quark_to_string (quarks->ORG_COMPIZ_INTEGRATED),
									factory->object.object_allocation));
    g_hash_table_insert (quarksToGSettingsWrappers, GINT_TO_POINTER (quarks->ORG_GNOME_DESKTOP_WM_KEYBINDINGS),
			 ccsGSettingsWrapperFactoryNewGSettingsWrapper (factory,
									g_quark_to_string (quarks->ORG_GNOME_DESKTOP_WM_KEYBINDINGS),
									factory->object.object_allocation));
    g_hash_table_insert (quarksToGSettingsWrappers, GINT_TO_POINTER (quarks->ORG_GNOME_DESKTOP_WM_PREFERENCES),
			 ccsGSettingsWrapperFactoryNewGSettingsWrapper (factory,
									g_quark_to_string (quarks->ORG_GNOME_DESKTOP_WM_PREFERENCES),
									factory->object.object_allocation));
    g_hash_table_insert (quarksToGSettingsWrappers, GINT_TO_POINTER (quarks->ORG_GNOME_DESKTOP_DEFAULT_APPLICATIONS_TERMINAL),
			 ccsGSettingsWrapperFactoryNewGSettingsWrapper (factory,
									g_quark_to_string (quarks->ORG_GNOME_DESKTOP_DEFAULT_APPLICATIONS_TERMINAL),
									factory->object.object_allocation));
    g_hash_table_insert (quarksToGSettingsWrappers, GINT_TO_POINTER (quarks->ORG_GNOME_SETTINGS_DAEMON_PLUGINS_MEDIA_KEYS),
			 ccsGSettingsWrapperFactoryNewGSettingsWrapper (factory,
									g_quark_to_string (quarks->ORG_GNOME_SETTINGS_DAEMON_PLUGINS_MEDIA_KEYS),
									factory->object.object_allocation));

    return quarksToGSettingsWrappers;
}

CCSIntegratedSetting *
ccsGSettingsIntegratedSettingFactoryCreateIntegratedSettingForCCSSettingNameAndType (CCSIntegratedSettingFactory *factory,
										     CCSIntegration              *integration,
										     const char                  *pluginName,
										     const char                  *settingName,
										     CCSSettingType              type)
{
    CCSGSettingsIntegratedSettingFactoryPrivate *priv = (CCSGSettingsIntegratedSettingFactoryPrivate *) ccsObjectGetPrivate (factory);
    GHashTable                              *settingsGSettingsWrapperQuarksHashTable = g_hash_table_lookup (priv->pluginsToSettingsGSettingsWrapperQuarksHashTable, pluginName);
    GHashTable                              *settingsSpecialTypesHashTable = g_hash_table_lookup (priv->pluginsToSettingsSpecialTypesHashTable, pluginName);
    GHashTable				    *settingsSettingNameGNOMENameHashTable = g_hash_table_lookup (priv->pluginsToSettingNameGNOMENameHashTable, pluginName);

    if (!priv->quarksToGSettingsWrappersHashTable)
	priv->quarksToGSettingsWrappersHashTable = initializeGSettingsWrappers (priv->wrapperFactory);

    if (settingsGSettingsWrapperQuarksHashTable &&
	settingsSpecialTypesHashTable &&
	settingsSettingNameGNOMENameHashTable)
    {
	GQuark  wrapperQuark = GPOINTER_TO_INT (g_hash_table_lookup (settingsGSettingsWrapperQuarksHashTable, settingName));
	CCSGSettingsWrapper *wrapper = g_hash_table_lookup (priv->quarksToGSettingsWrappersHashTable, GINT_TO_POINTER (wrapperQuark));
	SpecialOptionType specialType = (SpecialOptionType) GPOINTER_TO_INT (g_hash_table_lookup (settingsSpecialTypesHashTable, settingName));
	const gchar *integratedName = g_hash_table_lookup (settingsSettingNameGNOMENameHashTable, settingName);

	if (wrapper == NULL)
	    return NULL;

	return createNewGSettingsIntegratedSetting (wrapper,
						    integratedName,
						    pluginName,
						    settingName,
						    type,
						    specialType,
						    factory->object.object_allocation);
    }


    return NULL;
}

void
ccsGSettingsIntegratedSettingFactoryFree (CCSIntegratedSettingFactory *factory)
{
    CCSGSettingsIntegratedSettingFactoryPrivate *priv = (CCSGSettingsIntegratedSettingFactoryPrivate *) ccsObjectGetPrivate (factory);

    if (priv->pluginsToSettingsGSettingsWrapperQuarksHashTable)
	g_hash_table_unref (priv->pluginsToSettingsGSettingsWrapperQuarksHashTable);

    if (priv->quarksToGSettingsWrappersHashTable)
	g_hash_table_unref (priv->quarksToGSettingsWrappersHashTable);

    if (priv->pluginsToSettingsSpecialTypesHashTable)
	g_hash_table_unref (priv->pluginsToSettingsSpecialTypesHashTable);

    if (priv->pluginsToSettingNameGNOMENameHashTable)
	g_hash_table_unref (priv->pluginsToSettingNameGNOMENameHashTable);

    ccsGSettingsWrapperFactoryUnref (priv->wrapperFactory);

    ccsObjectFinalize (factory);
    (*factory->object.object_allocation->free_) (factory->object.object_allocation->allocator, factory);
}

const CCSIntegratedSettingFactoryInterface ccsGSettingsIntegratedSettingFactoryInterface =
{
    ccsGSettingsIntegratedSettingFactoryCreateIntegratedSettingForCCSSettingNameAndType,
    ccsGSettingsIntegratedSettingFactoryFree
};

CCSIntegratedSettingFactory *
ccsGSettingsIntegratedSettingFactoryNew (CCSGSettingsWrapperFactory   *wrapperFactory,
					 CCSGNOMEValueChangeData      *valueChangeData,
					 CCSObjectAllocationInterface *ai)
{
    CCSIntegratedSettingFactory *factory = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSIntegratedSettingFactory));

    if (!factory)
	return NULL;

    CCSGSettingsIntegratedSettingFactoryPrivate *priv = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSGSettingsIntegratedSettingFactoryPrivate));

    if (!priv)
    {
	(*ai->free_) (ai->allocator, factory);
	return NULL;
    }

    ccsGSettingsWrapperFactoryRef (wrapperFactory);

    priv->wrapperFactory = wrapperFactory;
    priv->pluginsToSettingsGSettingsWrapperQuarksHashTable = ccsGNOMEGSettingsIntegrationPopulateSettingNameToIntegratedSchemasQuarksHashTable ();
    priv->pluginsToSettingsSpecialTypesHashTable = ccsGNOMEIntegrationPopulateSpecialTypesHashTables ();
    priv->pluginsToSettingNameGNOMENameHashTable = ccsGNOMEIntegrationPopulateSettingNameToGNOMENameHashTables ();
    priv->valueChangeData = valueChangeData;

    ccsObjectInit (factory, ai);
    ccsObjectSetPrivate (factory, (CCSPrivate *) priv);
    ccsObjectAddInterface (factory, (const CCSInterface *) &ccsGSettingsIntegratedSettingFactoryInterface, GET_INTERFACE_TYPE (CCSIntegratedSettingFactoryInterface));

    ccsObjectRef (factory);

    return factory;
}

