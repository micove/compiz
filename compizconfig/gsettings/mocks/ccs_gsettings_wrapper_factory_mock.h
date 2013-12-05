/**
 *
 * GSettings libcompizconfig backend
 *
 * ccs_gsettings_wrapper_factory_mock.h
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
#ifndef _COMPIZCONFIG_CCS_GSETTINGS_WRAPPER_FACTORY_MOCK
#define _COMPIZCONFIG_CCS_GSETTINGS_WRAPPER_FACTORY_MOCK

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ccs_gsettings_wrapper_factory_interface.h>

CCSGSettingsWrapperFactory * ccsMockGSettingsWrapperFactoryNew ();
void ccsMockGSettingsWrapperFactoryFree (CCSGSettingsWrapperFactory *);

class CCSGSettingsWrapperFactoryMockInterface
{
    public:

	virtual ~CCSGSettingsWrapperFactoryMockInterface () {}
	virtual CCSGSettingsWrapper * newGSettingsWrapper (const gchar                  *schema,
							   CCSObjectAllocationInterface *ai) = 0;
	virtual CCSGSettingsWrapper * newGSettingsWrapperWithPath (const gchar                  *schema,
								   const gchar                  *path,
								   CCSObjectAllocationInterface *ai) = 0;
};

class CCSGSettingsWrapperFactoryGMock :
    public CCSGSettingsWrapperFactoryMockInterface
{
    public:

	CCSGSettingsWrapperFactoryGMock (CCSGSettingsWrapperFactory *wrapper) :
	    mWrapper (wrapper)
	{
	}

	MOCK_METHOD2 (newGSettingsWrapper, CCSGSettingsWrapper * (const gchar                  *schema,
								  CCSObjectAllocationInterface *ai));
	MOCK_METHOD3 (newGSettingsWrapperWithPath, CCSGSettingsWrapper * (const gchar                  *schema,
									  const gchar                  *path,
									  CCSObjectAllocationInterface *ai));

    private:

	CCSGSettingsWrapperFactory *mWrapper;

    public:

	static CCSGSettingsWrapper *
	ccsGSettingsWrapperFactoryNewGSettingsWrapper (CCSGSettingsWrapperFactory   *wrapperFactory,
						       const gchar                  *schema,
						       CCSObjectAllocationInterface *ai)
	{
	    return reinterpret_cast <CCSGSettingsWrapperFactoryMockInterface *> (ccsObjectGetPrivate (wrapperFactory))->newGSettingsWrapper (schema, ai);
	}

	static CCSGSettingsWrapper *
	ccsGSettingsWrapperFactoryNewGSettingsWrapperWithPath (CCSGSettingsWrapperFactory   *wrapperFactory,
							       const gchar                  *schema,
							       const gchar                  *path,
							       CCSObjectAllocationInterface *ai)
	{
	    return reinterpret_cast <CCSGSettingsWrapperFactoryMockInterface *> (ccsObjectGetPrivate (wrapperFactory))->newGSettingsWrapperWithPath (schema, path, ai);
	}

	static void
	ccsFreeGSettingsWrapperFactory (CCSGSettingsWrapperFactory *wrapperFactory)
	{
	    ccsMockGSettingsWrapperFactoryFree (wrapperFactory);
	}
};

#endif
