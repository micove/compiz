/*
 * Compiz configuration system library
 *
 * ccs_backend_loader_interface.c
 *
 * Copyright (C) 2012 Canonical Ltd.
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
 * Sam Spilsbury <sam.spilsbury@canonical.com>
 */

#include <ccs-defs.h>
#include <ccs-object.h>
#include <ccs_backend_loader_interface.h>
#include <ccs_backend_loader.h>

INTERFACE_TYPE (CCSBackendLoaderInterface);
CCSREF_OBJ (BackendLoader, CCSBackendLoader);

CCSBackend *
ccsBackendLoaderLoadBackend (CCSBackendLoader *loader,
			     const CCSInterfaceTable *interfaces,
			     CCSContext *context,
			     const char *name)
{
    return (*(GET_INTERFACE (CCSBackendLoaderInterface, loader))->loadBackend) (loader, interfaces, context, name);
}

void
ccsFreeBackendLoader (CCSBackendLoader *file)
{
    return (*(GET_INTERFACE (CCSBackendLoaderInterface, file))->free) (file);
}
