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

#include <boost/shared_array.hpp>
#include <boost/bind.hpp>

#include <xorg/gtest/xorg-gtest.h>
#include <compiz-xorg-gtest.h>
#include <compiz_xorg_gtest_communicator.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

using ::testing::MatchResultListener;
using ::testing::MakeMatcher;
using ::testing::Matcher;

namespace ct = compiz::testing;
namespace ctm = compiz::testing::messages;

class CompizXorgSystemStackingTest :
    public ct::AutostartCompizXorgSystemTest
{
    public:

	virtual void SetUp ()
	{
	    ct::AutostartCompizXorgSystemTest::SetUp ();

	    ::Display *dpy = Display ();
	    XSelectInput (dpy, DefaultRootWindow (dpy), SubstructureNotifyMask | PropertyChangeMask);
	}
};

namespace
{
    bool Advance (Display *dpy,
		  bool waitResult)
    {
	return ct::AdvanceToNextEventOnSuccess (dpy, waitResult);
    }

    void MakeDock (Display *dpy, Window w)
    {
	Atom _NET_WM_WINDOW_TYPE = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", false);
	Atom _NET_WM_WINDOW_TYPE_DOCK = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DOCK", false);

	XChangeProperty (dpy,
			 w,
			 _NET_WM_WINDOW_TYPE,
			 XA_ATOM,
			 32,
			 PropModeReplace,
			 reinterpret_cast <const unsigned char *> (&_NET_WM_WINDOW_TYPE_DOCK),
			 1);
    }

    void SetUserTime (Display *dpy, Window w, Time time)
    {
	Atom _NET_WM_USER_TIME = XInternAtom (dpy, "_NET_WM_USER_TIME", false);
	unsigned int value = (unsigned int) time;

	XChangeProperty (dpy, w,
			 _NET_WM_USER_TIME,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &value, 1);
    }

    void SetClientLeader (Display *dpy, Window w, Window leader)
    {
	Atom WM_CLIENT_LEADER = XInternAtom (dpy, "WM_CLIENT_LEADER", false);

	XChangeProperty (dpy, w,
			 WM_CLIENT_LEADER,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) &leader, 1);
    }
}

TEST_F (CompizXorgSystemStackingTest, TestSetup)
{
}

TEST_F (CompizXorgSystemStackingTest, TestCreateWindowsAndRestackRelativeToEachOther)
{
    ::Display *dpy = Display ();

    Window w1 = ct::CreateNormalWindow (dpy);
    Window w2 = ct::CreateNormalWindow (dpy);

    XMapRaised (dpy, w1);
    XMapRaised (dpy, w2);

    /* Both reparented and both mapped */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy,w1, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w1, MapNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w2, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w2, MapNotify, -1, -1)));

    ct::PropertyNotifyXEventMatcher matcher (dpy, "_NET_CLIENT_LIST_STACKING");

    /* Wait for property change notify on the root window to happen twice */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));

    /* Check the client list to see that w2 > w1 */
    std::list <Window> clientList = ct::NET_CLIENT_LIST_STACKING (dpy);

    ASSERT_EQ (clientList.size (), 2);
    EXPECT_EQ (clientList.front (), w1);
    EXPECT_EQ (clientList.back (), w2);
}

TEST_F (CompizXorgSystemStackingTest, TestCreateWindowsAndRestackRelativeToEachOtherDockAlwaysOnTop)
{
    ::Display *dpy = Display ();
    ct::PropertyNotifyXEventMatcher matcher (dpy, "_NET_CLIENT_LIST_STACKING");

    Window dock = ct::CreateNormalWindow (dpy);

    /* Make it a dock */
    MakeDock (dpy, dock);

    /* Immediately map the dock window and clear the event queue for it */
    XMapRaised (dpy, dock);

    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, dock, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, dock, MapNotify, -1, -1)));

    /* Dock window needs to be in the client list */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    std::list <Window> clientList = ct::NET_CLIENT_LIST_STACKING (dpy);
    ASSERT_EQ (clientList.size (), 1);

    Window w1 = ct::CreateNormalWindow (dpy);
    Window w2 = ct::CreateNormalWindow (dpy);

    XSelectInput (dpy, w2, StructureNotifyMask);

    XMapRaised (dpy, w1);
    XMapRaised (dpy, w2);

    /* Both reparented and both mapped */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w1, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w1, MapNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w2, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w2, MapNotify, -1, -1)));

    /* Wait for property change notify on the root window to happen twice */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    clientList = ct::NET_CLIENT_LIST_STACKING (dpy);

    /* Check the client list to see that dock > w2 > w1 */
    ASSERT_EQ (clientList.size (), 3);

    std::list <Window>::iterator it = clientList.begin ();

    EXPECT_EQ (*it++, w1); /* first should be the bottom normal window */
    EXPECT_EQ (*it++, w2); /* second should be the top normal window */
    EXPECT_EQ (*it++, dock); /* dock must always be on top */
}

TEST_F (CompizXorgSystemStackingTest, TestMapWindowWithOldUserTime)
{
    ::Display *dpy = Display ();
    ct::PropertyNotifyXEventMatcher matcher (dpy, "_NET_CLIENT_LIST_STACKING");

    Window w1 = ct::CreateNormalWindow (dpy);
    Window w2 = ct::CreateNormalWindow (dpy);
    Window w3 = ct::CreateNormalWindow (dpy);

    XMapRaised (dpy, w1);
    XMapRaised (dpy, w2);

    /* Wait for property change notify on the root window to happen twice */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));

    SetUserTime (dpy, w2, 200);
    SetClientLeader (dpy, w2, w2);
    SetUserTime (dpy, w3, 100);
    SetClientLeader (dpy, w3, w3);

    XMapRaised (dpy, w3);
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy, DefaultRootWindow (dpy), PropertyNotify, -1, -1, matcher)));

    /* Check the client list to see that w2 > w3 > w1 */
    std::list <Window> clientList = ct::NET_CLIENT_LIST_STACKING (dpy);
    ASSERT_EQ (clientList.size (), 3);

    std::list <Window>::iterator it = clientList.begin ();
    EXPECT_EQ (*it++, w1);
    EXPECT_EQ (*it++, w3);
    EXPECT_EQ (*it++, w2);
}

TEST_F (CompizXorgSystemStackingTest, TestMapWindowAndDenyFocus)
{
    ::Display *dpy = Display ();
    ct::PropertyNotifyXEventMatcher matcher (dpy, "_NET_CLIENT_LIST_STACKING");

    Window w1 = ct::CreateNormalWindow (dpy);
    Window w2 = ct::CreateNormalWindow (dpy);
    Window w3 = ct::CreateNormalWindow (dpy);

    XMapRaised (dpy, w1);
    XMapRaised (dpy, w2);

    /* Wait for property change notify on the root window to happen twice */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    SetUserTime (dpy, w3, 0);
    SetClientLeader (dpy, w3, w3);

    XMapRaised (dpy, w3);
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    /* Check the client list to see that w2 > w3 > w1 */
    std::list <Window> clientList = ct::NET_CLIENT_LIST_STACKING (dpy);
    ASSERT_EQ (clientList.size (), 3);

    std::list <Window>::iterator it = clientList.begin ();
    EXPECT_EQ (*it++, w1);
    EXPECT_EQ (*it++, w3);
    EXPECT_EQ (*it++, w2);
}

TEST_F (CompizXorgSystemStackingTest, TestCreateRelativeToDestroyedWindowFindsAnotherAppropriatePosition)
{
    ::Display *dpy = Display ();
    ct::PropertyNotifyXEventMatcher matcher (dpy, "_NET_CLIENT_LIST_STACKING");

    Window dock = ct::CreateNormalWindow (dpy);

    /* Make it a dock */
    MakeDock (dpy, dock);

    /* Immediately map the dock window and clear the event queue for it */
    XMapRaised (dpy, dock);

    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, dock, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, dock, MapNotify, -1, -1)));

    /* Dock window needs to be in the client list */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    std::list <Window> clientList = ct::NET_CLIENT_LIST_STACKING (dpy);
    ASSERT_EQ (clientList.size (), 1);

    Window w1 = ct::CreateNormalWindow (dpy);
    Window w2 = ct::CreateNormalWindow (dpy);

    XMapRaised (dpy, w1);

    /* All reparented and mapped */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy,w1, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w1, MapNotify, -1, -1)));

    /* Grab the server so that we can guarantee that all of these requests
     * happen before compiz gets them */
    XGrabServer (dpy);
    XSync (dpy, false);

    /* Map the second window, so it ideally goes above w1. Compiz will
     * receive the MapRequest for this first */
    XMapRaised (dpy, w2);

    /* Create window that has w2 as its ideal above-candidate
     * (compiz will receive the CreateNotify for this window
     *  after the MapRequest but before the subsequent MapNotify) */
    Window w3 = ct::CreateNormalWindow (dpy);

    XMapRaised (dpy, w3);

    /* Destroy w2 */
    XDestroyWindow (dpy, w2);

    XUngrabServer (dpy);
    XSync (dpy, false);

    /* Reparented and mapped */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w3, ReparentNotify, -1, -1)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindow (dpy, w3, MapNotify, -1, -1)));

    /* Update _NET_CLIENT_LIST_STACKING twice */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));

    /* Check the client list to see that dock > w3 > w1 */
    clientList = ct::NET_CLIENT_LIST_STACKING (dpy);

    std::list <Window>::iterator it = clientList.begin ();

    EXPECT_EQ (3, clientList.size ());
    EXPECT_EQ (w1, (*it++));
    EXPECT_EQ (w3, (*it++));
    EXPECT_EQ (dock, (*it++));
}

TEST_F(CompizXorgSystemStackingTest, TestWindowsDontStackAboveTransientForWindows)
{
    /* Here we are testing if new windows are being stacked above transientFor
     * windows. A problem is if a window is set to transientFor on a dock type
     * then a new window is mapped it set to be lower or above the transientFor
     * window causing it to be above the dock type window int the stack (which
     * is incorrect behavior)
     */
    ::Display *dpy = Display ();
    ct::PropertyNotifyXEventMatcher matcher (dpy, "_NET_CLIENT_LIST_STACKING");

    Window dock = ct::CreateNormalWindow (dpy);
    MakeDock (dpy, dock);

    XMapRaised (dpy, dock);

    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));

    Window w1 = ct::CreateNormalWindow (dpy);
    Window w2 = ct::CreateNormalWindow (dpy);

    XSetTransientForHint(dpy, w1, dock);

    XMapRaised (dpy, w1);
    XMapRaised (dpy, w2);

    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       matcher)));


    std::list <Window> clientList = ct::NET_CLIENT_LIST_STACKING (dpy);
    std::list <Window>::iterator it = clientList.begin ();

    /* Assert,  w1 > dock > w2 */
    EXPECT_EQ (3, clientList.size ());

    EXPECT_EQ (w2, (*it++));
    EXPECT_EQ (dock, (*it++));
    EXPECT_EQ (w1, (*it++));
}

class StackingSync :
    public ct::AutostartCompizXorgSystemTestWithTestHelper
{
    public:

	void SetUp ();
};

void
StackingSync::SetUp ()
{
    ct::AutostartCompizXorgSystemTestWithTestHelper::SetUp ();

    XSelectInput (Display (), DefaultRootWindow (Display ()), StructureNotifyMask |
							      SubstructureNotifyMask);
}

namespace
{
    Window CreateAndWaitForCreation (Display *dpy)
    {
	Window w = ct::CreateNormalWindow (dpy);
	Advance (dpy,
		 ct::WaitForEventOfTypeOnWindow (dpy,
						 DefaultRootWindow (dpy),
						 CreateNotify,
						 -1,
						 -1));
	return w;
    }

    Window CreateOverrideRedirectWindow (Display *dpy)
    {
	XSetWindowAttributes attrib;

	attrib.override_redirect = true;

	Window w =
	    XCreateWindow (dpy, DefaultRootWindow (dpy),
			   ct::WINDOW_X,
			   ct::WINDOW_Y,
			   ct::WINDOW_WIDTH,
			   ct::WINDOW_HEIGHT,
			   0,
			   DefaultDepth (dpy, 0),
			   InputOutput,
			   DefaultVisual (dpy, DefaultScreen (dpy)),
			   CWOverrideRedirect,
			   &attrib);

	XSelectInput (dpy, w, StructureNotifyMask);
	return w;
    }

    Window MapAndWaitForParent (Display *dpy, Window w)
    {
	XMapRaised (dpy, w);

	Advance (dpy,
		 ct::WaitForEventOfTypeOnWindow (dpy,
						 w,
						 ReparentNotify,
						 -1,
						 -1));

	return ct::GetTopmostNonRootParent (dpy, w);
    }

    void DestroyOnReparent (Display          *dpy,
			    ct::MessageAtoms &atoms,
			    Window           w)
    {
	Window root = DefaultRootWindow (dpy);
	Atom   atom =
	    atoms.FetchForString (ctm::TEST_HELPER_DESTROY_ON_REPARENT);

	std::vector <long> data (4);

	ct::SendClientMessage (dpy, atom, root, w, data);
    }

    Window WaitForNextCreatedWindow (Display *dpy)
    {
	XEvent ev;
	while (true)
	{
	    XNextEvent (dpy, &ev);
	    if (ev.type == CreateNotify)
		return ev.xcreatewindow.window;
	}

	return 0;
    }

    void WaitForManage (Display          *dpy,
			ct::MessageAtoms &atoms)
    {
	Atom   atom =
	    atoms.FetchForString (ctm::TEST_HELPER_WINDOW_READY);
	XEvent ev;

	ct::ReceiveMessage (dpy, atom, ev);
    }

    void RestackAbove (Display *dpy,
		       Window  w,
		       Window  above)
    {
	XWindowChanges xwc;

	xwc.stack_mode = Above;
	xwc.sibling = above;

	XConfigureWindow (dpy, w, CWStackMode | CWSibling, &xwc);
    }

    void RestackAtLeastAbove (Display          *dpy,
			      ct::MessageAtoms &atoms,
			      Window           w,
			      Window           above)
    {
	Window root = DefaultRootWindow (dpy);
	Atom   atom =
	    atoms.FetchForString (ctm::TEST_HELPER_RESTACK_ATLEAST_ABOVE);

	std::vector <long> data (4);

	data[0] = above;
	ct::SendClientMessage (dpy, atom, root, w, data);
    }

    void FreeWindowArray (Window *array)
    {
	XFree (array);
    }

    typedef boost::shared_array <Window> WindowArray;

    WindowArray GetChildren (Display      *dpy,
			     Window       w,
			     unsigned int &n)
    {
	Window unused;
	Window *children;

	if (!XQueryTree (dpy, w, &unused, &unused, &children, &n))
	    throw std::logic_error ("XQueryTree failed");

	return WindowArray (children, boost::bind (FreeWindowArray, _1));
    }

    class StackPositionMatcher :
	public MatcherInterface <Window>
    {
	public:

	    StackPositionMatcher (const WindowArray &array,
				  Window            cmp,
				  unsigned int      n);

	protected:

	    bool MatchAndExplain (Window              window,
				  MatchResultListener *listener) const;
	    void DescribeTo (std::ostream *os) const;

	private:

	    virtual bool Compare (int lhsPos,
				  int rhsPos) const = 0;
	    virtual void ExplainCompare (std::ostream *os) const = 0;

	    WindowArray  array;
	    unsigned int arrayNum;
	    Window       cmp;

    };

    StackPositionMatcher::StackPositionMatcher (const WindowArray &array,
						Window            cmp,
						unsigned int      n) :
	array (array),
	arrayNum (n),
	cmp (cmp)
    {
    }

    bool
    StackPositionMatcher::MatchAndExplain (Window window,
					   MatchResultListener *listener) const
    {
	int lhsPos = -1, rhsPos = -1;

	for (unsigned int i = 0; i < arrayNum; ++i)
	{
	    if (array[i] == window)
		lhsPos = i;
	    if (array[i] == cmp)
		rhsPos = i;
	}

	if (lhsPos > -1 &&
	    rhsPos > -1)
	{
	    if (Compare (lhsPos, rhsPos))
		return true;
	}

	/* Match failed, add stack to MatchResultListener */
	if (listener->IsInterested ())
	{
	    std::stringstream windowStack;

	    windowStack << "Window Stack (bottom to top ["
			<< arrayNum
			<< "]): \n";
	    *listener << windowStack.str ();
	    for (unsigned int i = 0; i < arrayNum; ++i)
	    {
		std::stringstream ss;
		ss << " - 0x"
		   << std::hex
		   << array[i]
		   << std::dec
		   << std::endl;
		*listener << ss.str ();
	    }

	    std::stringstream lhsPosMsg, rhsPosMsg;

	    lhsPosMsg << "Position of 0x"
		      << std::hex
		      << window
		      << std::dec
		      << " : "
		      << lhsPos;

	    rhsPosMsg << "Position of 0x"
		      << std::hex
		      << cmp
		      << std::dec
		      << " : "
		      << rhsPos;

	    *listener << lhsPosMsg.str () << "\n";
	    *listener << rhsPosMsg.str () << "\n";
	}

	return false;
    }

    void
    StackPositionMatcher::DescribeTo (std::ostream *os) const
    {
	*os << "Window is ";
	ExplainCompare (os);
	*os << " in relation to 0x" << std::hex << cmp << std::dec;
    }

    class GreaterThanInStackMatcher :
	public StackPositionMatcher
    {
	public:

	    GreaterThanInStackMatcher (const WindowArray &array,
				       Window            cmp,
				       unsigned int      n);

	private:

	    bool Compare (int lhsPos, int rhsPos) const;
	    void ExplainCompare (std::ostream *os) const;
    };

    GreaterThanInStackMatcher::GreaterThanInStackMatcher (const WindowArray &array,
							  Window            cmp,
							  unsigned int      n) :
	StackPositionMatcher (array, cmp, n)
    {
    }

    bool GreaterThanInStackMatcher::Compare (int lhsPos, int rhsPos) const
    {
	return lhsPos > rhsPos;
    }

    void GreaterThanInStackMatcher::ExplainCompare (std::ostream *os) const
    {
	*os << "greater than";
    }

    class LessThanInStackMatcher :
	public StackPositionMatcher
    {
	public:

	    LessThanInStackMatcher (const WindowArray &array,
				    Window            cmp,
				    unsigned int      n);

	private:

	    bool Compare (int lhsPos, int rhsPos) const;
	    void ExplainCompare (std::ostream *os) const;
    };

    LessThanInStackMatcher::LessThanInStackMatcher (const WindowArray &array,
						    Window            cmp,
						    unsigned int      n) :
	StackPositionMatcher (array, cmp, n)
    {
    }

    bool LessThanInStackMatcher::Compare (int lhsPos, int rhsPos) const
    {
	return lhsPos < rhsPos;
    }

    void LessThanInStackMatcher::ExplainCompare (std::ostream *os) const
    {
	*os << "less than";
    }

    inline Matcher <Window> GreaterThanInStack (const WindowArray &array,
						unsigned int      n,
						Window            cmp)
    {
	return MakeMatcher (new GreaterThanInStackMatcher (array, cmp, n));
    }

    inline Matcher <Window> LessThanInStack (const WindowArray &array,
					     unsigned int      n,
					     Window            cmp)
    {
	return MakeMatcher (new LessThanInStackMatcher (array, cmp, n));
    }
}

TEST_F (StackingSync, DestroyClientJustBeforeReparent)
{
    ::Display *dpy = Display ();

    ct::MessageAtoms atoms (dpy);

    /* Set up three normal windows */
    Window w1 = CreateAndWaitForCreation (dpy);
    Window w2 = CreateAndWaitForCreation (dpy);
    Window w3 = CreateAndWaitForCreation (dpy);

    Window p1 = MapAndWaitForParent (dpy, w1);
    Window p2 = MapAndWaitForParent (dpy, w2);
    Window p3 = MapAndWaitForParent (dpy, w3);

    /* Create another normal window, but immediately mark
     * it destroyed within compiz as soon as it is reparented,
     * so that we force the reparented-but-destroyed strategy
     * to kick in */
    Window destroyed = CreateAndWaitForCreation (dpy);

    DestroyOnReparent (dpy, atoms, destroyed);
    XMapRaised (dpy, destroyed);

    /* Wait for the destroyed window's parent to be created
     * in the toplevel stack as a result of the reparent operation */
    Window parentOfDestroyed = WaitForNextCreatedWindow (dpy);
    WaitForManage (dpy, atoms);

    /* Create an override redirect window and wait for it to be
     * managed */
    Window override = CreateOverrideRedirectWindow (dpy);
    WaitForManage (dpy, atoms);

    /* Place the destroyed window's parent above
     * p1 in the stack directly */
    RestackAbove (dpy, parentOfDestroyed, p1);

    /* Ask compiz to place the override redirect window
     * at least above the destroyed window's parent
     * in the stack. This requires compiz to locate the
     * destroyed window's parent in the stack */
    RestackAtLeastAbove (dpy, atoms, override, parentOfDestroyed);

    /* Wait for the override window to be configured */
    Advance (dpy,
	     ct::WaitForEventOfTypeOnWindow (dpy,
					     override,
					     ConfigureNotify,
					     -1,
					     -1));

    unsigned int n;
    WindowArray  windows (GetChildren (dpy, DefaultRootWindow (dpy), n));

    EXPECT_THAT (p2, LessThanInStack (windows, n, override));
    EXPECT_THAT (p3, GreaterThanInStack (windows, n, override));
}
