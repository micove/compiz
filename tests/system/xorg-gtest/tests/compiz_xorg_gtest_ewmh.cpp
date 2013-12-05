/*
 * Compiz XOrg GTest, EWMH compliance
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
#include <list>
#include <string>
#include <stdexcept>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <xorg/gtest/xorg-gtest.h>
#include <compiz-xorg-gtest.h>

#include <gtest_shared_tmpenv.h>
#include <gtest_shared_characterwrapper.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

using ::testing::MatchResultListener;
using ::testing::MakeMatcher;
using ::testing::Matcher;

namespace ct = compiz::testing;

namespace
{
unsigned int DEFAULT_VIEWPORT_WIDTH = 4;
unsigned int DEFAULT_VIEWPORT_HEIGHT = 1;

bool Advance (Display *d, bool r)
{
    return ct::AdvanceToNextEventOnSuccess (d, r);
}

}

class CompizXorgSystemEWMH :
    public ct::CompizXorgSystemTest
{
    public:

	virtual void SetUp ()
	{
	    ct::CompizXorgSystemTest::SetUp ();

	    ::Display *dpy = Display ();

	    XSelectInput (dpy, DefaultRootWindow (dpy),
			  PropertyChangeMask);

	    Window wDummy;
	    unsigned int uiDummy;
	    int iDummy;

	    ASSERT_TRUE (XGetGeometry (dpy, DefaultRootWindow (dpy),
				       &wDummy,
				       &iDummy,
				       &iDummy,
				       &screenWidth,
				       &screenHeight,
				       &uiDummy,
				       &uiDummy));
	}

	unsigned int screenWidth;
	unsigned int screenHeight;

    private:
};

TEST_F (CompizXorgSystemEWMH, InitialViewportGeometry)
{
    ::Display *dpy = Display ();
    StartCompiz (static_cast <ct::CompizProcess::StartupFlags> (
		     ct::CompizProcess::ReplaceCurrentWM),
		 ct::CompizProcess::PluginList ());

    ct::PropertyNotifyXEventMatcher desktopHintsProperty (dpy,
							  "_NET_DESKTOP_GEOMETRY");

    /* Assert that we get the property update */
    ASSERT_TRUE (Advance (dpy,
			  ct::WaitForEventOfTypeOnWindowMatching (dpy,
								  DefaultRootWindow (dpy),
								  PropertyNotify,
								  -1,
								  -1,
								  desktopHintsProperty)));

    unsigned int expectedDefaultWidth = screenWidth * DEFAULT_VIEWPORT_WIDTH;
    unsigned int expectedDefaultHeight = screenHeight * DEFAULT_VIEWPORT_HEIGHT;

    Atom actualType;
    int  actualFmt;
    unsigned long nItems, bytesAfter;
    unsigned char *property;

    ASSERT_EQ (Success,
	       XGetWindowProperty (dpy,
				   DefaultRootWindow (dpy),
				   XInternAtom (dpy, "_NET_DESKTOP_GEOMETRY", False),
				   0L,
				   8L,
				   False,
				   XA_CARDINAL,
				   &actualType,
				   &actualFmt,
				   &nItems,
				   &bytesAfter,
				   &property));

    ASSERT_EQ (XA_CARDINAL, actualType);
    ASSERT_EQ (32, actualFmt);
    ASSERT_EQ (2, nItems);

    unsigned long *geometry = reinterpret_cast <unsigned long *> (property);

    EXPECT_EQ (expectedDefaultWidth, geometry[0]);
    EXPECT_EQ (expectedDefaultHeight, geometry[1]);

    if (property)
	XFree (property);
}
