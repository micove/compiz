/*
 * Compiz configuration system library
 *
 * ccs_backend_loader.h
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
#ifndef CCS_BACKEND_LOADER_H
#define CCS_BACKEND_LOADER_H

#include <ccs-defs.h>
#include <ccs-object.h>
#include "ccs_backend_loader_interface.h"

COMPIZCONFIG_BEGIN_DECLS

CCSBackendLoader *
ccsSharedLibBackendLoaderNew (CCSObjectAllocationInterface *ai);

COMPIZCONFIG_END_DECLS

#endif
