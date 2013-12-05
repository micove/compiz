/**
 *
 * GSettings libcompizconfig backend
 *
 * ccs_gsettings_wrapper_factory_interface.c
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
#include <ccs-object.h>
#include "ccs_gsettings_wrapper_factory_interface.h"

INTERFACE_TYPE (CCSGSettingsWrapperFactoryInterface);

CCSGSettingsWrapper *
ccsGSettingsWrapperFactoryNewGSettingsWrapper (CCSGSettingsWrapperFactory   *factory,
					       const gchar                  *schemaName,
					       CCSObjectAllocationInterface *ai)
{
    return (*(GET_INTERFACE (CCSGSettingsWrapperFactoryInterface, factory))->newGSettingsWrapper) (factory, schemaName, ai);
}

CCSGSettingsWrapper *
ccsGSettingsWrapperFactoryNewGSettingsWrapperWithPath (CCSGSettingsWrapperFactory   *factory,
						       const gchar                  *schemaName,
						       const gchar                  *path,
						       CCSObjectAllocationInterface *ai)
{
    return (*(GET_INTERFACE (CCSGSettingsWrapperFactoryInterface, factory))->newGSettingsWrapperWithPath) (factory, schemaName, path, ai);
}

void
ccsFreeGSettingsWrapperFactory (CCSGSettingsWrapperFactory *factory)
{
    return (*(GET_INTERFACE (CCSGSettingsWrapperFactoryInterface, factory))->free) (factory);
}

CCSREF_OBJ (GSettingsWrapperFactory, CCSGSettingsWrapperFactory)
