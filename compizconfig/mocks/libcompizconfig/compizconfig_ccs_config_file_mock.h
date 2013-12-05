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
#ifndef COMPIZCONFIG_CCS_CONFIG_FILE_MOCK_H
#define COMPIZCONFIG_CCS_CONFIG_FILE_MOCK_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ccs-object.h>
#include <ccs-defs.h>
#include <ccs-config-private.h>

COMPIZCONFIG_BEGIN_DECLS

typedef struct _CCSConfigFile CCSConfigFile;
typedef struct _CCSObjectAllocationInterface CCSObjectAllocationInterface;

CCSConfigFile * ccsMockConfigFileNew (CCSObjectAllocationInterface *ai);
void            ccsFreeMockConfigFile (CCSConfigFile *);

typedef void (*ConfigChangeCallbackProc) (unsigned int watchId, void *closure);

COMPIZCONFIG_END_DECLS

class CCSConfigFileGMockInterface
{
    public:

	virtual ~CCSConfigFileGMockInterface () {}
	virtual Bool readConfigOption (ConfigOption, char **) = 0;
	virtual Bool writeConfigOption (ConfigOption, const char *) = 0;
	virtual void setConfigWatchCallback (ConfigChangeCallbackProc, void *closure) = 0;
};

class CCSConfigFileGMock :
    public CCSConfigFileGMockInterface
{
    public:

	MOCK_METHOD2 (readConfigOption, Bool (ConfigOption, char **));
	MOCK_METHOD2 (writeConfigOption, Bool (ConfigOption, const char *));
	MOCK_METHOD2 (setConfigWatchCallback, void (ConfigChangeCallbackProc, void *));
    public:

	static Bool ccsConfigFileReadConfigOption (CCSConfigFile *config,
						   ConfigOption  option,
						   char          **value)
	{
	    return reinterpret_cast <CCSConfigFileGMock *> (ccsObjectGetPrivate (config))->readConfigOption (option, value);
	}

	static Bool ccsConfigFileWriteConfigOption (CCSConfigFile *config,
						    ConfigOption  option,
						    const char    *value)
	{
	    return reinterpret_cast <CCSConfigFileGMock *> (ccsObjectGetPrivate (config))->writeConfigOption (option, value);
	}

	static void ccsSetConfigWatchCallback (CCSConfigFile            *config,
					       ConfigChangeCallbackProc proc,
					       void                     *closure)
	{
	    reinterpret_cast <CCSConfigFileGMock *> (ccsObjectGetPrivate (config))->setConfigWatchCallback (proc, closure);
	}

	static void ccsFreeConfigFile (CCSConfigFile *config)
	{
	    ccsFreeMockConfigFile (config);
	}
};

#endif
