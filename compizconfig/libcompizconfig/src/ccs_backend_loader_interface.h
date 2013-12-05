/*
 * Compiz configuration system library
 *
 * ccs_backend_loader_interface.h
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
#ifndef CCS_BACKEND_LOADER_INTERFACE_H
#define CCS_BACKEND_LOADER_INTERFACE_H

#include <ccs-fwd.h>
#include <ccs-defs.h>
#include <ccs-object.h>

COMPIZCONFIG_BEGIN_DECLS

typedef struct _CCSBackendLoaderInterface CCSBackendLoaderInterface;

typedef CCSBackend * (*CCSLoadBackend) (CCSBackendLoader *, const CCSInterfaceTable *interfaces, CCSContext *, const char *);
typedef void (*CCSFreeBackendLoader) (CCSBackendLoader *);

struct _CCSBackendLoaderInterface
{
    CCSLoadBackend       loadBackend;
    CCSFreeBackendLoader free;
};

struct _CCSBackendLoader
{
    CCSObject object;
};

CCSREF_HDR (BackendLoader, CCSBackendLoader);

CCSBackend *
ccsBackendLoaderLoadBackend (CCSBackendLoader *loader,
			     const CCSInterfaceTable *interfaces,
			     CCSContext *context,
			     const char *);

void
ccsFreeBackendLoader (CCSBackendLoader *);

unsigned int ccsCCSBackendLoaderInterfaceGetType ();

COMPIZCONFIG_END_DECLS

#endif
