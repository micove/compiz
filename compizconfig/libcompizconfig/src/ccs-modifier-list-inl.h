/*
 * Compiz configuration system library
 *
 * Copyright (C) 2007  Danny Baumann <maniac@opencompositing.org>
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

#ifndef _CCS_MODIFIER_LIST_INL_H
#define _CCS_MODIFIER_LIST_INL_H

#include <ccs-defs.h>

COMPIZCONFIG_BEGIN_DECLS

#include <X11/X.h>
#include <X11/Xlib.h>

#define CompAltMask        (1 << 16)
#define CompMetaMask       (1 << 17)
#define CompSuperMask      (1 << 18)
#define CompHyperMask      (1 << 19)
#define CompModeSwitchMask (1 << 20)
#define CompNumLockMask    (1 << 21)
#define CompScrollLockMask (1 << 22)

struct _Modifier
{
    char *name;
    int  modifier;
};

extern struct _Modifier modifierList[];
unsigned int ccsInternalUtilNumModifiers ();

COMPIZCONFIG_END_DECLS

#endif
