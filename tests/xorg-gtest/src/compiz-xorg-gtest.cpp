/*
 * Compiz XOrg GTest
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
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/shared_ptr.hpp>
#include <gtest_shared_tmpenv.h>
#include <gtest_shared_characterwrapper.h>
#include <xorg/gtest/xorg-gtest.h>
#include <compiz-xorg-gtest.h>
#include <compiz_xorg_gtest_communicator.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include "compiz-xorg-gtest-config.h"

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::MatcherInterface;
using ::testing::Matcher;

namespace ct = compiz::testing;
namespace
{
const int          WINDOW_X = ct::WINDOW_X;
const int          WINDOW_Y = ct::WINDOW_Y;
const unsigned int WINDOW_WIDTH = ct::WINDOW_HEIGHT;
const unsigned int WINDOW_HEIGHT = ct::WINDOW_HEIGHT;
const unsigned int WINDOW_BORDER = 0;
const unsigned int WINDOW_DEPTH = CopyFromParent;
const unsigned int WINDOW_CLASS = InputOutput;
Visual             *WINDOW_VISUAL = CopyFromParent;

const long                 WINDOW_ATTRIB_VALUE_MASK = 0;

void RemoveEventFromQueue (Display *dpy)
{
    XEvent event;

    if (XNextEvent (dpy, &event) != Success)
	throw std::runtime_error("Failed to remove X event");
}
}

Window
ct::CreateNormalWindow (Display *dpy)
{
    XSetWindowAttributes WINDOW_ATTRIB;
    Window w = XCreateWindow (dpy,
			      DefaultRootWindow (dpy),
			      WINDOW_X,
			      WINDOW_Y,
			      WINDOW_WIDTH,
			      WINDOW_HEIGHT,
			      WINDOW_BORDER,
			      WINDOW_DEPTH,
			      WINDOW_CLASS,
			      WINDOW_VISUAL,
			      WINDOW_ATTRIB_VALUE_MASK,
			      &WINDOW_ATTRIB);

    XSelectInput (dpy, w, StructureNotifyMask);
    return w;
}

Window
ct::GetImmediateParent (Display *display,
			Window w,
			Window &rootReturn)
{
    Window parentReturn = w;
    Window *childrenReturn;
    unsigned int nChildrenReturn;

    XQueryTree (display,
		w,
		&rootReturn,
		&parentReturn,
		&childrenReturn,
		&nChildrenReturn);
    XFree (childrenReturn);

    return parentReturn;
}

Window
ct::GetTopmostNonRootParent (Display *display,
			     Window  w)
{
    Window rootReturn = 0;
    Window parentReturn = w;
    Window lastParent = 0;

    do
    {
	lastParent = parentReturn;

	parentReturn = GetImmediateParent (display,
					   lastParent,
					   rootReturn);

    } while (parentReturn != rootReturn);

    return lastParent;
}

bool
ct::AdvanceToNextEventOnSuccess (Display *dpy,
				 bool waitResult)
{
    if (waitResult)
	RemoveEventFromQueue (dpy);

    return waitResult;
}

bool
ct::WaitForEventOfTypeOnWindow (Display *dpy,
				Window  w,
				int     type,
				int     ext,
				int     extType,
				int     timeout)
{
    while (xorg::testing::XServer::WaitForEventOfType (dpy, type, ext, extType, timeout))
    {
	XEvent event;
	if (!XPeekEvent (dpy, &event))
	    throw std::runtime_error ("Failed to peek event");

	if (event.xany.window != w)
	{
	    RemoveEventFromQueue (dpy);
	    continue;
	}

	return true;
    }

    return false;
}

bool
ct::WaitForEventOfTypeOnWindowMatching (Display             *dpy,
					Window              w,
					int                 type,
					int                 ext,
					int                 extType,
					const XEventMatcher &matcher,
					int                 timeout)
{
    while (ct::WaitForEventOfTypeOnWindow (dpy, w, type, ext, extType, timeout))
    {
	XEvent event;
	if (!XPeekEvent (dpy, &event))
	    throw std::runtime_error ("Failed to peek event");

	if (!matcher.MatchAndExplain (event, NULL))
	{
	    RemoveEventFromQueue (dpy);
	    continue;
	}

	return true;
    }

    std::stringstream ss;
    matcher.DescribeTo (&ss);
    ADD_FAILURE () << "Expected event matching: " << ss.str ();

    return false;
}

std::list <Window>
ct::NET_CLIENT_LIST_STACKING (Display *dpy)
{
    Atom property = XInternAtom (dpy, "_NET_CLIENT_LIST_STACKING", false);
    Atom actual_type;
    int actual_fmt;
    unsigned long nitems, nleft;
    unsigned char *prop;
    std::list <Window> stackingOrder;

    /* _NET_CLIENT_LIST_STACKING */
    if (XGetWindowProperty (dpy, DefaultRootWindow (dpy), property, 0L, 512L, false, XA_WINDOW,
			    &actual_type, &actual_fmt, &nitems, &nleft, &prop) == Success)
    {
	if (nitems && !nleft && actual_fmt == 32 && actual_type == XA_WINDOW)
	{
	    Window *window = reinterpret_cast <Window *> (prop);

	    while (nitems--)
		stackingOrder.push_back (*window++);

	}

	if (prop)
	    XFree (prop);
    }

    return stackingOrder;
}

namespace
{
class StartupClientMessageMatcher :
    public ct::XEventMatcher
{
    public:

	StartupClientMessageMatcher (Atom                             startup,
				     Window                           root,
				     ct::CompizProcess::StartupFlags state) :
	    mStartup (startup),
	    mRoot (root),
	    mFlags (state)
	{
	}

	virtual bool MatchAndExplain (const XEvent &event, MatchResultListener *listener) const
	{
	    int state = mFlags & ct::CompizProcess::ExpectStartupFailure ? 0 : 1;

	    if (event.xclient.window == mRoot &&
		event.xclient.message_type == mStartup &&
		event.xclient.data.l[0] == state)
		return true;

	    return false;
	}

	virtual void DescribeTo (std::ostream *os) const
	{
	    *os << "is startup message";
	}

	virtual void DescribeNegationTo (std::ostream *os) const
	{
	    *os << "is not startup message";
	}

    private:

	Atom mStartup;
	Window mRoot;
	ct::CompizProcess::StartupFlags mFlags;
};
}

class ct::PrivateClientMessageXEventMatcher
{
    public:

	PrivateClientMessageXEventMatcher (Display *display,
					   Atom    message,
					   Window  target) :
	    display (display),
	    message (message),
	    target (target)
	{
	}

	Display *display;
	Atom    message;
	Window  target;
};

ct::ClientMessageXEventMatcher::ClientMessageXEventMatcher (Display *display,
							    Atom    message,
							    Window  target) :
    priv (new ct::PrivateClientMessageXEventMatcher (display, message, target))
{
}

bool
ct::ClientMessageXEventMatcher::MatchAndExplain (const XEvent &event, MatchResultListener *listener) const
{
    const XClientMessageEvent *xce = reinterpret_cast <const XClientMessageEvent *> (&event);

    if (xce->message_type == priv->message &&
	xce->window == priv->target)
	return true;

    return false;
}

void
ct::ClientMessageXEventMatcher::DescribeTo (std::ostream *os) const
{
    CharacterWrapper name (XGetAtomName (priv->display,
					 priv->message));
    *os << "matches ClientMessage with type " << name
	<< " on window "
	<< std::hex << static_cast <long> (priv->target)
	<< std::dec << std::endl;
}

class ct::PrivatePropertyNotifyXEventMatcher
{
    public:

	PrivatePropertyNotifyXEventMatcher (Display           *dpy,
					    const std::string &propertyName) :
	    mPropertyName (propertyName),
	    mProperty (XInternAtom (dpy, propertyName.c_str (), false))
	{
	}

	std::string mPropertyName;
	Atom	mProperty;
};

ct::PropertyNotifyXEventMatcher::PropertyNotifyXEventMatcher (Display           *dpy,
							      const std::string &propertyName) :
    priv (new ct::PrivatePropertyNotifyXEventMatcher (dpy, propertyName))
{
}

bool
ct::PropertyNotifyXEventMatcher::MatchAndExplain (const XEvent &event, MatchResultListener *listener) const
{
    const XPropertyEvent *propertyEvent = reinterpret_cast <const XPropertyEvent *> (&event);

    if (priv->mProperty == propertyEvent->atom)
	return true;
    else
	return false;
}

void
ct::PropertyNotifyXEventMatcher::DescribeTo (std::ostream *os) const
{
    *os << "Is property identified by " << priv->mPropertyName;
}

class ct::PrivateConfigureNotifyXEventMatcher
{
    public:

	PrivateConfigureNotifyXEventMatcher (Window       above,
					     unsigned int border,
					     int          x,
					     int          y,
					     unsigned int width,
					     unsigned int height,
					     unsigned int mask) :
	    mAbove (above),
	    mBorder (border),
	    mX (x),
	    mY (y),
	    mWidth (width),
	    mHeight (height),
	    mMask (mask)
	{
	}

	Window       mAbove;
	int          mBorder;
	int          mX;
	int          mY;
	int          mWidth;
	int          mHeight;
	unsigned int mMask;
};

ct::ConfigureNotifyXEventMatcher::ConfigureNotifyXEventMatcher (Window       above,
								unsigned int border,
								int          x,
								int          y,
								unsigned int width,
								unsigned int height,
								unsigned int mask) :
    priv (new ct::PrivateConfigureNotifyXEventMatcher (above,
						       border,
						       x,
						       y,
						       width,
						       height,
						       mask))
{
}

bool
ct::ConfigureNotifyXEventMatcher::MatchAndExplain (const XEvent &event, MatchResultListener *listener) const
{
    const XConfigureEvent *ce = reinterpret_cast <const XConfigureEvent *> (&event);

    if (priv->mMask & CWSibling)
	if (ce->above != priv->mAbove)
	    return false;

    if (priv->mMask & CWBorderWidth)
	if (ce->border_width != priv->mBorder)
	    return false;

    if (priv->mMask & CWX)
	if (ce->x != priv->mX)
	    return false;

    if (priv->mMask & CWY)
	if (ce->y != priv->mY)
	    return false;

    if (priv->mMask & CWWidth)
	if (ce->width != priv->mWidth)
	    return false;

    if (priv->mMask & CWHeight)
	if (ce->height != priv->mHeight)
	    return false;

    return true;
}

void
ct::ConfigureNotifyXEventMatcher::DescribeTo (std::ostream *os) const
{
    std::stringstream x, y, width, height, border, sibling;

    if (priv->mMask & CWX)
	x << " x: " << priv->mX;

    if (priv->mMask & CWY)
	y << " y: " << priv->mY;

    if (priv->mMask & CWWidth)
	width << " width: " << priv->mWidth;

    if (priv->mMask & CWHeight)
	height << " height: " << priv->mHeight;

    if (priv->mMask & CWBorderWidth)
	border << " border: " << priv->mBorder;

    if (priv->mMask & CWSibling)
	sibling << " above: " << std::hex << priv->mAbove << std::dec;

    *os << "Matches ConfigureNotify with parameters : " << std::endl <<
	   x.str () << y.str () << width.str () << height.str () << border.str () << sibling.str ();
}

class ct::PrivateShapeNotifyXEventMatcher
{
    public:

	PrivateShapeNotifyXEventMatcher (int          kind,
					 int          x,
					 int          y,
					 unsigned int width,
					 unsigned int height,
					 Bool         shaped) :
	    mKind (kind),
	    mX (x),
	    mY (y),
	    mWidth (width),
	    mHeight (height),
	    mShaped (shaped)
	{
	}

	int          mKind;
	int          mX;
	int          mY;
	unsigned int mWidth;
	unsigned int mHeight;
	Bool         mShaped;
};

bool
ct::ShapeNotifyXEventMatcher::MatchAndExplain (const XEvent        &event,
					       MatchResultListener *listener) const
{
    const XShapeEvent *sev = reinterpret_cast <const XShapeEvent *> (&event);

    return sev->kind   == priv->mKind &&
	   sev->x      == priv->mX &&
	   sev->y      == priv->mY &&
	   sev->width  == priv->mWidth &&
	   sev->height == priv->mHeight &&
	   sev->shaped == priv->mShaped;
}

void
ct::ShapeNotifyXEventMatcher::DescribeTo (std::ostream *os) const
{
    std::string kindStr (priv->mKind == ShapeBounding ?
			     " ShapeBounding " :
			     " ShapeInput ");

    *os << " Matches ShapeNotify with parameters : " << std::endl <<
	   " kind : " << kindStr <<
	   " x : " << priv->mX <<
	   " y : " << priv->mY <<
	   " width: " << priv->mWidth <<
	   " height: " << priv->mHeight <<
	   " shaped: " << priv->mShaped;
}

ct::ShapeNotifyXEventMatcher::ShapeNotifyXEventMatcher (int          kind,
							int          x,
							int          y,
							unsigned int width,
							unsigned int height,
							Bool         shaped) :
    priv (new ct::PrivateShapeNotifyXEventMatcher (kind,
						   x,
						   y,
						   width,
						   height,
						   shaped))
{
}

void ct::RelativeWindowGeometry (Display      *dpy,
				 Window       w,
				 int          &x,
				 int          &y,
				 unsigned int &width,
				 unsigned int &height,
				 unsigned int &border)
{
    Window       root;
    unsigned int depth;

    if (!XGetGeometry (dpy, w, &root, &x, &y, &width, &height, &border, &depth))
	throw std::logic_error ("XGetGeometry failed");
}

void ct::AbsoluteWindowGeometry (Display      *display,
				 Window       window,
				 int          &x,
				 int          &y,
				 unsigned int &width,
				 unsigned int &height,
				 unsigned int &border)
{
    Window       root;
    Window       child;
    unsigned int depth;

    if (!XGetGeometry (display, window, &root,
		       &x, &y, &width, &height,
		       &border, &depth))
	throw std::logic_error ("XGetGeometry failed");

    if (!XTranslateCoordinates (display, window, root, x, y,
				&x, &y, &child))
	throw std::logic_error ("XTranslateCoordinates failed");
}

class ct::WindowGeometryMatcher::Private
{
    public:

	Private (Display                      *dpy,
		 ct::RetrievalFunc            func,
		 const Matcher <int>          &x,
		 const Matcher <int>          &y,
		 const Matcher <unsigned int> &width,
		 const Matcher <unsigned int> &height,
		 const Matcher <unsigned int> &border);

	Display       *mDpy;

	RetrievalFunc mFunc;

	Matcher <int> mX;
	Matcher <int> mY;
	Matcher <unsigned int> mWidth;
	Matcher <unsigned int> mHeight;
	Matcher <unsigned int> mBorder;
};

Matcher <Window>
ct::HasGeometry (Display             *dpy,
		 RetrievalFunc       func,
		 const Matcher <int> &x,
		 const Matcher <int> &y,
		 const Matcher <unsigned int> &width,
		 const Matcher <unsigned int> &height,
		 const Matcher <unsigned int> &border)
{
    return MakeMatcher (new WindowGeometryMatcher (dpy,
						   func,
						   x,
						   y,
						   width,
						   height,
						   border));
}

ct::WindowGeometryMatcher::WindowGeometryMatcher (Display             *dpy,
						  RetrievalFunc       func,
						  const Matcher <int> &x,
						  const Matcher <int> &y,
						  const Matcher <unsigned int> &width,
						  const Matcher <unsigned int> &height,
						  const Matcher <unsigned int> &border) :
    priv (new Private (dpy, func, x, y, width, height, border))
{
}

ct::WindowGeometryMatcher::Private::Private (Display             *dpy,
					     RetrievalFunc       func,
					     const Matcher <int> &x,
					     const Matcher <int> &y,
					     const Matcher <unsigned int> &width,
					     const Matcher <unsigned int> &height,
					     const Matcher <unsigned int> &border):
    mDpy (dpy),
    mFunc (func),
    mX (x),
    mY (y),
    mWidth (width),
    mHeight (height),
    mBorder (border)
{
}

bool
ct::WindowGeometryMatcher::MatchAndExplain (Window w,
					    MatchResultListener *listener) const
{
    int          x, y;
    unsigned int width, height, border;

    priv->mFunc (priv->mDpy, w, x, y, width, height, border);

    bool match = priv->mX.MatchAndExplain (x, listener) &&
		 priv->mY.MatchAndExplain (y, listener) &&
		 priv->mWidth.MatchAndExplain (width, listener) &&
		 priv->mHeight.MatchAndExplain (height, listener) &&
		 priv->mBorder.MatchAndExplain (border, listener);

    if (!match)
    {
	*listener << "Geometry:"
		  << " x: " << x
		  << " y: " << y
		  << " width: " << width
		  << " height: " << height
		  << " border: " << border;
    }

    return match;
}

void
ct::WindowGeometryMatcher::DescribeTo (std::ostream *os) const
{
    *os << "Window geometry matching :";

    *os << std::endl << " - ";
    priv->mX.DescribeTo (os);

    *os << std::endl << " - ";
    priv->mY.DescribeTo (os);

    *os << std::endl << " - ";
    priv->mWidth.DescribeTo (os);

    *os << std::endl << " - ";
    priv->mHeight.DescribeTo (os);

    *os << std::endl << " - ";
    priv->mBorder.DescribeTo (os);
}

class ct::PrivateCompizProcess
{
    public:
	PrivateCompizProcess (ct::CompizProcess::StartupFlags flags) :
	    mFlags (flags),
	    mIsRunning (true)
	{
	}

	void WaitForStartupMessage (Display                         *dpy,
				    ct::CompizProcess::StartupFlags flags,
				    unsigned int                    waitTimeout);

	typedef boost::shared_ptr <xorg::testing::Process> ProcessPtr;

	ct::CompizProcess::StartupFlags mFlags;
	bool                            mIsRunning;
	xorg::testing::Process          mProcess;

	std::string                     mPluginPaths;
	std::auto_ptr <TmpEnv>          mPluginPathsEnv;
};

void
ct::PrivateCompizProcess::WaitForStartupMessage (Display                         *dpy,
						 ct::CompizProcess::StartupFlags flags,
						 unsigned int                    waitTimeout)
{
    XWindowAttributes attrib;
    Window    root = DefaultRootWindow (dpy);

    Atom    startup = XInternAtom (dpy,
				   "_COMPIZ_TESTING_STARTUP",
				   false);

    StartupClientMessageMatcher matcher (startup, root, flags);

    /* Save the current event mask and subscribe to StructureNotifyMask only */
    ASSERT_TRUE (XGetWindowAttributes (dpy, root, &attrib));
    XSelectInput (dpy, root, StructureNotifyMask |
			     attrib.your_event_mask);

    ASSERT_TRUE (ct::AdvanceToNextEventOnSuccess (
		     dpy,
		     ct::WaitForEventOfTypeOnWindowMatching (dpy,
							     root,
							     ClientMessage,
							     -1,
							     -1,
							     matcher,
							     waitTimeout)));

    XSelectInput (dpy, root, attrib.your_event_mask);
}

namespace
{
std::string PathForPlugin (const std::string             &name,
			   ct::CompizProcess::PluginType type)
{
    switch (type)
    {
	case ct::CompizProcess::Real:
	    return compizRealPluginPath + name;
	case ct::CompizProcess::TestOnly:
	    return compizTestOnlyPluginPath + name;
	default:
	    throw std::logic_error ("Incorrect value for type");
    }

    return "";
}

}

ct::CompizProcess::CompizProcess (::Display                           *dpy,
				  ct::CompizProcess::StartupFlags     flags,
				  const ct::CompizProcess::PluginList &plugins,
				  int                                 waitTimeout) :
    priv (new PrivateCompizProcess (flags))
{
    xorg::testing::Process::SetEnv ("LD_LIBRARY_PATH", compizLDLibraryPath, true);

    std::vector <std::string> args;

    if (flags & ct::CompizProcess::ReplaceCurrentWM)
	args.push_back ("--replace");

    args.push_back ("--send-startup-message");

    /* Copy in plugin list and set environment variables */
    for (ct::CompizProcess::PluginList::const_iterator it = plugins.begin ();
	 it != plugins.end ();
	 ++it)
    {
	priv->mPluginPaths += PathForPlugin (it->name, it->type) + ":";
	args.push_back (it->name);
    }

    priv->mPluginPathsEnv.reset (new TmpEnv ("COMPIZ_PLUGIN_DIR",
					     priv->mPluginPaths.c_str ()));

    priv->mProcess.Start (compizBinaryPath, args);
    EXPECT_EQ (priv->mProcess.GetState (), xorg::testing::Process::RUNNING);

    if (flags & ct::CompizProcess::WaitForStartupMessage)
	priv->WaitForStartupMessage (dpy, flags, waitTimeout);
}

ct::CompizProcess::~CompizProcess ()
{
    if (priv->mProcess.GetState () == xorg::testing::Process::RUNNING)
	priv->mProcess.Kill ();
}

xorg::testing::Process::State
ct::CompizProcess::State ()
{
    return priv->mProcess.GetState ();
}

pid_t
ct::CompizProcess::Pid ()
{
    return priv->mProcess.Pid ();
}

class ct::PrivateCompizXorgSystemTest
{
    public:

	boost::shared_ptr <ct::CompizProcess> mProcess;
};

ct::CompizXorgSystemTest::CompizXorgSystemTest () :
    priv (new PrivateCompizXorgSystemTest)
{
}

void
ct::CompizXorgSystemTest::SetUp ()
{
    const unsigned int MAX_CONNECTION_ATTEMPTS = 10;
    const unsigned int USEC_TO_MSEC = 1000;
    const unsigned int SLEEP_TIME = 50 * USEC_TO_MSEC;

    int connectionAttemptsRemaining = MAX_CONNECTION_ATTEMPTS;

    /* Work around an inherent race condition in XOpenDisplay
     *
     * All xorg::testing::Test::SetUp does is call XOpenDisplay
     * and assign a display string, the former before the latter.
     * The current X Error handler will throw an exception if
     * an X error occurrs.
     *
     * Unfortunately there's an inherent race condition in spawning
     * a new server and using XOpenDisplay to connect to it - we
     * simply don't know when the new server will be ready, and even
     * watching its socket with inotify will be racey too. The only
     * solution would be a handshake process where we pass the server
     * a socket or pipe and it writes to it indicating that it is ready
     * to accept connections. There isn't such a thing. As such, we need
     * to work around that by simply re-trying our connection to the server
     * once every 50ms or so, and we're trying about 10 times before giving up
     * and assuming there is a problem with the server.
     *
     * The predecrement here is so that connectionAttemptsRemaining will be 0
     * on failure
     */
    while (--connectionAttemptsRemaining)
    {
	try
	{
	    xorg::testing::Test::SetUp ();
	    break;
	}
	catch (std::runtime_error &exception)
	{
	    usleep (SLEEP_TIME);
	}
    }

    if (!connectionAttemptsRemaining)
    {
	throw std::runtime_error ("Failed to connect to X Server. "\
                                  "Check the logs by setting "\
                                  "XORG_GTEST_CHILD_STDOUT=1 to see if "\
                                  "there are any startup errors. Otherwise "\
                                  "if you suspect the server is running "\
                                  "particularly slowly, try bumping up the "\
                                  "maximum number of connection attempts in "\
                                  "compiz-xorg-gtest.cpp");
    }
}

void
ct::CompizXorgSystemTest::TearDown ()
{
    priv->mProcess.reset ();

    xorg::testing::Test::TearDown ();
}

xorg::testing::Process::State
ct::CompizXorgSystemTest::CompizProcessState ()
{
    if (priv->mProcess)
	return priv->mProcess->State ();
    return xorg::testing::Process::NONE;
}

void
ct::CompizXorgSystemTest::StartCompiz (ct::CompizProcess::StartupFlags     flags,
				       const ct::CompizProcess::PluginList &plugins)
{
    priv->mProcess.reset (new ct::CompizProcess (Display (), flags, plugins));
}

class ct::PrivateAutostartCompizXorgSystemTest
{
};

ct::AutostartCompizXorgSystemTest::AutostartCompizXorgSystemTest () :
    priv (new ct::PrivateAutostartCompizXorgSystemTest ())
{
}

ct::CompizProcess::StartupFlags
ct::AutostartCompizXorgSystemTest::GetStartupFlags ()
{
    return static_cast <ct::CompizProcess::StartupFlags> (
		ct::CompizProcess::ReplaceCurrentWM |
		ct::CompizProcess::WaitForStartupMessage);
}

int
ct::AutostartCompizXorgSystemTest::GetEventMask () const
{
    return 0;
}

ct::CompizProcess::PluginList
ct::AutostartCompizXorgSystemTest::GetPluginList ()
{
    return ct::CompizProcess::PluginList ();
}

void
ct::AutostartCompizXorgSystemTest::SetUp ()
{
    ct::CompizXorgSystemTest::SetUp ();

    ::Display *display = Display ();
    XSelectInput (display, DefaultRootWindow (display),
		  GetEventMask ());

    StartCompiz (GetStartupFlags (),
		 GetPluginList ());
}

class ct::PrivateAutostartCompizXorgSystemTestWithTestHelper
{
    public:

	std::auto_ptr <ct::MessageAtoms> mMessages;
};

std::vector <long>
ct::AutostartCompizXorgSystemTestWithTestHelper::WaitForWindowCreation (Window w)
{
    ::Display *dpy = Display ();

    XEvent event;

    bool requestAcknowledged = false;
    while (ct::ReceiveMessage (dpy,
			       FetchAtom (ct::messages::TEST_HELPER_WINDOW_READY),
			       event))
    {
	requestAcknowledged =
	    w == static_cast <unsigned long> (event.xclient.data.l[0]);

	if (requestAcknowledged)
	    break;

    }

    EXPECT_TRUE (requestAcknowledged);

    std::vector <long> data;
    for (int i = 0; i < 5; ++i)
	data.push_back(static_cast <long> (event.xclient.data.l[i]));

    return data;
}

bool
ct::AutostartCompizXorgSystemTestWithTestHelper::IsOverrideRedirect (std::vector <long> &data)
{
    return (data[1] > 0) ? true : false;
}

Atom
ct::AutostartCompizXorgSystemTestWithTestHelper::FetchAtom (const char *message)
{
    return priv->mMessages->FetchForString (message);
}

ct::AutostartCompizXorgSystemTestWithTestHelper::AutostartCompizXorgSystemTestWithTestHelper () :
    priv (new ct::PrivateAutostartCompizXorgSystemTestWithTestHelper)
{
}

int
ct::AutostartCompizXorgSystemTestWithTestHelper::GetEventMask () const
{
    return AutostartCompizXorgSystemTest::GetEventMask () |
	   StructureNotifyMask;
}

void
ct::AutostartCompizXorgSystemTestWithTestHelper::SetUp ()
{
    ct::AutostartCompizXorgSystemTest::SetUp ();
    priv->mMessages.reset (new ct::MessageAtoms (Display ()));

    ::Display *dpy = Display ();
    Window root = DefaultRootWindow (dpy);

    Atom    ready = priv->mMessages->FetchForString (ct::messages::TEST_HELPER_READY_MSG);
    ct::ClientMessageXEventMatcher matcher (dpy, ready, root);

    ASSERT_TRUE (ct::AdvanceToNextEventOnSuccess (
		     dpy,
		     ct::WaitForEventOfTypeOnWindowMatching (dpy,
							     root,
							     ClientMessage,
							     -1,
							     -1,
							     matcher)));
}

ct::CompizProcess::PluginList
ct::AutostartCompizXorgSystemTestWithTestHelper::GetPluginList ()
{
    ct::CompizProcess::PluginList list;
    list.push_back (ct::CompizProcess::Plugin ("testhelper",
					       ct::CompizProcess::TestOnly));
    return list;
}
