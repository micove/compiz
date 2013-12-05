/*
 * Compiz configuration system library
 *
 * Copyright (C) 2013 Sam Spilsbury <smspillaz@gmail.com>.
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

#include <ccs.h>

#include "compizconfig_ccs_setting_stub.h"

typedef struct _Private
{
    Bool            integrated;
    Bool            readOnly;
    const char      *name;
    const char      *shortDesc;
    const char      *longDesc;
    const char      *hints;
    const char      *group;
    const char      *subGroup;
    CCSSettingType  type;
    CCSPlugin       *parent;
    CCSSettingValue *value;
    CCSSettingValue *defaultValue;
    CCSSettingInfo  info;
} Private;

static Bool ccsStubSettingGetInt (CCSSetting *setting,
				  int        *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asInt;
    return TRUE;
}

static Bool ccsStubSettingGetFloat (CCSSetting *setting,
				    float      *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asFloat;
    return TRUE;
}

static Bool ccsStubSettingGetBool (CCSSetting *setting,
				   Bool       *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asBool;
    return TRUE;
}

static Bool ccsStubSettingGetString (CCSSetting *setting,
				     const char **data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asString;
    return TRUE;
}

static Bool ccsStubSettingGetColor (CCSSetting           *setting,
				    CCSSettingColorValue *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asColor;
    return TRUE;
}

static Bool ccsStubSettingGetMatch (CCSSetting *setting,
				    const char **data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asMatch;
    return TRUE;
}

static Bool ccsStubSettingGetKey (CCSSetting         *setting,
				  CCSSettingKeyValue *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asKey;
    return TRUE;
}

static Bool ccsStubSettingGetButton (CCSSetting            *setting,
				     CCSSettingButtonValue *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asButton;
    return TRUE;
}

static Bool ccsStubSettingGetEdge (CCSSetting  *setting,
				   unsigned int *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asEdge;
    return TRUE;
}

static Bool ccsStubSettingGetBell (CCSSetting *setting,
				   Bool       *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asBell;
    return TRUE;
}

static Bool ccsStubSettingGetList (CCSSetting          *setting,
				   CCSSettingValueList *data)
{
    *data = ((Private *) ccsObjectGetPrivate (setting))->value->value.asList;
    return TRUE;
}

static CCSSetStatus ccsStubSettingSetInt (CCSSetting *setting,
					  int        data,
					  Bool       processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asInt = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetFloat (CCSSetting *setting,
					    float      data,
					    Bool       processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asFloat = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetBool (CCSSetting *setting,
					   Bool       data,
					   Bool       processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asBool = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetString (CCSSetting *setting,
					     const char *data,
					     Bool       processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asString = (char *) data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetColor (CCSSetting           *setting,
					    CCSSettingColorValue data,
					    Bool                 processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asColor = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetMatch (CCSSetting *setting,
					    const char *data,
					    Bool       processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asMatch = (char *) data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetKey (CCSSetting         *setting,
					  CCSSettingKeyValue data,
					  Bool               processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asKey = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetButton (CCSSetting            *setting,
					     CCSSettingButtonValue data,
					     Bool                  processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asButton = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetEdge (CCSSetting   *setting,
					   unsigned int data,
					   Bool         processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asEdge = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetBell (CCSSetting *setting,
					   Bool       data,
					   Bool       processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asBell = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetList (CCSSetting          *setting,
					   CCSSettingValueList data,
					   Bool                processChanged)
{
    ((Private *) ccsObjectGetPrivate (setting))->value->value.asList = data;
    return SetToNewValue;
}

static CCSSetStatus ccsStubSettingSetValue (CCSSetting      *setting,
					    CCSSettingValue *data,
					    Bool            processChanged)
{
    Private *priv = (Private *) ccsObjectGetPrivate (setting);

    ccsCopyValueInto (data,
		      priv->value,
		      priv->type,
		      &priv->info);
    return SetToNewValue;
}

static void ccsStubSettingResetToDefault (CCSSetting * setting, Bool processChanged)
{
}

static Bool ccsStubSettingIsIntegrated (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->integrated;
}

static Bool ccsStubSettingReadOnly (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->readOnly;
}

static const char * ccsStubSettingGetName (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->name;
}

static const char * ccsStubSettingGetShortDesc (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->shortDesc;
}

static const char * ccsStubSettingGetLongDesc (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->longDesc;
}

static CCSSettingType ccsStubSettingGetType (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->type;
}

static CCSSettingInfo * ccsStubSettingGetInfo (CCSSetting *setting)
{
    return &((Private *) ccsObjectGetPrivate (setting))->info;
}

static const char * ccsStubSettingGetGroup (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->group;
}

static const char * ccsStubSettingGetSubGroup (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->subGroup;
}

static const char * ccsStubSettingGetHints (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->hints;
}

static CCSSettingValue * ccsStubSettingGetDefaultValue (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->defaultValue;
}

static CCSSettingValue * ccsStubSettingGetValue (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->value;
}

static Bool ccsStubSettingIsDefault (CCSSetting *setting)
{
    Private *priv = (Private *) ccsObjectGetPrivate (setting);

    return ccsCheckValueEq (priv->value,
			    priv->type,
			    &priv->info,
			    priv->defaultValue,
			    priv->type,
			    &priv->info);
}

static CCSPlugin * ccsStubSettingGetParent (CCSSetting *setting)
{
    return ((Private *) ccsObjectGetPrivate (setting))->parent;
}

static void * ccsStubSettingGetPrivatePtr (CCSSetting *setting)
{
    return NULL;
}

static void ccsStubSettingSetPrivatePtr (CCSSetting *setting, void *ptr)
{
}

static Bool ccsStubSettingIsReadableByBackend (CCSSetting *setting)
{
    return TRUE;
}

static void ccsFreeSettingTypeStub (CCSSetting *setting)
{
    Private *priv = (Private *) ccsObjectGetPrivate (setting);

    ccsFreeSettingValueWithType (priv->value, priv->type);
    ccsFreeSettingValueWithType (priv->defaultValue, priv->type);
    ccsCleanupSettingInfo (&priv->info, priv->type);

    ccsObjectFinalize (setting);

    free (setting);
}

static const CCSSettingInterface ccsStubSettingInterface =
{
    ccsStubSettingGetName,
    ccsStubSettingGetShortDesc,
    ccsStubSettingGetLongDesc,
    ccsStubSettingGetType,
    ccsStubSettingGetInfo,
    ccsStubSettingGetGroup,
    ccsStubSettingGetSubGroup,
    ccsStubSettingGetHints,
    ccsStubSettingGetDefaultValue,
    ccsStubSettingGetValue,
    ccsStubSettingIsDefault,
    ccsStubSettingGetParent,
    ccsStubSettingGetPrivatePtr,
    ccsStubSettingSetPrivatePtr,
    ccsStubSettingSetInt,
    ccsStubSettingSetFloat,
    ccsStubSettingSetBool,
    ccsStubSettingSetString,
    ccsStubSettingSetColor,
    ccsStubSettingSetMatch,
    ccsStubSettingSetKey,
    ccsStubSettingSetButton,
    ccsStubSettingSetEdge,
    ccsStubSettingSetBell,
    ccsStubSettingSetList,
    ccsStubSettingSetValue,
    ccsStubSettingGetInt,
    ccsStubSettingGetFloat,
    ccsStubSettingGetBool,
    ccsStubSettingGetString,
    ccsStubSettingGetColor,
    ccsStubSettingGetMatch,
    ccsStubSettingGetKey,
    ccsStubSettingGetButton,
    ccsStubSettingGetEdge,
    ccsStubSettingGetBell,
    ccsStubSettingGetList,
    ccsStubSettingResetToDefault,
    ccsStubSettingIsIntegrated,
    ccsStubSettingReadOnly,
    ccsStubSettingIsReadableByBackend,
    ccsFreeSettingTypeStub
};

CCSSetting *
ccsSettingTypeStubNew (CCSSettingType  type,
                       Bool            integrated,
                       Bool            readOnly,
                       const char      *name,
                       const char      *shortDesc,
                       const char      *longDesc,
                       const char      *hints,
                       const char      *group,
                       const char      *subGroup,
                       CCSSettingValue *value,
                       CCSSettingValue *defaultValue,
                       CCSSettingInfo  *info,
                       CCSObjectAllocationInterface *ai)
{
    Private *priv = (Private *) calloc (1, sizeof (Private));
    priv->integrated = integrated;
    priv->readOnly  = readOnly;
    priv->name = name;
    priv->shortDesc = shortDesc;
    priv->longDesc = longDesc;
    priv->hints = hints;
    priv->group = group;
    priv->subGroup = subGroup;
    priv->type = type;

    priv->value = (CCSSettingValue *) calloc (1, sizeof (CCSSettingValue));
    priv->defaultValue = (CCSSettingValue *) calloc (1, sizeof (CCSSettingValue));

    if (value)
	ccsCopyValueInto (value, priv->value, priv->type, info);
    if (defaultValue)
	ccsCopyValueInto (defaultValue, priv->defaultValue, priv->type, info);
    if (info)
	ccsCopyInfo (info, &priv->info, priv->type);

    CCSSetting *setting = (CCSSetting *) calloc (1, sizeof (CCSSetting));
    ccsObjectInit (setting, ai);
    ccsObjectSetPrivate (setting, (CCSPrivate *) priv);
    ccsObjectAddInterface (setting,
			   (CCSInterface *) &ccsStubSettingInterface,
			   GET_INTERFACE_TYPE (CCSSettingInterface));
    ccsSettingRef (setting);

    return setting;
}
