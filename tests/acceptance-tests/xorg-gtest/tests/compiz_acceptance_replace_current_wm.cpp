/*
 * Compiz XOrg GTest Acceptance Tests
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
#include <sys/types.h>
#include <signal.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <xorg/gtest/xorg-gtest.h>
#include <compiz-xorg-gtest.h>

#include <gtest_shared_tmpenv.h>
#include <gtest_shared_characterwrapper.h>
#include <gtest_shared_asynctask.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace ct = compiz::testing;

class XorgAcceptance :
    public xorg::testing::Test
{
    public:

	typedef boost::shared_ptr <ct::CompizProcess> ProcessPtr;
};

namespace
{

char TEST_FAILED_MSG = 's';
char PROCESS_EXITED_MSG = 'd';

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
    } while (mProcessState () != xorg::testing::Process::FINISHED_SUCCESS);

    /* The process died, send a message back saying that it did */
    SendMsgToTest (PROCESS_EXITED_MSG);
}

TEST_F (XorgAcceptance, SIGINTClosesDown)
{
    /* XXX: This is a bit stupid, but we have to do it.
     * It seems as though closing the child stdout or
     * stderr will cause the client to hang indefinitely
     * when the child calls XSync (and that can happen
     * implicitly, eg XCloseDisplay) */
    TmpEnv env ("XORG_GTEST_CHILD_STDOUT", "1");
    ProcessPtr compiz (boost::make_shared <ct::CompizProcess> (Display (),
							       ct::CompizProcess::WaitForStartupMessage,
							       ct::CompizProcess::PluginList ()));

    pid_t firstProcessPid = compiz->Pid ();

    WaitForSuccessDeathTask::GetProcessState procState (boost::bind (&ct::CompizProcess::State,
								     compiz.get ()));
    AsyncTask::Ptr task (boost::make_shared <WaitForSuccessDeathTask> (procState));

    kill (firstProcessPid, SIGINT);

    const unsigned int maximumWaitTime = 10 * 1000; // Ten seconds

    if (!task->ReadMsgFromTask (PROCESS_EXITED_MSG, maximumWaitTime))
    {
	task->SendMsgToTask (TEST_FAILED_MSG);
	throw std::runtime_error ("compiz process did not exit with success status");
    }
}

TEST_F (XorgAcceptance, ReplaceOtherWMFast)
{
    /* XXX: This is a bit stupid, but we have to do it.
     * It seems as though closing the child stdout or
     * stderr will cause the client to hang indefinitely
     * when the child calls XSync (and that can happen
     * implicitly, eg XCloseDisplay) */
    TmpEnv env ("XORG_GTEST_CHILD_STDOUT", "1");
    ProcessPtr firstCompiz (boost::make_shared <ct::CompizProcess> (Display (),
								    ct::CompizProcess::WaitForStartupMessage,
								    ct::CompizProcess::PluginList ()));

    /* Expect it to exit */
    WaitForSuccessDeathTask::GetProcessState procState (boost::bind (&ct::CompizProcess::State,
								     firstCompiz.get ()));
    AsyncTask::Ptr task (boost::make_shared <WaitForSuccessDeathTask> (procState));
    const unsigned int maximumWaitTime = 10 * 1000; // Ten seconds

    ProcessPtr secondCompiz (boost::make_shared <ct::CompizProcess> (Display (),
								     static_cast <ct::CompizProcess::StartupFlags> (
									ct::CompizProcess::WaitForStartupMessage |
									ct::CompizProcess::ReplaceCurrentWM),
								     ct::CompizProcess::PluginList (),
								     maximumWaitTime));

    if (!task->ReadMsgFromTask (PROCESS_EXITED_MSG, maximumWaitTime))
    {
	task->SendMsgToTask (TEST_FAILED_MSG);
	throw std::runtime_error ("compiz process did not exit with success status");
    }
}

namespace
{
char TEST_FINISHED_MSG = 's';
}

class SlowDownTask :
    public AsyncTask
{
    public:

	typedef boost::function <xorg::testing::Process::State ()> GetProcessState;
	typedef boost::function <pid_t ()> GetPid;

	SlowDownTask (const GetProcessState &procState,
		      const GetPid          &pid) :
	    mProcessState (procState),
	    mPid (pid),
	    mIsRunning (true)
	{
	}
	virtual ~SlowDownTask () {}

    private:

	GetProcessState mProcessState;
	GetPid          mPid;
	bool            mIsRunning;

	void Task ();
};

void
SlowDownTask::Task ()
{
    do
    {
	if (ReadMsgFromTest (TEST_FINISHED_MSG, mIsRunning ? 1 : 400))
	    return;

	pid_t pid = mPid ();

	if (pid)
	{
	    if (mIsRunning)
	    {
		kill (pid, SIGSTOP);
		mIsRunning = false;
	    }
	    else
	    {
		kill (pid, SIGCONT);
		mIsRunning = true;
	    }
	}

    } while (mProcessState () != xorg::testing::Process::FINISHED_SUCCESS);

    /* The process died, send a message back saying that it did */
    SendMsgToTest (PROCESS_EXITED_MSG);
}

TEST_F (XorgAcceptance, ReplaceOtherWMSlow)
{
    ::Display *dpy = Display ();

    /* XXX: This is a bit stupid, but we have to do it.
     * It seems as though closing the child stdout or
     * stderr will cause the client to hang indefinitely
     * when the child calls XSync (and that can happen
     * implicitly, eg XCloseDisplay) */
    TmpEnv env ("XORG_GTEST_CHILD_STDOUT", "1");
    ProcessPtr firstCompiz (boost::make_shared <ct::CompizProcess> (dpy,
								    ct::CompizProcess::WaitForStartupMessage,
								    ct::CompizProcess::PluginList (),
								    3000));

    SlowDownTask::GetProcessState procState (boost::bind (&ct::CompizProcess::State,
							  firstCompiz.get ()));
    SlowDownTask::GetPid getPid (boost::bind (&ct::CompizProcess::Pid,
					      firstCompiz.get ()));

    /* Slow down the first compiz */
    AsyncTask::Ptr task (boost::make_shared <SlowDownTask> (procState, getPid));

    /* Select for StructureNotifyMask */
    XSelectInput (dpy,
		  DefaultRootWindow (dpy),
		  StructureNotifyMask);

    const unsigned int maximumWaitTime = 20 * 1000; // Twenty seconds

    ProcessPtr secondCompiz (boost::make_shared <ct::CompizProcess> (Display (),
								     static_cast <ct::CompizProcess::StartupFlags> (
									ct::CompizProcess::ReplaceCurrentWM |
									ct::CompizProcess::WaitForStartupMessage),
								     ct::CompizProcess::PluginList (),
								     maximumWaitTime));

    /* Wait until the first one goes away */
    if (!task->ReadMsgFromTask (PROCESS_EXITED_MSG, maximumWaitTime))
    {
	task->SendMsgToTask (TEST_FINISHED_MSG);
	throw std::runtime_error ("compiz process did not exit with success status");
    }
}
