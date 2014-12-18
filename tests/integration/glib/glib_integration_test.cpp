/*
 * Compiz GLib integration test
 *
 * Copyright (C) 2012 Sam Spilsbury <smspillaz@gmail.com>
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

#include "privateeventsource.h"
#include "privatetimeoutsource.h"
#include "privateiosource.h"

class DieVerifier
{
    public:

	MOCK_METHOD0 (Die, void ());
};

class GLibSourceDestroyIntegration :
    public ::testing::Test
{
    public:

	GLibSourceDestroyIntegration () :
	    context (Glib::MainContext::get_default ()),
	    mainloop (Glib::MainLoop::create ())
	{
	}

	DieVerifier                  die;
	Glib::RefPtr <Glib::MainContext> context;
	Glib::RefPtr <Glib::MainLoop>    mainloop;
};

class StubTimeoutSource :
    public CompTimeoutSource
{
    public:

	StubTimeoutSource (DieVerifier *dieVerifier,
			   Glib::RefPtr <Glib::MainContext> &context) :
	    CompTimeoutSource (context),
	    mDie (dieVerifier)
	{
	}

	virtual ~StubTimeoutSource ()
	{
	    mDie->Die ();
	}

    private:

	DieVerifier *mDie;
};

class StubEventSource :
    public CompEventSource
{
    public:

	StubEventSource (DieVerifier *dieVerifier) :
	    CompEventSource (NULL, 0),
	    mDie (dieVerifier)
	{
	}

	virtual ~StubEventSource ()
	{
	    mDie->Die ();
	}

    private:

	DieVerifier *mDie;
};

class StubWatchFd :
    public CompWatchFd
{
    public:

	StubWatchFd (DieVerifier *dieVerifier,
		     int fd,
		     Glib::IOCondition iocond,
		     const FdWatchCallBack &cb) :
	    CompWatchFd (fd, iocond, cb),
	    mDie (dieVerifier)
	{
	}

	virtual ~StubWatchFd ()
	{
	    mDie->Die ();
	}

    private:

	DieVerifier *mDie;
};

TEST_F (GLibSourceDestroyIntegration, TimeoutSourceGSourceDestroy)
{
    Glib::RefPtr<StubTimeoutSource> sts(new StubTimeoutSource (&die, context));

    EXPECT_CALL (die, Die ());

    g_source_destroy (sts->gobj ());
}

TEST_F (GLibSourceDestroyIntegration, EventSourceGSourceDestroy)
{
    Glib::RefPtr<StubEventSource> sts(new StubEventSource (&die));

    sts->attach (context);

    EXPECT_CALL (die, Die ());

    g_source_destroy (sts->gobj ());
}

TEST_F (GLibSourceDestroyIntegration, FdSourceGSourceDestroy)
{
    Glib::IOCondition iocond = Glib::IO_IN;
    int fd = 0;
    FdWatchCallBack cb;
    Glib::RefPtr<StubWatchFd> sts(new StubWatchFd (&die, fd, iocond, cb));

    sts->attach (context);

    EXPECT_CALL (die, Die ());

    g_source_destroy (sts->gobj ());
}
