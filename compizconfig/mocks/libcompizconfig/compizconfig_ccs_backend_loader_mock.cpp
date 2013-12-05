/*
 * Compiz configuration system library
 *
 * Copyright (C) 2013 Sam Spilsbury.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authored By:
 * Sam Spilsbury <smspillaz@gmail.com>
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ccs-object.h>
#include <ccs_backend_loader_interface.h>
#include "compizconfig_ccs_backend_loader_mock.h"

const CCSBackendLoaderInterface ccsMockBackendLoaderInterface =
{
    CCSBackendLoaderGMock::ccsBackendLoaderLoadBackend,
    CCSBackendLoaderGMock::ccsFreeBackendLoader
};

static void
finalizeAndFreeBackendLoader (CCSBackendLoader *loader)
{
    CCSObjectAllocationInterface *ai = loader->object.object_allocation;

    ccsObjectFinalize (loader);
    (*ai->free_) (ai->allocator, loader);
}

void
ccsFreeMockBackendLoader (CCSBackendLoader *loader)
{
    CCSBackendLoaderGMock *gmock = reinterpret_cast <CCSBackendLoaderGMock *> (ccsObjectGetPrivate (loader));
    delete gmock;

    ccsObjectSetPrivate (loader, NULL);
    finalizeAndFreeBackendLoader (loader);

}

static CCSBackendLoaderGMock *
newCCSBackendLoaderGMockInterface (CCSBackendLoader *loader)
{
    CCSBackendLoaderGMock *gmock = new CCSBackendLoaderGMock ();

    if (!gmock)
    {
	finalizeAndFreeBackendLoader (loader);
	return NULL;
    }

    return gmock;
}

static CCSBackendLoader *
allocateCCSBackendLoader (CCSObjectAllocationInterface *ai)
{
    CCSBackendLoader *loader = reinterpret_cast <CCSBackendLoader *> ((*ai->calloc_) (ai->allocator, 1, sizeof (CCSBackendLoader)));

    if (!loader)
	return NULL;

    ccsObjectInit (loader, ai);

    return loader;
}

CCSBackendLoader *
ccsMockBackendLoaderNew (CCSObjectAllocationInterface *ai)
{
    CCSBackendLoader *loader = allocateCCSBackendLoader (ai);

    if (!loader)
	return NULL;

    CCSBackendLoaderGMock *gmock = newCCSBackendLoaderGMockInterface (loader);

    if (!gmock)
	return NULL;

    ccsObjectSetPrivate (loader, (CCSPrivate *) gmock);
    ccsObjectAddInterface (loader,
			   (const CCSInterface *) &ccsMockBackendLoaderInterface,
			   GET_INTERFACE_TYPE (CCSBackendLoaderInterface));
    ccsObjectRef (loader);

    return loader;
}
