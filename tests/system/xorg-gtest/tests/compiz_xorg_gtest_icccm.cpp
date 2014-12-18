/*
 * Compiz XOrg GTest, ICCCM compliance
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
#include <gtest_shared_asynctask.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

using ::testing::MatchResultListener;
using ::testing::MakeMatcher;
using ::testing::Matcher;

namespace ct = compiz::testing;

namespace
{

char TEST_FAILED_MSG = 's';
char PROCESS_EXITED_MSG = 'd';

bool Advance (Display *d, bool r)
{
    return ct::AdvanceToNextEventOnSuccess (d, r);
}

}

class WaitForSuccessDeathTask :
    public AsyncTask
{
    public:

	typedef boost::function <xorg::testing::Process::State ()> GetProcessState;

	WaitForSuccessDeathTask (const GetProcessState &procState) :
	    mProcessState (procState)
	{
	}
	virtual ~WaitForSuccessDeathTask () {}

    private:

	GetProcessState mProcessState;

	void Task ();
};

void
WaitForSuccessDeathTask::Task ()
{
    do
    {
	if (ReadMsgFromTest (TEST_FAILED_MSG, 1))
	    return;
    } while (mProcessState () != xorg::testing::Process::FINISHED_FAILURE);

    /* The process died, send a message back saying that it did */
    SendMsgToTest (PROCESS_EXITED_MSG);
}

class CompizXorgSystemICCCM :
    public ct::CompizXorgSystemTest
{
    public:

	CompizXorgSystemICCCM () :
	    /* See note in the acceptance tests about this */
	    env ("XORG_GTEST_CHILD_STDOUT", "1")
	{
	}

	virtual void SetUp ()
	{
	    ct::CompizXorgSystemTest::SetUp ();

	    ::Display *dpy = Display ();

	    XSelectInput (dpy, DefaultRootWindow (dpy),
			  StructureNotifyMask |
			  FocusChangeMask |
			  PropertyChangeMask |
			  SubstructureRedirectMask);
	}

    private:

	TmpEnv env;
};

TEST_F (CompizXorgSystemICCCM, SomeoneElseHasSubstructureRedirectMask)
{
    StartCompiz (static_cast <ct::CompizProcess::StartupFlags> (
		     ct::CompizProcess::ExpectStartupFailure |
		     ct::CompizProcess::ReplaceCurrentWM |
		     ct::CompizProcess::WaitForStartupMessage),
		 ct::CompizProcess::PluginList ());

    WaitForSuccessDeathTask::GetProcessState processState (boost::bind (&CompizXorgSystemICCCM::CompizProcessState,
								 this));
    AsyncTask::Ptr task (boost::make_shared <WaitForSuccessDeathTask> (processState));

    /* Now wait for the thread to tell us the news -
     * this will block for up to ten seconds */
    const int maximumWaitTime = 1000 * 10; // 10 seconds

    if (!task->ReadMsgFromTask (PROCESS_EXITED_MSG, maximumWaitTime))
    {
	task->SendMsgToTask (TEST_FAILED_MSG);
	throw std::runtime_error ("compiz process did not exit with failure status");
    }
}

class AutostartCompizXorgSystemICCCM :
    public ct::CompizXorgSystemTest
{
    public:

	virtual void SetUp ()
	{
	    ct::CompizXorgSystemTest::SetUp ();

	    ::Display *dpy = Display ();

	    XSelectInput (dpy, DefaultRootWindow (dpy),
			  StructureNotifyMask |
			  SubstructureNotifyMask |
			  FocusChangeMask |
			  PropertyChangeMask);

	    StartCompiz (static_cast <ct::CompizProcess::StartupFlags> (
			     ct::CompizProcess::ReplaceCurrentWM |
			     ct::CompizProcess::WaitForStartupMessage),
			  ct::CompizProcess::PluginList ());
	}
};

TEST_F (AutostartCompizXorgSystemICCCM, ConfigureRequestSendsBackAppropriateConfigureNotify)
{
    ::Display *dpy = Display ();
    Window    root = DefaultRootWindow (dpy);

    Window w1 = ct::CreateNormalWindow (dpy);
    Window w2 = ct::CreateNormalWindow (dpy);

    XMapRaised (dpy, w1);
    XMapRaised (dpy, w2);

    ct::PropertyNotifyXEventMatcher propertyMatcher (dpy, "_NET_CLIENT_LIST_STACKING");

    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       propertyMatcher)));
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       propertyMatcher)));

    /* Select for StructureNotify on w1 */
    XSelectInput (dpy, w1, StructureNotifyMask);

    /* Send a ConfigureRequest to the server asking for
     * a particular geometry and stack position relative
     * to w2 */

    XEvent                 event;
    XConfigureRequestEvent *configureRequestEvent = &event.xconfigurerequest;

    event.type = ConfigureRequest;

    const Window REQUEST_ABOVE = w2;
    const unsigned int REQUEST_BORDER_WIDTH = 0;
    const unsigned int REQUEST_X = 110;
    const unsigned int REQUEST_Y = 120;
    const unsigned int REQUEST_WIDTH = 200;
    const unsigned int REQUEST_HEIGHT = 200;

    configureRequestEvent->window = w1;

    configureRequestEvent->above = REQUEST_ABOVE;
    configureRequestEvent->detail = Above;
    configureRequestEvent->border_width = REQUEST_BORDER_WIDTH;
    configureRequestEvent->x = REQUEST_X;
    configureRequestEvent->y = REQUEST_Y;
    configureRequestEvent->width = REQUEST_WIDTH;
    configureRequestEvent->height = REQUEST_HEIGHT;

    configureRequestEvent->value_mask = CWX |
					CWY |
					CWWidth |
					CWHeight |
					CWBorderWidth |
					CWSibling |
					CWStackMode;

    XSendEvent (dpy,
		root,
		0,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&event);

    XFlush (dpy);

    /* Now wait for a ConfigureNotify to be sent back */
    ct::ConfigureNotifyXEventMatcher configureMatcher (0, // Always zero
						       REQUEST_BORDER_WIDTH,
						       REQUEST_X,
						       REQUEST_Y,
						       REQUEST_WIDTH,
						       REQUEST_HEIGHT,
						       configureRequestEvent->value_mask &
						       ~(CWSibling | CWStackMode));

    /* Should get a ConfigureNotify with the right parameters */
    EXPECT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       w1,
								       ConfigureNotify,
								       -1,
								       -1,
								       configureMatcher)));

    /* Check if the window is actually above */
    ASSERT_TRUE (Advance (dpy, ct::WaitForEventOfTypeOnWindowMatching (dpy,
								       DefaultRootWindow (dpy),
								       PropertyNotify,
								       -1,
								       -1,
								       propertyMatcher)));

    /* Check the client list to see that w1 > w2 */
    std::list <Window> clientList = ct::NET_CLIENT_LIST_STACKING (dpy);

    ASSERT_EQ (2, clientList.size ());
    EXPECT_EQ (w2, clientList.front ());
    EXPECT_EQ (w1, clientList.back ());
}
