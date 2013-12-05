/*
 * Copyright (C) 2012 Canonical Ltd.
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
 * Sam Spilsbury <sam.spilsbury@canonical.com>
 */
#ifndef _COMPIZ_GTEST_SHARED_TMPENV
#define _COMPIZ_GTEST_SHARED_TMPENV

#include <cstdlib>
#include <boost/noncopyable.hpp>

class TmpEnv :
    boost::noncopyable
{
    public:

	explicit TmpEnv (const char *env, const char *val) :
	    envRestore (env),
	    envRestoreVal (getenv (env))
	{
	    if (!val)
		unsetenv (env);
	    else
		setenv (env, val, 1);
	}

	~TmpEnv ()
	{
	    if (envRestoreVal)
		setenv (envRestore, envRestoreVal, 1);
	    else
		unsetenv (envRestore);
	}

    private:

	const char *envRestore;
	const char *envRestoreVal;
};

#endif
