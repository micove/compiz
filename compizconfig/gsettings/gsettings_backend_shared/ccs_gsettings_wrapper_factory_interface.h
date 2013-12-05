/**
 *
 * GSettings libcompizconfig backend
 *
 * ccs_gsettings_wrapper_factory_interface.h
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

#ifndef _CCS_GSETTINGS_WRAPPER_FACTORY_INTERFACE_H
#define _CCS_GSETTINGS_WRAPPER_FACTORY_INTERFACE_H

#include <ccs-defs.h>

COMPIZCONFIG_BEGIN_DECLS

#include <gio/gio.h>
#include <ccs-fwd.h>
#include <ccs-object.h>
#include <ccs_gsettings_backend_fwd.h>

typedef struct _CCSGSettingsWrapperFactoryInterface CCSGSettingsWrapperFactoryInterface;

typedef CCSGSettingsWrapper * (*CCSGSettingsWrapperFactoryNewGSettingsWrapper) (CCSGSettingsWrapperFactory   *wrapperFactory,
										const gchar                  *schema,
										CCSObjectAllocationInterface *ai);
typedef CCSGSettingsWrapper * (*CCSGSettingsWrapperFactoryNewGSettingsWrapperWithPath) (CCSGSettingsWrapperFactory   *wrapperFactory,
											const gchar                  *schema,
											const gchar                  *path,
											CCSObjectAllocationInterface *ai);

typedef void (*CCSGSettingsWrapperFactoryFree) (CCSGSettingsWrapperFactory *wrapperFactory);

struct _CCSGSettingsWrapperFactoryInterface
{
    CCSGSettingsWrapperFactoryNewGSettingsWrapper newGSettingsWrapper;
    CCSGSettingsWrapperFactoryNewGSettingsWrapperWithPath newGSettingsWrapperWithPath;
    CCSGSettingsWrapperFactoryFree free;
};

/**
 * @brief The _CCSGSettingsWrapperFactory struct
 *
 * Will create new CCSGSettingsIntegratedSetting objects on demand
 */
struct _CCSGSettingsWrapperFactory
{
    CCSObject object;
};

unsigned int ccsCCSGSettingsWrapperFactoryInterfaceGetType ();

CCSREF_HDR (GSettingsWrapperFactory, CCSGSettingsWrapperFactory);

CCSGSettingsWrapper *
ccsGSettingsWrapperFactoryNewGSettingsWrapper (CCSGSettingsWrapperFactory   *wrapperFactory,
					       const gchar                  *schema,
					       CCSObjectAllocationInterface *ai);

CCSGSettingsWrapper *
ccsGSettingsWrapperFactoryNewGSettingsWrapperWithPath (CCSGSettingsWrapperFactory   *factory,
						       const gchar                  *schemaName,
						       const gchar                  *path,
						       CCSObjectAllocationInterface *ai);

COMPIZCONFIG_END_DECLS

#endif
