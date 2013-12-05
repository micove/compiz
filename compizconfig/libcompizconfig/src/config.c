/*
 * Compiz configuration system library
 *
 * Copyright (C) 2007  Dennis Kasprzyk <onestone@opencompositing.org>
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "ccs-config-private.h"

char *
getSectionName (void)
{
    const char *profile;
    char *section;

    profile = getenv ("COMPIZ_CONFIG_PROFILE");
    if (profile)
    {
	if (strlen (profile))
	{
	    if (asprintf (&section, "general_%s", profile) == -1)
		section = NULL;
	}
	else
	{
	    section = strdup ("general");
	}

	return section;
    }

    profile = getenv ("GNOME_DESKTOP_SESSION_ID");
    if (profile && strlen (profile))
	return strdup ("gnome_session");

    profile = getenv ("KDE_SESSION_VERSION");
    if (profile && strlen (profile) && strcasecmp (profile, "4") == 0)
	return strdup ("kde4_session");

    profile = getenv ("KDE_FULL_SESSION");
    if (profile && strlen (profile) && strcasecmp (profile, "true") == 0)
	return strdup ("kde_session");

    return strdup ("general");
}
