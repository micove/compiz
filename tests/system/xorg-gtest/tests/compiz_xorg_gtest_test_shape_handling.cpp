/*
 * Compiz XOrg GTest, window stacking
 *
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
#include <list>
#include <string>
#include <stdexcept>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <xorg/gtest/xorg-gtest.h>
#include <compiz-xorg-gtest.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

using ::testing::MatchResultListener;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::IsNull;
using ::testing::NotNull;

namespace ct = compiz::testing;

namespace
{
    bool Advance (Display *dpy,
		  bool waitResult)
    {
	return ct::AdvanceToNextEventOnSuccess (dpy, waitResult);
    }

    Window FindTopLevelParent (Display *dpy,
			       Window  child)
    {
	Window       lastParent = child;
	Window       parent = lastParent, root = None;
	Window       *children;
	unsigned int nchildren;

	/* Keep doing XQueryTree until we find a non-root parent */
	while (parent != root)
	{
	    lastParent = parent;
	    XQueryTree (dpy, lastParent, &root, &parent,
			&children, &nchildren);

	    XFree (children);
	}

	return lastParent;
    }
}

class CompizXorgSystemShapeHandling :
    public ct::AutostartCompizXorgSystemTest
{
    public:

	virtual void SetUp ()
	{
	    ct::AutostartCompizXorgSystemTest::SetUp ();

	    ::Display *dpy = Display ();
	    XSelectInput (dpy, DefaultRootWindow (dpy),
			  SubstructureNotifyMask | PropertyChangeMask);
	    XShapeQueryExtension (dpy, &shapeBase, &shapeError);

	    shapeEvent = shapeBase + ShapeNotify;

	    w = ct::CreateNormalWindow (dpy);
	    XMapRaised (dpy, w);

	    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w, ReparentNotify, -1, -1)));
	    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w, MapNotify, -1, -1)));

	    /* Now that we've received the ReparentNotify, find the toplevel parent */
	    parent = FindTopLevelParent (dpy, w);

	    /* Select for ShapeNotify on the parent window */
	    XShapeSelectInput (dpy, parent, ShapeNotifyMask);
	}

	int shapeBase;
	int shapeError;
	int shapeEvent;
	Window w;
	Window parent;
};

TEST_F (CompizXorgSystemShapeHandling, TestParentInputShapeSet)
{
    ::Display *dpy = Display ();

    /* Change the input shape on the child window to None */
    XShapeCombineRectangles (dpy, w, ShapeInput, 0, 0, NULL, 0, ShapeSet, 0);

    ct::ShapeNotifyXEventMatcher matcher (ShapeInput, 0, 0, 0, 0, True);

    ASSERT_TRUE (Advance (dpy,
			  ct::WaitForEventOfTypeOnWindowMatching (dpy,
								  parent,
								  shapeEvent,
								  -1,
								  0,
								  matcher)));

    /* Query Input Shape - should be no rectangles */
    int n, ordering;
    ASSERT_THAT (XShapeGetRectangles (dpy, parent, ShapeInput, &n, &ordering),
		 IsNull ());
    EXPECT_EQ (0, n);

    /* Query Bounding Shape - should be one rectangle */
    XRectangle *rects =
	XShapeGetRectangles (dpy, parent, ShapeBounding, &n, &ordering);
    ASSERT_THAT (rects, NotNull ());
    ASSERT_EQ (1, n);
    EXPECT_EQ (ct::WINDOW_X, rects->x);
    EXPECT_EQ (ct::WINDOW_Y, rects->y);
    EXPECT_EQ (ct::WINDOW_WIDTH, rects->width);
    EXPECT_EQ (ct::WINDOW_HEIGHT, rects->height);
}

TEST_F (CompizXorgSystemShapeHandling, TestParentBoundingShapeSet)
{
    ::Display *dpy = Display ();

    /* Change the input shape on the child window to None */
    XShapeCombineRectangles (dpy,
			     w,
			     ShapeBounding,
			     0,
			     0,
			     NULL,
			     0,
			     ShapeSet,
			     0);

    ct::ShapeNotifyXEventMatcher matcher (ShapeBounding, 0, 0, 0, 0, True);

    ASSERT_TRUE (Advance (dpy,
			  ct::WaitForEventOfTypeOnWindowMatching (dpy,
								  parent,
								  shapeEvent,
								  -1,
								  0,
								  matcher)));

    /* Query Bounding Shape - should be no rectangles */
    int n, ordering;
    ASSERT_THAT (XShapeGetRectangles (dpy, parent, ShapeBounding, &n, &ordering),
		 IsNull ());
    EXPECT_EQ (0, n);

    /* Query Input Shape - should be one rectangle */
    XRectangle *rects =
	XShapeGetRectangles (dpy, parent, ShapeInput, &n, &ordering);
    ASSERT_THAT (rects, NotNull ());
    ASSERT_EQ (1, n);
    EXPECT_EQ (ct::WINDOW_X, rects->x);
    EXPECT_EQ (ct::WINDOW_Y, rects->y);
    EXPECT_EQ (ct::WINDOW_WIDTH, rects->width);
    EXPECT_EQ (ct::WINDOW_HEIGHT, rects->height);
}
