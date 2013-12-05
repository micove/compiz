/**
 *
 * GSettings libcompizconfig backend
 *
 * ccs_gnome_integration_gsettings_wrapper_factory.c
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
#include "ccs_gnome_integration_gsettings_wrapper_factory.h"

/* CCSGSettingsWrapperFactory implementation */
typedef struct _CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate;
struct _CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate
{
    CCSGSettingsWrapperFactory *wrapperFactory;
    GCallback                  callback;

    /* This is expected to stay alive during the
     * lifetime of this object */
    CCSGNOMEValueChangeData    *data;
};

static void
ccsGNOMEIntegrationGSettingsWrapperFree (CCSGSettingsWrapperFactory *wrapperFactory)
{
    CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate *priv =
	    GET_PRIVATE (CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate, wrapperFactory);

    ccsGSettingsWrapperFactoryUnref (priv->wrapperFactory);

    ccsObjectFinalize (wrapperFactory);
    (*wrapperFactory->object.object_allocation->free_) (wrapperFactory->object.object_allocation->allocator,
							wrapperFactory);
}

static void
connectWrapperToChangedSignal (CCSGSettingsWrapper                               *wrapper,
			       CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate *priv)
{
    ccsGSettingsWrapperConnectToChangedSignal (wrapper, priv->callback, priv->data);
}

static CCSGSettingsWrapper *
ccsGNOMEIntegrationGSettingsWrapperFactoryNewGSettingsWrapper (CCSGSettingsWrapperFactory   *factory,
							       const gchar                  *schemaName,
							       CCSObjectAllocationInterface *ai)
{
    CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate *priv =
	    GET_PRIVATE (CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate, factory);
    CCSGSettingsWrapper *wrapper = ccsGSettingsWrapperFactoryNewGSettingsWrapper (priv->wrapperFactory,
										  schemaName,
										  factory->object.object_allocation);

    if (wrapper != NULL)
	connectWrapperToChangedSignal (wrapper, priv);

    return wrapper;
}

static CCSGSettingsWrapper *
ccsGNOMEIntegrationGSettingsWrapperFactoryNewGSettingsWrapperWithPath (CCSGSettingsWrapperFactory   *factory,
								       const gchar                  *schemaName,
								       const gchar                  *path,
								       CCSObjectAllocationInterface *ai)
{
    CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate *priv =
	    GET_PRIVATE (CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate, factory);
    CCSGSettingsWrapper *wrapper = ccsGSettingsWrapperFactoryNewGSettingsWrapperWithPath (priv->wrapperFactory,
											  schemaName,
											  path,
											  factory->object.object_allocation);

    connectWrapperToChangedSignal (wrapper, priv);

    return wrapper;
}

const CCSGSettingsWrapperFactoryInterface ccsGNOMEIntegrationGSettingsWrapperFactoryInterface =
{
    ccsGNOMEIntegrationGSettingsWrapperFactoryNewGSettingsWrapper,
    ccsGNOMEIntegrationGSettingsWrapperFactoryNewGSettingsWrapperWithPath,
    ccsGNOMEIntegrationGSettingsWrapperFree
};

CCSGSettingsWrapperFactory *
ccsGNOMEIntegrationGSettingsWrapperFactoryDefaultImplNew (CCSObjectAllocationInterface *ai,
							  CCSGSettingsWrapperFactory   *factory,
							  GCallback                    callback,
							  CCSGNOMEValueChangeData      *data)
{
    CCSGSettingsWrapperFactory *wrapperFactory = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSGSettingsWrapperFactory));

    if (!wrapperFactory)
	return NULL;

    CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate *priv = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSGNOMEIntegrationGSettingsWrapperFactoryPrivate));

    if (!priv)
    {
	(*ai->free_) (ai->allocator, wrapperFactory);
	return NULL;
    }

    ccsGSettingsWrapperFactoryRef (factory);

    priv->wrapperFactory = factory;
    priv->callback = callback;
    priv->data = data;

    ccsObjectInit (wrapperFactory, ai);
    ccsObjectAddInterface (wrapperFactory, (const CCSInterface *) &ccsGNOMEIntegrationGSettingsWrapperFactoryInterface, GET_INTERFACE_TYPE (CCSGSettingsWrapperFactoryInterface));
    ccsObjectSetPrivate (wrapperFactory, (CCSPrivate *) priv);

    ccsGSettingsWrapperFactoryRef (wrapperFactory);

    return wrapperFactory;
}
