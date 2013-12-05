#ifndef _CCS_GNOME_GCONF_INTEGRATED_SETTING_H
#define _CCS_GNOME_GCONF_INTEGRATED_SETTING_H

#include <ccs-defs.h>
#include <ccs-fwd.h>
#include <ccs_gnome_fwd.h>
#include <gconf/gconf-client.h>

COMPIZCONFIG_BEGIN_DECLS

/**
 * @brief ccsGConfIntegratedSettingNew
 * @param base a CCSGNOMEIntegratedSetting
 * @param client a GConfClient
 * @param section the preceeding path to the keyname
 * @param ai a CCSObjectAllocationInterface
 * @return
 *
 * Creates the GConf implementation of a CCSIntegratedSetting, which will
 * write to GConf keys when necessary.
 */
CCSIntegratedSetting *
ccsGConfIntegratedSettingNew (CCSGNOMEIntegratedSettingInfo *base,
			      GConfClient		*client,
			      const char		*section,
			      CCSObjectAllocationInterface *ai);

COMPIZCONFIG_END_DECLS

#endif
