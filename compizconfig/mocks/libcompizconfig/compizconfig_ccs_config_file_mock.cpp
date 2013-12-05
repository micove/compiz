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
#include <ccs_config_file_interface.h>
#include "compizconfig_ccs_config_file_mock.h"

const CCSConfigFileInterface ccsMockConfigFileInterface =
{
    CCSConfigFileGMock::ccsConfigFileReadConfigOption,
    CCSConfigFileGMock::ccsConfigFileWriteConfigOption,
    CCSConfigFileGMock::ccsSetConfigWatchCallback,
    CCSConfigFileGMock::ccsFreeConfigFile
};

static void
finalizeAndFreeConfigFile (CCSConfigFile *configFile)
{
    CCSObjectAllocationInterface *ai = configFile->object.object_allocation;

    ccsObjectFinalize (configFile);
    (*ai->free_) (ai->allocator, configFile);
}

void
ccsFreeMockConfigFile (CCSConfigFile *configFile)
{
    CCSConfigFileGMock *gmock = reinterpret_cast <CCSConfigFileGMock *> (ccsObjectGetPrivate (configFile));
    delete gmock;

    ccsObjectSetPrivate (configFile, NULL);
    finalizeAndFreeConfigFile (configFile);

}

static CCSConfigFileGMock *
newCCSConfigFileGMockInterface (CCSConfigFile *configFile)
{
    CCSConfigFileGMock *gmock = new CCSConfigFileGMock ();

    if (!gmock)
    {
	finalizeAndFreeConfigFile (configFile);
	return NULL;
    }

    return gmock;
}

static CCSConfigFile *
allocateCCSConfigFile (CCSObjectAllocationInterface *ai)
{
    CCSConfigFile *configFile = reinterpret_cast <CCSConfigFile *> ((*ai->calloc_) (ai->allocator, 1, sizeof (CCSConfigFile)));

    if (!configFile)
	return NULL;

    ccsObjectInit (configFile, ai);

    return configFile;
}

CCSConfigFile *
ccsMockConfigFileNew (CCSObjectAllocationInterface *ai)
{
    CCSConfigFile *configFile = allocateCCSConfigFile (ai);

    if (!configFile)
	return NULL;

    CCSConfigFileGMock *gmock = newCCSConfigFileGMockInterface (configFile);

    if (!gmock)
	return NULL;

    ccsObjectSetPrivate (configFile, (CCSPrivate *) gmock);
    ccsObjectAddInterface (configFile,
			   (const CCSInterface *) &ccsMockConfigFileInterface,
			   GET_INTERFACE_TYPE (CCSConfigFileInterface));
    ccsObjectRef (configFile);

    return configFile;
}
