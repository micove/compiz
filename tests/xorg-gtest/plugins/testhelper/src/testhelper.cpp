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

#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include "testhelper.h"

COMPIZ_PLUGIN_20090315 (testhelper, TestHelperPluginVTable)

namespace
{
template <typename T>
void XFreeT (T *t)
{
    XFree (t);
}
}

namespace ct = compiz::testing;
namespace ctm = compiz::testing::messages;

void
TestHelperScreen::handleEvent (XEvent *event)
{
    if (event->type == ClientMessage)
    {
	if (event->xclient.window != screen->root ())
	{
	    std::map <Atom, ClientMessageHandler>::iterator it =
		mMessageHandlers.find (event->xclient.message_type);

	    if (it != mMessageHandlers.end ())
	    {
		ClientMessageHandler handler (it->second);
		CompWindow *w = screen->findWindow (event->xclient.window);

		XClientMessageEvent *xce = &event->xclient;
		long                *data = xce->data.l;

		if (w)
		    ((*TestHelperWindow::get (w)).*(handler)) (data);
	    }
	}
    }

    screen->handleEvent (event);
}

void
TestHelperScreen::watchForMessage (Atom message, ClientMessageHandler handler)
{
    if (mMessageHandlers.find (message) != mMessageHandlers.end ())
    {
	boost::shared_ptr <char> name (XGetAtomName (screen->dpy (), message),
				       boost::bind (XFreeT <char>, _1));
	compLogMessage ("testhelper", CompLogLevelWarn,
			"a message handler was already defined for %s",
			name.get ());
	return;
    }

    mMessageHandlers[message] = handler;
}

void
TestHelperScreen::removeMessageWatch (Atom message)
{
    std::map <Atom, ClientMessageHandler>::iterator it =
	mMessageHandlers.find (message);

    if (it != mMessageHandlers.end ())
	mMessageHandlers.erase (it);
}

Atom
TestHelperScreen::fetchAtom (const char *message)
{
    return mAtomStore.FetchForString (message);
}

void
TestHelperWindow::configureAndReport (long *data)
{
    XWindowChanges xwc;
    XWindowChanges saved;

    xwc.x = data[0];
    xwc.y = data[1];
    xwc.width = data[2];
    xwc.height = data[3];
    int mask = data[4];

    /* configureXWindow has a nasty side-effect of
     * changing xwc to the client-window co-ordinates,
     * so we should back it up first */
    saved = xwc;

    window->configureXWindow (mask, &xwc);

    xwc = saved;

    std::vector <long> response;

    response.push_back (xwc.x);
    response.push_back (xwc.y);
    response.push_back (xwc.width);
    response.push_back (xwc.height);

    TestHelperScreen *ts = TestHelperScreen::get (screen);
    const Atom       atom = ts->fetchAtom (ctm::TEST_HELPER_WINDOW_CONFIGURE_PROCESSED);

    ct::SendClientMessage (screen->dpy (),
			   atom,
			   screen->root (),
			   window->id (),
			   response);
}

void
TestHelperWindow::setFrameExtentsAndReport (long *data)
{
    /* Only change the frame input and not the border */
    CompWindowExtents input (data[0], data[1], data[2], data[3]);
    CompWindowExtents border (0, 0, 0, 0);

    window->setWindowFrameExtents (&border, &input);

    std::vector <long> response;

    response.push_back (input.left);
    response.push_back (input.right);
    response.push_back (input.top);
    response.push_back (input.bottom);

    TestHelperScreen *ts = TestHelperScreen::get (screen);
    const Atom       atom = ts->fetchAtom (ctm::TEST_HELPER_FRAME_EXTENTS_CHANGED);

    ct::SendClientMessage (screen->dpy (),
			   atom,
			   screen->root (),
			   window->id (),
			   response);
}

void
TestHelperWindow::setConfigureLock (long *data)
{
    bool enabled = data[0] ? true : false;

    if (enabled && !configureLock)
	configureLock = window->obtainLockOnConfigureRequests ();
    else if (!enabled && configureLock)
    {
	configureLock->release ();
	configureLock.reset ();
    }
}

void
TestHelperWindow::setDestroyOnReparent (long *)
{
    destroyOnReparent = true;
}

void
TestHelperWindow::restackAtLeastAbove (long *data)
{
    ServerLock lock (screen->serverGrabInterface ());

    Window            above = data[0];
    XWindowAttributes attrib;

    if (!XGetWindowAttributes (screen->dpy (), above, &attrib))
	return;

    CompWindow *w = screen->findTopLevelWindow (above, true);
    for (; w; w = w->next)
	if (!w->overrideRedirect ())
	    break;

    if (!w)
	return;

    XWindowChanges xwc;

    xwc.stack_mode = Above;
    xwc.sibling = w->frame () ? w->frame () : w->id ();

    window->restackAndConfigureXWindow (CWStackMode | CWSibling,
					&xwc,
					lock);
}

void
TestHelperWindow::windowNotify (CompWindowNotify n)
{
    switch (n)
    {
	case CompWindowNotifyReparent:
	    if (destroyOnReparent)
	    {
		Window id = window->id ();
		window->destroy ();
		XDestroyWindow (screen->dpy (), id);
	    }
	    break;
	default:
	    break;
    }

    window->windowNotify (n);
}

TestHelperWindow::TestHelperWindow (CompWindow *w) :
    PluginClassHandler <TestHelperWindow, CompWindow> (w),
    window (w),
    configureLock (),
    destroyOnReparent (false)
{
    WindowInterface::setHandler (w);

    TestHelperScreen *ts = TestHelperScreen::get (screen);

    std::vector <long> data;
    data.push_back (static_cast <long> (window->id ()));
    data.push_back (static_cast <long> (window->overrideRedirect ()));
    ct::SendClientMessage (screen->dpy (),
			   ts->fetchAtom (ctm::TEST_HELPER_WINDOW_READY),
			   screen->root (),
			   screen->root (),
			   data);
}

TestHelperScreen::TestHelperScreen (CompScreen *s) :
    PluginClassHandler <TestHelperScreen, CompScreen> (s),
    screen (s),
    mAtomStore (s->dpy ())
{
    ScreenInterface::setHandler (s);

    /* Register the message handlers on each window */
    watchForMessage (fetchAtom (ctm::TEST_HELPER_CONFIGURE_WINDOW),
		     &TestHelperWindow::configureAndReport);
    watchForMessage (fetchAtom (ctm::TEST_HELPER_CHANGE_FRAME_EXTENTS),
		     &TestHelperWindow::setFrameExtentsAndReport);
    watchForMessage (fetchAtom (ctm::TEST_HELPER_LOCK_CONFIGURE_REQUESTS),
		     &TestHelperWindow::setConfigureLock);
    watchForMessage (fetchAtom (ctm::TEST_HELPER_DESTROY_ON_REPARENT),
		     &TestHelperWindow::setDestroyOnReparent);
    watchForMessage (fetchAtom (ctm::TEST_HELPER_RESTACK_ATLEAST_ABOVE),
		     &TestHelperWindow::restackAtLeastAbove);

    ct::SendClientMessage (s->dpy (),
			   mAtomStore.FetchForString (ctm::TEST_HELPER_READY_MSG),
			   s->root (),
			   s->root (),
			   std::vector <long> ());
}

bool
TestHelperPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
	return true;

    return false;
}
