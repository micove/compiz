/*
* Copyright © 2013 Sam Spilsbury
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
* Author: Sam Spilsbury <smspillaz@gmail.com>
*/

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <poll.h>
#include <X11/Xlib.h>
#include <compiz_xorg_gtest_communicator.h>

namespace compiz
{
namespace testing
{
namespace messages
{
namespace internal
{
const char *messages[] =
{
    "_COMPIZ_TEST_HELPER_READY",
    "_COMPIZ_TEST_HELPER_REGISTER_CLIENT",
    "_COMPIZ_TEST_HELPER_LOCK_CONFIGURE_REQUESTS",
    "_COMPIZ_TEST_HELPER_CHANGE_FRAME_EXTENTS",
    "_COMPIZ_TEST_HELPER_FRAME_EXTENTS_CHANGED",
    "_COMPIZ_TEST_HELPER_CONFIGURE_WINDOW",
    "_COMPIZ_TEST_HELPER_WINDOW_CONFIGURE_PROCESSED",
    "_COMPIZ_TEST_HELPER_DESTROY_ON_REPARENT",
    "_COMPIZ_TEST_HELPER_RESTACK_ATLEAST_ABOVE",
    "_COMPIZ_TEST_HELPER_WINDOW_READY"
};
}

const char *TEST_HELPER_READY_MSG = internal::messages[0];
const char *TEST_HELPER_REGISTER_CLIENT = internal::messages[1];
const char *TEST_HELPER_LOCK_CONFIGURE_REQUESTS = internal::messages[2];
const char *TEST_HELPER_CHANGE_FRAME_EXTENTS = internal::messages[3];
const char *TEST_HELPER_FRAME_EXTENTS_CHANGED = internal::messages[4];
const char *TEST_HELPER_CONFIGURE_WINDOW = internal::messages[5];
const char *TEST_HELPER_WINDOW_CONFIGURE_PROCESSED = internal::messages[6];
const char *TEST_HELPER_DESTROY_ON_REPARENT = internal::messages[7];
const char *TEST_HELPER_RESTACK_ATLEAST_ABOVE = internal::messages[8];
const char *TEST_HELPER_WINDOW_READY = internal::messages[9];
}
}
}

namespace ct = compiz::testing;
namespace ctmi = compiz::testing::messages::internal;

class ct::MessageAtoms::Private
{
    public:

	std::map <const char *, Atom> atoms;
};

ct::MessageAtoms::MessageAtoms (Display *display) :
    priv (new ct::MessageAtoms::Private)
{
    int  nAtoms = sizeof (ctmi::messages) / sizeof (const char *);
    Atom atoms[nAtoms];

    if (!XInternAtoms (display,
		       const_cast <char **> (ctmi::messages),
		       sizeof (ctmi::messages) / sizeof (const char *),
		       0,
		       atoms))
	throw std::runtime_error ("XInternAtoms generated an error");

    for (int i = 0; i < nAtoms; ++i)
	priv->atoms[ctmi::messages[i]] = atoms[i];
}

Atom
ct::MessageAtoms::FetchForString (const char *message)
{
    std::map <const char *, Atom>::iterator it (priv->atoms.find (message));

    if (it == priv->atoms.end ())
    {
	std::stringstream ss;

	ss << "Atom for message " << message << " does not exist";
	throw std::runtime_error (ss.str ());
    }

    return it->second;
}

namespace
{
bool FindClientMessage (Display *display,
			XEvent  &event,
			Atom    message)
{
    while (XPending (display))
    {
	XNextEvent (display, &event);

	if (event.type == ClientMessage)
	{
	    if (event.xclient.message_type == message)
		return true;
	}
    }

    return false;
}
}

bool
ct::ReceiveMessage (Display *display,
		    Atom    message,
		    XEvent  &event,
		    int     timeout)
{
    /* Ensure the event queue is fully flushed */
    XFlush (display);

    if (FindClientMessage (display, event, message))
	return true;
    else
    {
	struct pollfd pfd;
	pfd.events = POLLIN | POLLHUP | POLLERR;
	pfd.revents = 0;
	pfd.fd = ConnectionNumber (display);

	poll (&pfd, 1, timeout);

	/* Make sure we get something */
	if ((pfd.revents & POLLIN) &&
	    !(pfd.revents & (POLLHUP | POLLERR)))
	{
	    return ReceiveMessage (display, message, event, timeout);
	}
	else
	    return false;
    }

    return false;
}

void
ct::SendClientMessage (Display *display,
		       Atom    message,
		       Window  destination,
		       Window  target,
		       const std::vector<long> &data)
{
    if (data.size () > 5)
	throw std::runtime_error ("data size must be less than 5");

    XEvent event;

    memset (&event, 0, sizeof (XEvent));

    event.type = ClientMessage;

    event.xclient.display = display;
    event.xclient.send_event = 1;
    event.xclient.serial = 0;

    event.xclient.message_type = message;
    event.xclient.window = target;
    event.xclient.format = 32;

    int count = 0;
    for (std::vector <long>::const_iterator it = data.begin ();
	 it != data.end ();
	 ++it)
	event.xclient.data.l[count++] = *it;

    XSendEvent (display,
		destination,
		0,
		StructureNotifyMask,
		&event);
    XFlush (display);
};
