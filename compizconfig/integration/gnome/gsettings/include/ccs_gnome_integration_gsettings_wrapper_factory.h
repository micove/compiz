/**
 *
 * GSettings libcompizconfig backend
 *
 * ccs_gsettings_wrapper_factory.h
 *
 * Copyright (c) 2012 Canonical Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Authored By:
 *	Sam Spilsbury <sam.spilsbury@canonical.com>
 *
 **/
#ifndef _CCS_GNOME_INTEGRATION_GSETTINGS_WRAPPER_FACTORY_H
#define _CCS_GNOME_INTEGRATION_GSETTINGS_WRAPPER_FACTORY_H

#include <ccs-defs.h>

COMPIZCONFIG_BEGIN_DECLS

#include <gio/gio.h>

#include <ccs-fwd.h>
#include <ccs_gsettings_backend_fwd.h>
#include <ccs_gnome_fwd.h>

CCSGSettingsWrapperFactory *
ccsGNOMEIntegrationGSettingsWrapperFactoryDefaultImplNew (CCSObjectAllocationInterface *ai,
							  CCSGSettingsWrapperFactory   *factory,
							  GCallback                    callback,
							  CCSGNOMEValueChangeData      *data);

COMPIZCONFIG_END_DECLS

#endif
