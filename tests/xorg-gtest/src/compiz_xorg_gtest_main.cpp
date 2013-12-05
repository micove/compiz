/*
 * Compiz XOrg GTest Decoration Pixmap Protocol Integration Tests
 *
 * Copyright (C) 2013 Sam Spilsbury.
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

#include <csignal>

#include <sstream>
#include <stdexcept>

#include <gtest/gtest.h>
#include "xorg/gtest/xorg-gtest-environment.h"

#include <X11/Xlib.h>

namespace compiz
{
namespace testing
{
class XorgEnvironment :
    public xorg::testing::Environment
{
    public:

	virtual void SetUp ()
	{
	    /* Another hack - if the server fails
	     * to start then just set another display
	     * number until it does */
	    const int MaxConnections = 255;
	    int displayNumber = 0;
	    bool serverRunningOnDisplay = true;

	    while (serverRunningOnDisplay &&
		   displayNumber < MaxConnections)
	    {
		std::stringstream ss;
		ss << ":" << displayNumber;
		Display *check = XOpenDisplay (ss.str ().c_str ());

		if (!check)
		    serverRunningOnDisplay = false;
		else
		{
		    XCloseDisplay (check);
		    ++displayNumber;
		}
	    }

	    if (displayNumber == MaxConnections)
		throw std::runtime_error ("couldn't find a socket "
					  "to launch on");

	    std::stringstream logFile;
	    logFile << "/tmp/Compiz.Xorg.GTest." << displayNumber << ".log";

	    SetDisplayNumber (displayNumber);
	    SetLogFile (logFile.str ());
	    xorg::testing::Environment::SetUp ();
	}
};
}
}

/* X testing environment - Google Test environment feat. dummy x server
 * Copyright (C) 2011, 2012 Canonical Ltd.
 */
compiz::testing::XorgEnvironment *environment = NULL;

namespace
{

void SignalHandler (int signum)
{
    if (environment)
	environment->Kill ();

    /* This will call the default handler because we used SA_RESETHAND */
    raise (signum);
}

void SetupSignalHandlers ()
{
    static const int signals[] =
    {
	SIGHUP,
	SIGTERM,
	SIGQUIT,
	SIGILL,
	SIGABRT,
	SIGFPE,
	SIGSEGV,
	SIGPIPE,
	SIGALRM,
	SIGTERM,
	SIGUSR1,
	SIGUSR2,
	SIGBUS,
	SIGPOLL,
	SIGPROF,
	SIGSYS,
	SIGTRAP,
	SIGVTALRM,
	SIGXCPU,
	SIGXFSZ,
	SIGIOT,
	SIGSTKFLT,
	SIGIO,
	SIGPWR,
	SIGUNUSED,
    };

    struct sigaction action;
    action.sa_handler = SignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;

    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
	if (sigaction(signals[i], &action, NULL))
	    std::cerr << "Warning: Failed to set signal handler for signal "
		      << signals[i] << "\n";
}
}

int main (int argc, char **argv)
{
    ::testing::InitGoogleTest (&argc, argv);

    SetupSignalHandlers ();

    environment = new compiz::testing::XorgEnvironment ();
    ::testing::AddGlobalTestEnvironment (environment);

    return RUN_ALL_TESTS ();
}

