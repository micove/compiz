/**
 *
 * GSettings libcompizconfig backend
 *
 * ccs_gsettings_wrapper_factory.c
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
#include "ccs_gsettings_wrapper_factory.h"
#include "ccs_gsettings_wrapper_factory_interface.h"
#include "ccs_gsettings_interface.h"
#include "ccs_gsettings_interface_wrapper.h"

/* CCSGSettingsWrapperFactory implementation */
typedef struct _CCSGSettingsWrapperFactoryPrivate CCSGSettingsWrapperFactoryPrivate;
struct _CCSGSettingsWrapperFactoryPrivate
{
};

static void
ccsGSettingsWrapperDefaultImplFree (CCSGSettingsWrapperFactory *wrapperFactory)
{
    ccsObjectFinalize (wrapperFactory);
    (*wrapperFactory->object.object_allocation->free_) (wrapperFactory->object.object_allocation->allocator,
							wrapperFactory);
}

static CCSGSettingsWrapper *
ccsGSettingsWrapperFactoryNewGSettingsWrapperDefault (CCSGSettingsWrapperFactory   *factory,
						      const gchar                  *schemaName,
						      CCSObjectAllocationInterface *ai)
{
    CCSGSettingsWrapper *wrapper = ccsGSettingsWrapperNewForSchema (schemaName, ai);

    return wrapper;
}

CCSGSettingsWrapper *
ccsGSettingsWrapperFactoryNewGSettingsWrapperWithPathDefault (CCSGSettingsWrapperFactory   *wrapperFactory,
							      const gchar                  *schemaName,
							      const gchar                  *path,
							      CCSObjectAllocationInterface *ai)
{
    CCSGSettingsWrapper *wrapper = ccsGSettingsWrapperNewForSchemaWithPath (schemaName,
									    path,
									    ai);

    return wrapper;
}

const CCSGSettingsWrapperFactoryInterface ccsGSettingsWrapperFactoryInterface =
{
    ccsGSettingsWrapperFactoryNewGSettingsWrapperDefault,
    ccsGSettingsWrapperFactoryNewGSettingsWrapperWithPathDefault,
    ccsGSettingsWrapperDefaultImplFree
};

CCSGSettingsWrapperFactory *
ccsGSettingsWrapperFactoryDefaultImplNew (CCSObjectAllocationInterface *ai)
{
    CCSGSettingsWrapperFactory *wrapperFactory = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSGSettingsWrapperFactory));

    if (!wrapperFactory)
	return NULL;

    CCSGSettingsWrapperFactoryPrivate *priv = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSGSettingsWrapperFactoryPrivate));

    if (!priv)
    {
	(*ai->free_) (ai->allocator, wrapperFactory);
	return NULL;
    }

    ccsObjectInit (wrapperFactory, ai);
    ccsObjectAddInterface (wrapperFactory, (const CCSInterface *) &ccsGSettingsWrapperFactoryInterface, GET_INTERFACE_TYPE (CCSGSettingsWrapperFactoryInterface));
    ccsObjectSetPrivate (wrapperFactory, (CCSPrivate *) priv);

    ccsGSettingsWrapperFactoryRef (wrapperFactory);

    return wrapperFactory;
}
