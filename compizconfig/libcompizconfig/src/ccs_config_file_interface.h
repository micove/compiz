/*
 * Compiz configuration system library
 *
 * ccs_config_file_interface.h
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
#ifndef CCS_CONFIG_FILE_INTERFACE_H
#define CCS_CONFIG_FILE_INTERFACE_H

#include <ccs-defs.h>
#include <ccs-object.h>
#include <ccs-config-private.h>

COMPIZCONFIG_BEGIN_DECLS

typedef struct _CCSConfigFile CCSConfigFile;
typedef struct _CCSConfigFileInterface CCSConfigFileInterface;

typedef void (*ConfigChangeCallbackProc) (unsigned int watchId, void *closure);

typedef Bool (*CCSReadConfigOption) (CCSConfigFile *, ConfigOption, char **value);
typedef Bool (*CCSWriteConfigOption) (CCSConfigFile *, ConfigOption, const char *value);
typedef void (*CCSSetConfigWatchCallback) (CCSConfigFile *, ConfigChangeCallbackProc, void *closure);
typedef void   (*CCSFreeConfigFile) (CCSConfigFile *file);

struct _CCSConfigFileInterface
{
    CCSReadConfigOption       readConfigOption;
    CCSWriteConfigOption      writeConfigOption;
    CCSSetConfigWatchCallback setConfigWatchCallback;
    CCSFreeConfigFile	      free;
};

struct _CCSConfigFile
{
    CCSObject object;
};

CCSREF_HDR (ConfigFile, CCSConfigFile);

Bool
ccsConfigFileReadConfigOption (CCSConfigFile *, ConfigOption, char **value);

Bool
ccsConfigFileWriteConfigOption (CCSConfigFile *, ConfigOption, const char *);

void
ccsSetConfigWatchCallback (CCSConfigFile *, ConfigChangeCallbackProc, void *);

void
ccsFreeConfigFile (CCSConfigFile *);

unsigned int ccsCCSConfigFileInterfaceGetType ();

COMPIZCONFIG_END_DECLS

#endif
