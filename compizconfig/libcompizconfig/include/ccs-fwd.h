/*
 * Compiz configuration system library
 *
 * Copyright (C) 2007  Dennis Kasprzyk <onestone@beryl-project.org>
 * Copyright (C) 2007  Danny Baumann <maniac@beryl-project.org>
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
 */

#include <ccs-defs.h>

#ifndef _CSS_FWD_H
#define _CSS_FWD_H

COMPIZCONFIG_BEGIN_DECLS

typedef struct _CCSObject                    CCSObject;
typedef struct _CCSObjectAllocationInterface CCSObjectAllocationInterface;
typedef struct _CCSSetting	             CCSSetting;
typedef struct _CCSPlugin                    CCSPlugin;
typedef struct _CCSContext	             CCSContext;
typedef struct _CCSBackend	             CCSBackend;
typedef struct _CCSBackendInfo               CCSBackendInfo;
typedef struct _CCSBackendInterface          CCSBackendInterface;
typedef struct _CCSBackendLoader             CCSBackendLoader;
typedef struct _CCSDynamicBackend	     CCSDynamicBackend;
typedef struct _CCSConfigFile                CCSConfigFile;
typedef struct _CCSIntegration               CCSIntegration;
typedef struct _CCSSettingValue              CCSSettingValue;
typedef struct _CCSIntegratedSettingInfo     CCSIntegratedSettingInfo;
typedef struct _CCSIntegratedSetting         CCSIntegratedSetting;
typedef struct _CCSIntegratedSettingFactory  CCSIntegratedSettingFactory;
typedef struct _CCSIntegratedSettingsStorage CCSIntegratedSettingsStorage;
typedef struct _CCSInterfaceTable            CCSInterfaceTable;

typedef CCSBackendInterface * (*BackendGetInfoProc) (void);

COMPIZCONFIG_END_DECLS

#endif

