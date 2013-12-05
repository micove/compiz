/*
 * Compiz configuration system library
 *
 * ccs_config_file.c
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

#include <ccs-defs.h>
#include <ccs-object.h>
#include <ccs-private.h>
#include <ccs-config-private.h>
#include <ccs_config_file_interface.h>
#include <ccs_config_file.h>

#define SETTINGPATH "compiz-1/compizconfig"

typedef struct _CCSIniConfigFilePrivate CCSIniConfigFilePrivate;

struct _CCSIniConfigFilePrivate
{
    int configWatch;
};

static char*
getConfigFileName (void)
{
    char *configDir = NULL;
    char *fileName = NULL;

    configDir = getenv ("XDG_CONFIG_HOME");
    if (configDir && strlen (configDir))
    {
	if (asprintf (&fileName, "%s/%s/config", configDir, SETTINGPATH) == -1)
	    fileName = NULL;

	return fileName;
    }

    configDir = getenv ("HOME");
    if (configDir && strlen (configDir))
    {
	if (asprintf (&fileName, "%s/.config/%s/config", configDir, SETTINGPATH) == -1)
	    fileName = NULL;

	return fileName;
    }

    return NULL;
}

static IniDictionary*
getConfigFile (void)
{
    char          *fileName;
    IniDictionary *iniFile;

    fileName = getConfigFileName();

    if (!fileName)
	return NULL;

    iniFile = ccsIniOpen (fileName);

    free (fileName);

    return iniFile;
}

static Bool
ccsReadGlobalConfig (ConfigOption option,
		     char         **value)
{
    IniDictionary *iniFile;
    char          *entry = NULL;
    char          *section;
    Bool          ret;
    FILE          *fp;

    /* check if the global config file exists - if it doesn't, exit
       to avoid it being created by ccsIniOpen */
    fp = fopen (SYSCONFDIR "/compizconfig/config", "r");
    if (!fp)
	return FALSE;
    fclose (fp);

    iniFile = ccsIniOpen (SYSCONFDIR "/compizconfig/config");
    if (!iniFile)
	return FALSE;

    switch (option)
    {
    case OptionProfile:
	entry = "profile";
	break;
    case OptionBackend:
	entry = "backend";
	break;
    case OptionIntegration:
	entry = "integration";
	break;
    case OptionAutoSort:
	entry = "plugin_list_autosort";
	break;
    default:
	break;
    }

    if (!entry)
    {
	ccsIniClose (iniFile);
	return FALSE;
    }

    *value = NULL;
    section = getSectionName();
    ret = ccsIniGetString (iniFile, section, entry, value);
    free (section);
    ccsIniClose (iniFile);

    return ret;
}

static Bool
ccsIniConfigFileReadConfigOption (CCSConfigFile *config, ConfigOption option, char **value)
{
    IniDictionary *iniFile;
    char          *entry = NULL;
    char          *section;
    Bool          ret;

    iniFile = getConfigFile();
    if (!iniFile)
	return ccsReadGlobalConfig (option, value);

    switch (option)
    {
    case OptionProfile:
	entry = "profile";
	break;
    case OptionBackend:
	entry = "backend";
	break;
    case OptionIntegration:
	entry = "integration";
	break;
    case OptionAutoSort:
	entry = "plugin_list_autosort";
	break;
    default:
	break;
    }

    if (!entry)
    {
	ccsIniClose (iniFile);
	return FALSE;
    }

    *value = NULL;
    section = getSectionName();
    ret = ccsIniGetString (iniFile, section, entry, value);
    free (section);
    ccsIniClose (iniFile);

    if (!ret)
	ret = ccsReadGlobalConfig (option, value);

    return ret;
}

static Bool
ccsIniConfigFileWriteConfigOption (CCSConfigFile *config, ConfigOption option, const char *value)
{
    CCSIniConfigFilePrivate *priv = GET_PRIVATE (CCSIniConfigFilePrivate, config);
    IniDictionary *iniFile;
    char          *entry = NULL;
    char          *section = NULL;
    char          *fileName = NULL;
    char          *curVal = NULL;
    Bool          changed = TRUE;

    ccsDisableFileWatch (priv->configWatch);

    /* don't change config if nothing changed */
    if (ccsIniConfigFileReadConfigOption (config, option, &curVal))
    {
	changed = (strcmp (value, curVal) != 0);
	if (curVal)
	    free (curVal);
    }

    if (!changed)
    {
	ccsEnableFileWatch (priv->configWatch);
	return TRUE;
    }

    iniFile = getConfigFile();
    if (!iniFile)
    {
	ccsEnableFileWatch (priv->configWatch);
	return FALSE;
    }

    switch (option)
    {
    case OptionProfile:
	entry = "profile";
	break;
    case OptionBackend:
	entry = "backend";
	break;
    case OptionIntegration:
	entry = "integration";
	break;
    case OptionAutoSort:
	entry = "plugin_list_autosort";
	break;
    default:
	break;
    }

    if (!entry)
    {
	ccsIniClose (iniFile);
	ccsEnableFileWatch (priv->configWatch);
	return FALSE;
    }

    section = getSectionName();
    ccsIniSetString (iniFile, section, entry, value);
    free (section);

    fileName = getConfigFileName();
    if (!fileName)
    {
	ccsIniClose (iniFile);
	ccsEnableFileWatch (priv->configWatch);
	return FALSE;
    }

    ccsIniSave (iniFile, fileName);
    ccsIniClose (iniFile);
    free (fileName);

    ccsEnableFileWatch (priv->configWatch);

    return TRUE;
}

static void
ccsSetIniConfigWatchCallback (CCSConfigFile *config,
			      ConfigChangeCallbackProc callback,
			      void *data)
{
    CCSIniConfigFilePrivate *priv = GET_PRIVATE (CCSIniConfigFilePrivate, config);

    char         *fileName;

    fileName = getConfigFileName();
    if (!fileName)
	return;

    priv->configWatch = ccsAddFileWatch (fileName, TRUE, (FileWatchCallbackProc) callback, data);
    free (fileName);

    return;
}

static void
freeAndFinalizeConfigFile (CCSConfigFile *config, CCSObjectAllocationInterface *ai)
{
    CCSIniConfigFilePrivate *priv = GET_PRIVATE (CCSIniConfigFilePrivate, config);

    if (priv)
    {
	if (priv->configWatch)
	    ccsRemoveFileWatch (priv->configWatch);
    }

    ccsObjectFinalize (config);
    (*ai->free_) (ai->allocator, config);
}

static void
ccsIniConfigFileFree (CCSConfigFile *config)
{
    freeAndFinalizeConfigFile (config,
			       config->object.object_allocation);
}

const CCSConfigFileInterface ccsIniConfigFileInterface =
{
    ccsIniConfigFileReadConfigOption,
    ccsIniConfigFileWriteConfigOption,
    ccsSetIniConfigWatchCallback,
    ccsIniConfigFileFree
};

static CCSConfigFile *
allocateConfigFile (CCSObjectAllocationInterface *ai)
{
    CCSConfigFile *loader = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSConfigFile));

    if (!loader)
	return NULL;

    ccsObjectInit (loader, ai);

    return loader;
}

static CCSIniConfigFilePrivate *
allocatePrivate (CCSConfigFile *config, CCSObjectAllocationInterface *ai)
{
    CCSIniConfigFilePrivate *priv = (*ai->calloc_) (ai->allocator, 1, sizeof (CCSIniConfigFilePrivate));

    if (!priv)
    {
	freeAndFinalizeConfigFile (config, ai);
	return NULL;
    }

    return priv;
}

CCSConfigFile *
ccsInternalConfigFileNew (CCSObjectAllocationInterface *ai)
{
    CCSConfigFile *loader = allocateConfigFile (ai);

    if (!loader)
	return NULL;

    CCSIniConfigFilePrivate *priv = allocatePrivate (loader, ai);

    if (!priv)
	return NULL;

    priv->configWatch = -1;

    ccsObjectSetPrivate (loader, (CCSPrivate *) (priv));
    ccsObjectAddInterface (loader, (const CCSInterface *) &ccsIniConfigFileInterface,
			   GET_INTERFACE_TYPE (CCSConfigFileInterface));
    ccsObjectRef (loader);

    return loader;
}
