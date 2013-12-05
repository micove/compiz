/*
 * Compiz configuration system library
 *
 * ccs_config_file_interface.c
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

#include <ccs-defs.h>
#include <ccs-object.h>
#include <ccs_config_file_interface.h>
#include <ccs_config_file.h>

INTERFACE_TYPE (CCSConfigFileInterface);
CCSREF_OBJ (ConfigFile, CCSConfigFile);

Bool
ccsConfigFileReadConfigOption (CCSConfigFile *config, ConfigOption opt, char **value)
{
    return (*(GET_INTERFACE (CCSConfigFileInterface, config))->readConfigOption) (config, opt, value);
}

Bool
ccsConfigFileWriteConfigOption (CCSConfigFile *config, ConfigOption opt, const char *value)
{
    return (*(GET_INTERFACE (CCSConfigFileInterface, config))->writeConfigOption) (config, opt, value);
}

void
ccsSetConfigWatchCallback (CCSConfigFile *config, ConfigChangeCallbackProc callback, void *closure)
{
    (*(GET_INTERFACE (CCSConfigFileInterface, config))->setConfigWatchCallback) (config, callback, closure);
}

void
ccsFreeConfigFile (CCSConfigFile *config)
{
    return (*(GET_INTERFACE (CCSConfigFileInterface, config))->free) (config);
}
