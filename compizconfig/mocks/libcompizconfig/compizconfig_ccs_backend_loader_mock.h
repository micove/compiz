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
#ifndef COMPIZCONFIG_CCS_BACKEND_LOADER_MOCK_H
#define COMPIZCONFIG_CCS_BACKEND_LOADER_MOCK_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ccs-object.h>
#include <ccs-defs.h>

COMPIZCONFIG_BEGIN_DECLS

typedef struct _CCSBackend CCSBackend;
typedef struct _CCSInterfaceTable CCSInterfaceTable;
typedef struct _CCSBackendLoader CCSBackendLoader;
typedef struct _CCSObjectAllocationInterface CCSObjectAllocationInterface;

CCSBackendLoader * ccsMockBackendLoaderNew (CCSObjectAllocationInterface *ai);
void            ccsFreeMockBackendLoader (CCSBackendLoader *);

typedef void (*ConfigChangeCallbackProc) (unsigned int watchId, void *closure);

COMPIZCONFIG_END_DECLS

class CCSBackendLoaderGMockInterface
{
    public:

	virtual ~CCSBackendLoaderGMockInterface () {}
	virtual CCSBackend * loadBackend (const CCSInterfaceTable *, CCSContext *, const char *) = 0;
};

class CCSBackendLoaderGMock :
    public CCSBackendLoaderGMockInterface
{
    public:

	MOCK_METHOD3 (loadBackend, CCSBackend * (const CCSInterfaceTable *, CCSContext *, const char *));
    public:

	static CCSBackend *
	ccsBackendLoaderLoadBackend (CCSBackendLoader *loader,
				     const CCSInterfaceTable *interfaces,
				     CCSContext *context,
				     const char *name)
	{
	    return reinterpret_cast <CCSBackendLoaderGMock *> (ccsObjectGetPrivate (loader))->loadBackend (interfaces, context, name);
	}

	static void ccsFreeBackendLoader (CCSBackendLoader *config)
	{
	    ccsFreeMockBackendLoader (config);
	}
};

#endif
