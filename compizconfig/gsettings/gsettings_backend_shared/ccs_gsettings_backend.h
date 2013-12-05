#ifndef _COMPIZCONFIG_CCS_GSETTINGS_BACKEND_H
#define _COMPIZCONFIG_CCS_GSETTINGS_BACKEND_H

#include <ccs-defs.h>
#include <ccs-backend.h>
#include <ccs_gsettings_backend_fwd.h>
#include <ccs_gnome_fwd.h>
#include <glib.h>

COMPIZCONFIG_BEGIN_DECLS

Bool
ccsGSettingsBackendAttachNewToBackend (CCSBackend                 *backend,
				       CCSContext                 *context,
				       CCSGSettingsWrapper        *compizconfigSettings,
				       CCSGSettingsWrapper        *currentProfileSettings,
				       CCSGSettingsWrapperFactory *wrapperFactory,
				       CCSIntegration             *integration,
				       CCSGNOMEValueChangeData    *valueChangeData,
				       char                       *currentProfile);

void
ccsGSettingsBackendDetachFromBackend (CCSBackend *backend);

/* Default implementations, should be moved */

void
ccsGSettingsBackendUpdateCurrentProfileNameDefault (CCSBackend *backend, const char *profile);

gboolean
ccsGSettingsBackendUpdateProfileDefault (CCSBackend *backend, CCSContext *context);

void
ccsGSettingsBackendUnsetAllChangedPluginKeysInProfileDefault (CCSBackend *backend,
							      CCSContext *context,
							      GVariant *pluginsWithChangedKeys,
							      const char * profile);

gboolean ccsGSettingsBackendAddProfileDefault (CCSBackend *backend,
					       const char *profile);

void ccsGSettingsSetIntegration (CCSBackend *backend,
				 CCSIntegration *integration);

COMPIZCONFIG_END_DECLS

#endif
