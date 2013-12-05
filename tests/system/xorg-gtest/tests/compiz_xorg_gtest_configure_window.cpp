/*
 * Compiz XOrg GTest, ConfigureWindow handling
 *
 * Copyright (C) 2012 Sam Spilsbury (smspillaz@gmail.com)
 *
* Permission to use, copy, modify, distribute, and sell this software
* and its documentation for any purpose is hereby granted without
* fee, provided that the above copyright notice appear in all copies
* and that both that copyright notice and this permission notice
* appear in supporting documentation, and that the name of
* Novell, Inc. not be used in advertising or publicity pertaining to
* distribution of the software without specific, written prior permission.
* Novell, Inc. makes no representations about the suitability of this
* software for any purpose. It is provided "as is" without express or
* implied warranty.
*
* NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
* INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
* NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
* CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
* OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
* WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authored By:
 * Sam Spilsbury <smspillaz@gmail.com>
 */
#include <list>
#include <string>
#include <stdexcept>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <xorg/gtest/xorg-gtest.h>
#include <compiz-xorg-gtest.h>
#include <compiz_xorg_gtest_communicator.h>

#include <gtest_shared_tmpenv.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

using ::testing::MatchResultListener;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::_;

using ::compiz::testing::HasGeometry;
using ::compiz::testing::RelativeWindowGeometry;
using ::compiz::testing::AbsoluteWindowGeometry;

namespace ct = compiz::testing;

namespace
{

bool Advance (Display *d, bool r)
{
    return ct::AdvanceToNextEventOnSuccess (d, r);
}


Window GetTopParent (Display *display,
		     Window w)
{
    return ct::GetTopmostNonRootParent (display, w);
}

bool QueryGeometry (Display *dpy,
		    Window w,
		    int &x,
		    int &y,
		    unsigned int &width,
		    unsigned int &height)
{
    Window rootRet;
    unsigned int  depth, border;

    if (!XGetGeometry (dpy,
		       w,
		       &rootRet,
		       &x,
		       &y,
		       &width,
		       &height,
		       &depth,
		       &border))
	return false;

    return true;
}

bool WaitForReparentAndMap (Display *dpy,
			    Window w)
{
    bool ret = Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy,
							     w,
							     ReparentNotify,
							     -1,
							     -1));
    EXPECT_TRUE (ret);
    if (!ret)
	return false;


    ret = Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy,
							w,
							MapNotify,
							-1,
							-1));
    EXPECT_TRUE (ret);
    if (!ret)
	return false;

    return true;
}

struct ReparentedWindow
{
    Window client;
    Window frame;
};

typedef boost::function <void (Window)> CreateWaitFunc;

ReparentedWindow
GetNewWindowAndFrame (Display *dpy, const CreateWaitFunc &waitForCreation)
{
    ReparentedWindow w;

    w.client = ct::CreateNormalWindow (dpy);
    waitForCreation (w.client);

    XMapRaised (dpy, w.client);
    WaitForReparentAndMap (dpy, w.client);

    w.frame = GetTopParent (dpy, w.client);

    XSelectInput (dpy, w.frame, StructureNotifyMask);

    return w;
}

bool
WaitForConfigureNotify (Display *dpy,
			Window  w,
			int     x,
			int     y,
			int     width,
			int     height,
			int     border,
			Window  above,
			unsigned int mask)
{
    ct::ConfigureNotifyXEventMatcher matcher (above, border, x, y, width, height,
					      mask);

    return Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								 w,
								 ConfigureNotify,
								 -1,
								 -1,
								 matcher));
}

}

class CompizXorgSystemConfigureWindowTest :
    public ct::AutostartCompizXorgSystemTestWithTestHelper
{
    public:

	CompizXorgSystemConfigureWindowTest () :
	    /* See note in the acceptance tests about this */
	    env ("XORG_GTEST_CHILD_STDOUT", "1")
	{
	}

	void SendConfigureRequest (Window w, int x, int y, int width, int height, int mask);
	void SendSetFrameExtentsRequest (Window w, int left, int right, int top, int bottom);
	void SendConfigureLockRequest (Window w, bool lockRequests);
	bool VerifyConfigureResponse (Window w, int x, int y, int width, int height);
	bool VerifySetFrameExtentsResponse (Window w, int left, int right, int top, int bottom);

	/* Helper functions for the Create*WindowOverrideRedirect* tests */
	Window GrabAndCreateWindowWithAttrs (::Display *dpy,
					     XSetWindowAttributes &attr);
	void UngrabSyncAndTestOverride (::Display *dpy,
					Window w,
					bool overrideAssert);


    protected:

	ReparentedWindow CreateWindow (::Display *);
	int GetEventMask () const;

    private:

	TmpEnv env;
};

int
CompizXorgSystemConfigureWindowTest::GetEventMask () const
{
    return ct::AutostartCompizXorgSystemTestWithTestHelper::GetEventMask () |
	    SubstructureNotifyMask;
}

void
CompizXorgSystemConfigureWindowTest::SendConfigureRequest (Window w,
							   int x,
							   int y,
							   int width,
							   int height,
							   int mask)
{
    ::Display *dpy = Display ();

    std::vector <long> data;
    data.push_back (x);
    data.push_back (y);
    data.push_back (width);
    data.push_back (height);
    data.push_back (mask);

    ct::SendClientMessage (dpy,
			   FetchAtom (ct::messages::TEST_HELPER_CONFIGURE_WINDOW),
			   DefaultRootWindow (dpy),
			   w,
			   data);
}

void
CompizXorgSystemConfigureWindowTest::SendSetFrameExtentsRequest (Window w,
								 int    left,
								 int    right,
								 int    top,
								 int    bottom)
{
    ::Display *dpy = Display ();

    std::vector <long> data;
    data.push_back (left);
    data.push_back (right);
    data.push_back (top);
    data.push_back (bottom);

    ct::SendClientMessage (dpy,
			   FetchAtom (ct::messages::TEST_HELPER_CHANGE_FRAME_EXTENTS),
			   DefaultRootWindow (dpy),
			   w,
			   data);
}

void
CompizXorgSystemConfigureWindowTest::SendConfigureLockRequest (Window w,
							       bool   lockRequests)
{
    ::Display *dpy = Display ();

    std::vector <long> data;
    data.push_back (lockRequests);

    ct::SendClientMessage (dpy,
			   FetchAtom (ct::messages::TEST_HELPER_LOCK_CONFIGURE_REQUESTS),
			   DefaultRootWindow (dpy),
			   w,
			   data);
}

bool
CompizXorgSystemConfigureWindowTest::VerifyConfigureResponse (Window w,
							      int x,
							      int y,
							      int width,
							      int height)
{
    ::Display *dpy = Display ();
    XEvent event;

    while (ct::ReceiveMessage (dpy,
			       FetchAtom (ct::messages::TEST_HELPER_WINDOW_CONFIGURE_PROCESSED),
			       event))
    {
	bool requestAcknowledged =
		x == event.xclient.data.l[0] &&
		y == event.xclient.data.l[1] &&
		width == event.xclient.data.l[2] &&
		height == event.xclient.data.l[3];

	if (requestAcknowledged)
	    return true;

    }

    return false;
}

bool
CompizXorgSystemConfigureWindowTest::VerifySetFrameExtentsResponse (Window w,
								    int left,
								    int right,
								    int top,
								    int bottom)
{
    ::Display *dpy = Display ();
    XEvent event;

    while (ct::ReceiveMessage (dpy,
			       FetchAtom (ct::messages::TEST_HELPER_FRAME_EXTENTS_CHANGED),
			       event))
    {
	bool requestAcknowledged =
		left == event.xclient.data.l[0] &&
		right == event.xclient.data.l[1] &&
		top == event.xclient.data.l[2] &&
		bottom == event.xclient.data.l[3];

	if (requestAcknowledged)
	    return true;

    }

    return false;
}

ReparentedWindow
CompizXorgSystemConfigureWindowTest::CreateWindow (::Display *dpy)
{
    return GetNewWindowAndFrame (dpy,
				 boost::bind (&CompizXorgSystemConfigureWindowTest::WaitForWindowCreation,
					      this,
					      _1));
}

TEST_F (CompizXorgSystemConfigureWindowTest, ConfigureAndReponseUnlocked)
{
    ::Display *dpy = Display ();

    int x = 1;
    int y = 1;
    int width = 100;
    int height = 200;
    int mask = CWX | CWY | CWWidth | CWHeight;

    ReparentedWindow w = CreateWindow (dpy);

    SendConfigureRequest (w.client, x, y, width, height, mask);

    /* Wait for a response */
    ASSERT_TRUE (VerifyConfigureResponse (w.client, x, y, width, height));

    /* Query the window size again */
    ASSERT_THAT (w.frame, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       x,
				       y,
				       width,
				       height,
				       _));

}

TEST_F (CompizXorgSystemConfigureWindowTest, FrameExtentsAndReponseUnlocked)
{
    ::Display *dpy = Display ();

    int left = 1;
    int right = 2;
    int top = 3;
    int bottom = 4;

    ReparentedWindow w = CreateWindow (dpy);

    int x, y;
    unsigned int width, height;
    ASSERT_TRUE (QueryGeometry (dpy, w.client, x, y, width, height));

    SendSetFrameExtentsRequest (w.client, left, right, top, bottom);

    /* Wait for a response */
    ASSERT_TRUE (VerifySetFrameExtentsResponse (w.client, left, right, top, bottom));

    /* Client geometry is always unchanged */
    ASSERT_THAT (w.client, HasGeometry (dpy,
					RelativeWindowGeometry,
					x,
					y,
					width,
					height,
				       _));

    /* Frame geometry is frame geometry offset by extents */
    x -= left;
    y -= top;
    width += left + right;
    height += top + bottom;

    ASSERT_THAT (w.frame, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       x,
				       y,
				       width,
				       height,
				       _));
}

TEST_F (CompizXorgSystemConfigureWindowTest, MoveFrameLocked)
{
    ::Display *dpy = Display ();

    int x = 1;
    int y = 1;
    int width = 0; int height = 0;
    int mask = CWX | CWY;

    ReparentedWindow w = CreateWindow (dpy);

    int currentX, currentY;
    unsigned int currentWidth, currentHeight;
    ASSERT_TRUE (QueryGeometry (dpy,
				w.frame,
				currentX,
				currentY,
				currentWidth,
				currentHeight));

    SendConfigureLockRequest (w.client, true);
    SendConfigureRequest (w.client, x, y, width, height, mask);

    /* Wait for a response */
    ASSERT_TRUE (VerifyConfigureResponse (w.client, x, y, width, height));

    /* Query the window size again - it should be the same */
    ASSERT_THAT (w.frame, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       currentX,
				       currentY,
				       currentWidth,
				       currentHeight,
				       _));


    SendConfigureLockRequest (w.client, false);

    /* Expect buffer to be released */
    ASSERT_TRUE (WaitForConfigureNotify (dpy,
					 w.frame,
					 x,
					 y,
					 0,
					 0,
					 0,
					 0,
					 mask));

}

TEST_F (CompizXorgSystemConfigureWindowTest, ResizeFrameLocked)
{
    ::Display *dpy = Display ();

    int x = 0;
    int y = 0;
    int width = 200; int height = 300;
    int mask = CWWidth | CWHeight;

    ReparentedWindow w = CreateWindow (dpy);

    int currentX, currentY;
    unsigned int currentWidth, currentHeight;
    ASSERT_TRUE (QueryGeometry (dpy,
				w.frame,
				currentX,
				currentY,
				currentWidth,
				currentHeight));

    SendConfigureLockRequest (w.client, true);
    SendConfigureRequest (w.client, x, y, width, height, mask);

    /* Expect buffer to be released immediately */
    ASSERT_TRUE (WaitForConfigureNotify (dpy,
					 w.frame,
					 0,
					 0,
					 width,
					 height,
					 0,
					 0,
					 mask));

    /* Wait for a response */
    ASSERT_TRUE (VerifyConfigureResponse (w.client, x, y, width, height));

    SendConfigureLockRequest (w.client, false);

    /* Query the window size again - it should be the same */
    ASSERT_THAT (w.frame, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       currentX,
				       currentY,
				       width,
				       height,
				       _));
}

TEST_F (CompizXorgSystemConfigureWindowTest, SetFrameExtentsLocked)
{
    ::Display *dpy = Display ();

    /* Give the client window a 1px border, this will cause
     * the client to move within the frame window by 1, 1 ,
     * the frame window to move by -1, -1  and resize by 2, 2 */
    int left = 1;
    int right = 1;
    int top = 1;
    int bottom = 1;
    int frameMask = CWX | CWY | CWWidth | CWHeight;

    ReparentedWindow w = CreateWindow (dpy);

    int currentX, currentY;
    unsigned int currentWidth, currentHeight;
    ASSERT_TRUE (QueryGeometry (dpy,
				w.frame,
				currentX,
				currentY,
				currentWidth,
				currentHeight));

    SendConfigureLockRequest (w.client, true);
    SendSetFrameExtentsRequest (w.client, left, right, top, bottom);

    /* Expect buffer to be released immediately */
    ASSERT_TRUE (WaitForConfigureNotify (dpy,
					 w.frame,
					 currentX - left,
					 currentY - top,
					 currentWidth + (left + right),
					 currentHeight + (top + bottom),
					 0,
					 0,
					 frameMask));


    /* Wait for a response */
    ASSERT_TRUE (VerifySetFrameExtentsResponse (w.client, left, right, top, bottom));

    SendConfigureLockRequest (w.client, false);

    /* Query the window size again - it should be the same */
    ASSERT_THAT (w.frame, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       currentX - left,
				       currentY - top,
				       currentWidth + (left + right),
				       currentHeight + (top + bottom),
				       _));
}

/* Check that changing the frame extents by one on each side
 * adjusts the wrapper window appropriately */
TEST_F (CompizXorgSystemConfigureWindowTest, SetFrameExtentsAdjWrapperWindow)
{
    ::Display *dpy = Display ();

    ReparentedWindow w = CreateWindow (dpy);

    int currentX, currentY;
    unsigned int currentWidth, currentHeight;
    ASSERT_TRUE (QueryGeometry (dpy,
				w.frame,
				currentX,
				currentY,
				currentWidth,
				currentHeight));

    /* Set frame extents and get a response */
    int left = 1;
    int right = 1;
    int top = 1;
    int bottom = 1;

    SendSetFrameExtentsRequest (w.client, left, right, top, bottom);
    ASSERT_TRUE (VerifySetFrameExtentsResponse (w.client, left, right, top, bottom));

    /* Wrapper geometry is extents.xy, size.wh */
    Window root;
    Window wrapper = ct::GetImmediateParent (dpy, w.client, root);
    ASSERT_THAT (wrapper, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       left,
				       top,
				       currentWidth,
				       currentHeight,
				       _));
}

TEST_F (CompizXorgSystemConfigureWindowTest, SetFrameExtentsUnmapped)
{
    ::Display *dpy = Display ();

    Window client = ct::CreateNormalWindow (dpy);
    WaitForWindowCreation (client);

    /* Set frame extents and get a response */
    int left = 1;
    int right = 1;
    int top = 1;
    int bottom = 1;

    int currentX, currentY;
    unsigned int currentWidth, currentHeight;
    ASSERT_TRUE (QueryGeometry (dpy,
				client,
				currentX,
				currentY,
				currentWidth,
				currentHeight));

    /* We should get a response with our frame extents but it shouldn't actually
     * do anything to the client as it is unmapped */
    SendSetFrameExtentsRequest (client, left, right, top, bottom);
    ASSERT_TRUE (VerifySetFrameExtentsResponse (client, left, right, top, bottom));

    ASSERT_THAT (client, HasGeometry (dpy,
				      RelativeWindowGeometry,
				      currentX,
				      currentY,
				      currentWidth,
				      currentHeight,
				      _));
}

TEST_F (CompizXorgSystemConfigureWindowTest, SetFrameExtentsCorrectMapBehaviour)
{
    ::Display *dpy = Display ();

    Window client = ct::CreateNormalWindow (dpy);
    WaitForWindowCreation (client);

    /* Set frame extents and get a response */
    int left = 1;
    int right = 1;
    int top = 1;
    int bottom = 1;

    int currentX, currentY;
    unsigned int currentWidth, currentHeight;
    ASSERT_TRUE (QueryGeometry (dpy,
				client,
				currentX,
				currentY,
				currentWidth,
				currentHeight));

    SendSetFrameExtentsRequest (client, left, right, top, bottom);
    ASSERT_TRUE (VerifySetFrameExtentsResponse (client, left, right, top, bottom));

    /* Map the window */
    XMapRaised (dpy, client);
    WaitForReparentAndMap (dpy, client);

    /* Check the geometry of the frame */
    Window frame = GetTopParent (dpy, client);
    ASSERT_THAT (frame, HasGeometry (dpy,
				     RelativeWindowGeometry,
				     currentX - left,
				     currentY - top,
				     currentWidth + (left + right),
				     currentHeight + (top + bottom),
				     _));

    /* Wrapper geometry is extents.xy, size.wh */
    Window root;
    Window wrapper = ct::GetImmediateParent (dpy, client, root);
    ASSERT_THAT (wrapper, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       left,
				       top,
				       currentWidth,
				       currentHeight,
				       _));
}

TEST_F (CompizXorgSystemConfigureWindowTest, SetFrameExtentsConsistentBehaviourAfterMap)
{
    ::Display *dpy = Display ();

    Window client = ct::CreateNormalWindow (dpy);
    WaitForWindowCreation (client);

    /* Set frame extents and get a response */
    int left = 1;
    int right = 1;
    int top = 1;
    int bottom = 1;

    int currentX, currentY;
    unsigned int currentWidth, currentHeight;
    ASSERT_TRUE (QueryGeometry (dpy,
				client,
				currentX,
				currentY,
				currentWidth,
				currentHeight));

    SendSetFrameExtentsRequest (client, left, right, top, bottom);
    
    ASSERT_TRUE (VerifySetFrameExtentsResponse (client, left, right, top, bottom));

    /* Map the window */
    XMapRaised (dpy, client);
    WaitForReparentAndMap (dpy, client);

    /* Send it another frame extents request */
    right = right + 1;
    bottom = bottom + 1;

    SendSetFrameExtentsRequest (client, left, right, top, bottom);
    ASSERT_TRUE (VerifySetFrameExtentsResponse (client, left, right, top, bottom));

    /* Check the geometry of the frame */
    Window frame = GetTopParent (dpy, client);
    ASSERT_THAT (frame, HasGeometry (dpy,
				     RelativeWindowGeometry,
				     currentX - left,
				     currentY - top,
				     currentWidth + (left + right),
				     currentHeight + (top + bottom),
				     _));

    /* Wrapper geometry is extents.xy, size.wh */
    Window root;
    Window wrapper = ct::GetImmediateParent (dpy, client, root);
    ASSERT_THAT (wrapper, HasGeometry (dpy,
				       RelativeWindowGeometry,
				       left,
				       top,
				       currentWidth,
				       currentHeight,
				       _));
}


Window
CompizXorgSystemConfigureWindowTest::GrabAndCreateWindowWithAttrs (::Display *dpy,
								   XSetWindowAttributes &attr)
{
    /* NOTE: We need to ungrab the server after this function */
    XGrabServer (dpy);

    /* Create a window with our custom attributes */
    return XCreateWindow (dpy, DefaultRootWindow (dpy),
			  0, 0, 100, 100, 0, CopyFromParent,
			  InputOutput, CopyFromParent, CWOverrideRedirect,
			  &attr);
}

void
CompizXorgSystemConfigureWindowTest::UngrabSyncAndTestOverride (::Display *dpy,
								Window w,
								bool overrideAssert)
{
    XSync (dpy, false);

    XUngrabServer (dpy);

    XSync (dpy, false);

    /* Check if the created window had the attributes passed correctly */
    std::vector <long> data = WaitForWindowCreation (w);
    EXPECT_EQ (overrideAssert, IsOverrideRedirect (data));
}


TEST_F (CompizXorgSystemConfigureWindowTest, CreateDestroyWindowOverrideRedirectTrue)
{
    ::Display *dpy = Display ();
    XSetWindowAttributes attr;
    attr.override_redirect = 1;

    Window w = GrabAndCreateWindowWithAttrs (dpy, attr);

    XDestroyWindow (dpy, w);

    UngrabSyncAndTestOverride (dpy, w, true);
}

TEST_F (CompizXorgSystemConfigureWindowTest, CreateDestroyWindowOverrideRedirectFalse)
{
    ::Display *dpy = Display ();
    XSetWindowAttributes attr;
    attr.override_redirect = 0;

    Window w = GrabAndCreateWindowWithAttrs (dpy, attr);

    XDestroyWindow (dpy, w);

    UngrabSyncAndTestOverride (dpy, w, false);
}

TEST_F (CompizXorgSystemConfigureWindowTest, CreateChangeWindowOverrideRedirectTrue)
{
    ::Display *dpy = Display ();
    XSetWindowAttributes attr;
    attr.override_redirect = 0;

    Window w = GrabAndCreateWindowWithAttrs (dpy, attr);

    attr.override_redirect = 1;
    XChangeWindowAttributes (dpy, w, CWOverrideRedirect, &attr);

    UngrabSyncAndTestOverride (dpy, w, true);

    XDestroyWindow (dpy, w);
}

TEST_F (CompizXorgSystemConfigureWindowTest, CreateChangeWindowOverrideRedirectFalse)
{
    ::Display *dpy = Display ();
    XSetWindowAttributes attr;
    attr.override_redirect = 1;

    Window w = GrabAndCreateWindowWithAttrs (dpy, attr);

    attr.override_redirect = 0;
    XChangeWindowAttributes (dpy, w, CWOverrideRedirect, &attr);

    UngrabSyncAndTestOverride (dpy, w, false);

    XDestroyWindow (dpy, w);
}
