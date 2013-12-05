/*
 * Compiz configuration system library
 *
 * ccs_backend_loader.c
 *
 * Copyright (C) 2013 Sam Spilsbury
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <ccs-defs.h>
#include <ccs-object.h>
#include <ccs-backend.h>
#include <ccs-private.h>
#include <ccs_backend_loader_interface.h>
#include <ccs_backend_loader.h>

typedef struct _CCSShraedLibBackendLoaderPrivate CCSShraedLibBackendLoaderPrivate;

struct _CCSShraedLibBackendLoaderPrivate
{
};

static void *
openBackend (const char *backend)
{
    const char *home = getenv ("HOME");
    const char *override_backend = getenv ("LIBCOMPIZCONFIG_BACKEND_PATH");
    void *dlhand = NULL;
    char *dlname = NULL;
    char *err = NULL;

    if (override_backend && strlen (override_backend))
    {
	if (asprintf (&dlname, "%s/lib%s.so",
		      override_backend, backend) == -1)
	    dlname = NULL;

	dlerror ();
	dlhand = dlopen (dlname, RTLD_NOW | RTLD_NODELETE | RTLD_LOCAL);
	err = dlerror ();
    }

    if (!dlhand && home && strlen (home))
    {
	if (dlname)
	    free (dlname);

	if (asprintf (&dlname, "%s/.compizconfig/backends/lib%s.so",
		      home, backend) == -1)
	    dlname = NULL;

	dlerror ();
	dlhand = dlopen (dlname, RTLD_NOW | RTLD_NODELETE | RTLD_LOCAL);
	err = dlerror ();
    }

    if (!dlhand)
    {
	if (dlname)
	    free (dlname);

	if (asprintf (&dlname, "%s/compizconfig/backends/lib%s.so",
		      LIBDIR, backend) == -1)
	    dlname = NULL;
	dlhand = dlopen (dlname, RTLD_NOW | RTLD_NODELETE | RTLD_LOCAL);
	err = dlerror ();
    }

    free (dlname);

    if (err)
    {
	ccsError ("dlopen: %s", err);
    }

    return dlhand;
}

static CCSDynamicBackend *
ccsDynamicBackendWrapLoadedBackend (const CCSInterfaceTable *interfaces, CCSBackend *backend, void *dlhand)
{
    CCSDynamicBackend *dynamicBackend = calloc (1, sizeof (CCSDynamicBackend));
    CCSDynamicBackendPrivate *dbPrivate = NULL;

    if (!dynamicBackend)
	return NULL;

    ccsObjectInit (dynamicBackend, &ccsDefaultObjectAllocator);
    ccsDynamicBackendRef (dynamicBackend);

    dbPrivate = calloc (1, sizeof (CCSDynamicBackendPrivate));

    if (!dbPrivate)
    {
	ccsDynamicBackendUnref (dynamicBackend);
	return NULL;
    }

    dbPrivate->dlhand = dlhand;
    dbPrivate->backend = backend;

    ccsObjectSetPrivate (dynamicBackend, (CCSPrivate *) dbPrivate);
    ccsObjectAddInterface (dynamicBackend, (CCSInterface *) interfaces->dynamicBackendWrapperInterface, GET_INTERFACE_TYPE (CCSBackendInterface));
    ccsObjectAddInterface (dynamicBackend, (CCSInterface *) interfaces->dynamicBackendInterface, GET_INTERFACE_TYPE (CCSDynamicBackendInterface));

    return dynamicBackend;
}

static CCSBackend *
ccsSharedLibBackendLoaderLoadBackend (CCSBackendLoader *loader,
				      const CCSInterfaceTable *interfaces,
				      CCSContext       *context,
				      const char       *name)
{
    CCSBackendInterface *vt;
    void *dlhand = openBackend (name);

    if (!dlhand)
	return NULL;

    BackendGetInfoProc getInfo = dlsym (dlhand, "getBackendInfo");
    if (!getInfo)
    {
	dlclose (dlhand);
	return NULL;
    }

    vt = getInfo ();
    if (!vt)
    {
	dlclose (dlhand);
	return NULL;
    }

    CCSBackend *backend = ccsBackendNewWithDynamicInterface (context, vt);

    if (!backend)
    {
	dlclose (dlhand);
	return NULL;
    }

    CCSDynamicBackend *backendWrapper = ccsDynamicBackendWrapLoadedBackend (interfaces, backend, dlhand);

    if (!backendWrapper)
    {
	dlclose (dlhand);
	ccsBackendUnref (backend);
	return NULL;
    }

    return (CCSBackend *) backendWrapper;
}

static void
freeAndFinalizeLoader (CCSBackendLoader *loader, CCSObjectAllocationInterface *ai)
{
    ccsObjectFinalize (loader);
    (*ai->free_) (ai->allocator, loader);
}

static void
ccsSharedLibBackendLoaderFree (CCSBackendLoader *loader)
{
    freeAndFinalizeLoader (loader,
			   loader->object.object_allocation);
}

const CCSBackendLoaderInterface ccsSharedLibBackendLoaderInterface =
{
    ccsSharedLibBackendLoaderLoadBackend,
    ccsSharedLibBackendLoaderFree
};

static CCSBackendLoader *
allocateLoader (CCSObjectAllocationInterface *ai)
{
    CCSBackendLoader *loader = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSBackendLoader));

    if (!loader)
	return NULL;

    ccsObjectInit (loader, ai);

    return loader;
}

static CCSShraedLibBackendLoaderPrivate *
allocatePrivate (CCSBackendLoader *loader, CCSObjectAllocationInterface *ai)
{
    CCSShraedLibBackendLoaderPrivate *priv = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSShraedLibBackendLoaderPrivate));

    if (!priv)
    {
	freeAndFinalizeLoader (loader, ai);
	return NULL;
    }

    return priv;
}

CCSBackendLoader *
ccsSharedLibBackendLoaderNew (CCSObjectAllocationInterface *ai)
{
    CCSBackendLoader *loader = allocateLoader (ai);

    if (!loader)
	return NULL;

    CCSShraedLibBackendLoaderPrivate *priv = allocatePrivate (loader, ai);

    if (!priv)
	return NULL;

    ccsObjectSetPrivate (loader, (CCSPrivate *) (priv));
    ccsObjectAddInterface (loader, (const CCSInterface *) &ccsSharedLibBackendLoaderInterface,
			   GET_INTERFACE_TYPE (CCSBackendLoaderInterface));
    ccsObjectRef (loader);

    return loader;
}
