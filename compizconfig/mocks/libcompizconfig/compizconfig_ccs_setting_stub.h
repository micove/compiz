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

#ifndef _COMPIZCONFIG_CCS_SETTING_STUB
#define _COMPIZCONFIG_CCS_SETTING_STUB

#include <ccs.h>

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
		       CCSObjectAllocationInterface *ai);

#endif
