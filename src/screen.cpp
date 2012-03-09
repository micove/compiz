/*
 * Copyright © 2005 Novell, Inc.
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
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECI<<<<<fAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <poll.h>
#include <algorithm>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>

#include <core/core.h>

#include <core/screen.h>
#include <core/icon.h>
#include <core/atoms.h>
#include "privatescreen.h"
#include "privatewindow.h"
#include "privateaction.h"

bool inHandleEvent = false;

bool screenInitalized = false;

CompScreen *targetScreen = NULL;
CompOutput *targetOutput;

int lastPointerX = 0;
int lastPointerY = 0;
unsigned int lastPointerMods = 0;
int pointerX     = 0;
int pointerY     = 0;
unsigned int pointerMods = 0;

#define MwmHintsFunctions   (1L << 0)
#define MwmHintsDecorations (1L << 1)
#define PropMotifWmHintElements 3

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
} MwmHints;



CompScreen *screen;
ModifierHandler *modHandler;

PluginClassStorage::Indices screenPluginClassIndices (0);

unsigned int
CompScreen::allocPluginClassIndex ()
{
    unsigned int i = PluginClassStorage::allocatePluginClassIndex (screenPluginClassIndices);

    if (screenPluginClassIndices.size () != screen->pluginClasses.size ())
	screen->pluginClasses.resize (screenPluginClassIndices.size ());

    return i;
}

void
CompScreen::freePluginClassIndex (unsigned int index)
{
    PluginClassStorage::freePluginClassIndex (screenPluginClassIndices, index);

    if (screenPluginClassIndices.size () != screen->pluginClasses.size ())
	screen->pluginClasses.resize (screenPluginClassIndices.size ());
}

void
CompScreen::eventLoop ()
{
    priv->ctx = Glib::MainContext::get_default ();
    priv->mainloop = Glib::MainLoop::create (priv->ctx, false);
    priv->source = CompEventSource::create ();
    priv->timeout = CompTimeoutSource::create ();

    priv->source->attach (priv->ctx);

    /* Kick the event loop */
    priv->ctx->iteration (false);

    priv->mainloop->run ();
}

CompFileWatchHandle
CompScreen::addFileWatch (const char        *path,
			  int               mask,
			  FileWatchCallBack callBack)
{
    CompFileWatch *fileWatch = new CompFileWatch ();
    if (!fileWatch)
	return 0;

    fileWatch->path	= path;
    fileWatch->mask	= mask;
    fileWatch->callBack = callBack;
    fileWatch->handle   = priv->lastFileWatchHandle++;

    if (priv->lastFileWatchHandle == MAXSHORT)
	priv->lastFileWatchHandle = 1;

    priv->fileWatch.push_front (fileWatch);

    fileWatchAdded (fileWatch);

    return fileWatch->handle;
}

void
CompScreen::removeFileWatch (CompFileWatchHandle handle)
{
    std::list<CompFileWatch *>::iterator it;
    CompFileWatch                        *w;

    for (it = priv->fileWatch.begin (); it != priv->fileWatch.end (); it++)
	if ((*it)->handle == handle)
	    break;

    if (it == priv->fileWatch.end ())
	return;

    w = (*it);
    priv->fileWatch.erase (it);

    fileWatchRemoved (w);

    delete w;
}

const CompFileWatchList &
CompScreen::getFileWatches () const
{
    return priv->fileWatch;
}

void
PrivateScreen::addTimer (CompTimer *timer)
{
    std::list<CompTimer *>::iterator it;

    it = std::find (timers.begin (), timers.end (), timer);

    if (it != timers.end ())
	return;

    for (it = timers.begin (); it != timers.end (); it++)
    {
	if ((int) timer->mMinTime < (*it)->mMinLeft)
	    break;
    }

    timer->mMinLeft = timer->mMinTime;
    timer->mMaxLeft = timer->mMaxTime;

    timers.insert (it, timer);
}

void
PrivateScreen::removeTimer (CompTimer *timer)
{
    std::list<CompTimer *>::iterator it;

    it = std::find (timers.begin (), timers.end (), timer);

    if (it == timers.end ())
	return;

    timers.erase (it);
}

CompWatchFd::CompWatchFd (int		    fd,
			  Glib::IOCondition events,
			  FdWatchCallBack   callback) :
    Glib::IOSource (fd, events),
    mFd (fd),
    mCallBack (callback),
    mForceFail (false),
    mExecuting (false)
{
    connect (sigc::mem_fun <Glib::IOCondition, bool>
	     (this, &CompWatchFd::internalCallback));
}

Glib::RefPtr <CompWatchFd>
CompWatchFd::create (int               fd,
		     Glib::IOCondition events,
		     FdWatchCallBack   callback)
{
    return Glib::RefPtr <CompWatchFd> (new CompWatchFd (fd, events, callback));
}

CompWatchFdHandle
CompScreen::addWatchFd (int             fd,
			short int       events,
			FdWatchCallBack callBack)
{
    Glib::IOCondition gEvents;
    
    memset (&gEvents, 0, sizeof (Glib::IOCondition));

    if (events & POLLIN)
	gEvents |= Glib::IO_IN;
    if (events & POLLOUT)
	gEvents |= Glib::IO_OUT;
    if (events & POLLPRI)
	gEvents |= Glib::IO_PRI;
    if (events & POLLERR)
	gEvents |= Glib::IO_ERR;
    if (events & POLLHUP)
	gEvents |= Glib::IO_HUP;

    Glib::RefPtr <CompWatchFd> watchFd = CompWatchFd::create (fd, gEvents, callBack);

    watchFd->attach (priv->ctx);

    if (!watchFd)
	return 0;
    watchFd->mHandle   = priv->lastWatchFdHandle++;

    if (priv->lastWatchFdHandle == MAXSHORT)
	priv->lastWatchFdHandle = 1;

    priv->watchFds.push_front (watchFd);

    return watchFd->mHandle;
}

void
CompScreen::removeWatchFd (CompWatchFdHandle handle)
{
    std::list<Glib::RefPtr <CompWatchFd> >::iterator it;
    Glib::RefPtr <CompWatchFd>	       w;

    for (it = priv->watchFds.begin();
	 it != priv->watchFds.end (); it++)
    {
	if ((*it)->mHandle == handle)
	    break;
    }

    if (it == priv->watchFds.end ())
	return;

    w = (*it);

    if (w->mExecuting)
    {
	w->mForceFail = true;
	return;
    }

    w.reset ();
    priv->watchFds.erase (it);
}

void
CompScreen::storeValue (CompString key, CompPrivate value)
{
    std::map<CompString,CompPrivate>::iterator it;

    it = priv->valueMap.find (key);

    if (it != priv->valueMap.end ())
    {
	it->second = value;
    }
    else
    {
	priv->valueMap.insert (std::pair<CompString,CompPrivate> (key, value));
    }
}

bool
CompScreen::hasValue (CompString key)
{
    return (priv->valueMap.find (key) != priv->valueMap.end ());
}

CompPrivate
CompScreen::getValue (CompString key)
{
    CompPrivate p;

    std::map<CompString,CompPrivate>::iterator it;
    it = priv->valueMap.find (key);

    if (it != priv->valueMap.end ())
    {
	return it->second;
    }
    else
    {
	p.uval = 0;
	return p;
    }
}

bool
CompWatchFd::internalCallback (Glib::IOCondition events)
{
    short int revents = 0;

    if (events & Glib::IO_IN)
	revents |= POLLIN;
    if (events & Glib::IO_OUT)
	revents |= POLLOUT;
    if (events & Glib::IO_PRI)
	revents |= POLLPRI;
    if (events & Glib::IO_ERR)
	revents |= POLLERR;
    if (events & Glib::IO_HUP)
	revents |= POLLHUP;
    if (events & Glib::IO_NVAL)
	return false;

    mExecuting = true;
    mCallBack (revents);
    mExecuting = false;

    if (mForceFail)
    {
	/* FIXME: Need to find a way to properly remove the watchFd
	 * from the internal list in core */
	//screen->priv->watchFds.remove (this);
	return false;
    }
    
    return true;
}    

void
CompScreen::eraseValue (CompString key)
{
    std::map<CompString,CompPrivate>::iterator it;
    it = priv->valueMap.find (key);

    if (it != priv->valueMap.end ())
    {
	priv->valueMap.erase (key);
    }
}

void
CompScreen::fileWatchAdded (CompFileWatch *watch)
    WRAPABLE_HND_FUNC (0, fileWatchAdded, watch)

void
CompScreen::fileWatchRemoved (CompFileWatch *watch)
    WRAPABLE_HND_FUNC (1, fileWatchRemoved, watch)

bool
CompScreen::setOptionForPlugin (const char        *plugin,
				const char        *name,
				CompOption::Value &value)
{
    WRAPABLE_HND_FUNC_RETURN (4, bool, setOptionForPlugin,
			      plugin, name, value)

    CompPlugin *p = CompPlugin::find (plugin);
    if (p)
	return p->vTable->setOption (name, value);

    return false;
}

void
CompScreen::sessionEvent (CompSession::Event event,
			  CompOption::Vector &arguments)
    WRAPABLE_HND_FUNC (5, sessionEvent, event, arguments)

void
ScreenInterface::fileWatchAdded (CompFileWatch *watch)
    WRAPABLE_DEF (fileWatchAdded, watch)

void
ScreenInterface::fileWatchRemoved (CompFileWatch *watch)
    WRAPABLE_DEF (fileWatchRemoved, watch)

bool
ScreenInterface::initPluginForScreen (CompPlugin *plugin)
    WRAPABLE_DEF (initPluginForScreen, plugin)

void
ScreenInterface::finiPluginForScreen (CompPlugin *plugin)
    WRAPABLE_DEF (finiPluginForScreen, plugin)

bool
ScreenInterface::setOptionForPlugin (const char        *plugin,
				     const char	       *name,
				     CompOption::Value &value)
    WRAPABLE_DEF (setOptionForPlugin, plugin, name, value)

void
ScreenInterface::sessionEvent (CompSession::Event event,
			       CompOption::Vector &arguments)
    WRAPABLE_DEF (sessionEvent, event, arguments)


static int errors = 0;

static int
errorHandler (Display     *dpy,
	      XErrorEvent *e)
{

#ifdef DEBUG
    char str[128];
#endif

    errors++;

#ifdef DEBUG
    XGetErrorDatabaseText (dpy, "XlibMessage", "XError", "", str, 128);
    fprintf (stderr, "%s", str);

    XGetErrorText (dpy, e->error_code, str, 128);
    fprintf (stderr, ": %s\n  ", str);

    XGetErrorDatabaseText (dpy, "XlibMessage", "MajorCode", "%d", str, 128);
    fprintf (stderr, str, e->request_code);

    sprintf (str, "%d", e->request_code);
    XGetErrorDatabaseText (dpy, "XRequest", str, "", str, 128);
    if (strcmp (str, ""))
	fprintf (stderr, " (%s)", str);
    fprintf (stderr, "\n  ");

    XGetErrorDatabaseText (dpy, "XlibMessage", "MinorCode", "%d", str, 128);
    fprintf (stderr, str, e->minor_code);
    fprintf (stderr, "\n  ");

    XGetErrorDatabaseText (dpy, "XlibMessage", "ResourceID", "%d", str, 128);
    fprintf (stderr, str, e->resourceid);
    fprintf (stderr, "\n");

    /* abort (); */
#endif

    return 0;
}

int
CompScreen::checkForError (Display *dpy)
{
    int e;

    XSync (dpy, false);

    e = errors;
    errors = 0;

    return e;
}

Display *
CompScreen::dpy ()
{
    return priv->dpy;
}

bool
CompScreen::XRandr ()
{
    return priv->randrExtension;
}

int
CompScreen::randrEvent ()
{
    return priv->randrEvent;
}

bool
CompScreen::XShape ()
{
    return priv->shapeExtension;
}

int
CompScreen::shapeEvent ()
{
    return priv->shapeEvent;
}

int
CompScreen::syncEvent ()
{
    return priv->syncEvent;
}

SnDisplay *
CompScreen::snDisplay ()
{
    return priv->snDisplay;
}

Window
CompScreen::activeWindow ()
{
    return priv->activeWindow;
}

Window
CompScreen::autoRaiseWindow ()
{
    return priv->autoRaiseWindow;
}

const char *
CompScreen::displayString ()
{
    return priv->displayString;
}

void
PrivateScreen::updateScreenInfo ()
{
    if (xineramaExtension)
    {
	int nInfo;
	XineramaScreenInfo *info = XineramaQueryScreens (dpy, &nInfo);

	screenInfo = std::vector<XineramaScreenInfo> (info, info + nInfo);

	if (info)
	    XFree (info);
    }
}

void
PrivateScreen::setAudibleBell (bool audible)
{
    if (xkbExtension)
	XkbChangeEnabledControls (dpy,
				  XkbUseCoreKbd,
				  XkbAudibleBellMask,
				  audible ? XkbAudibleBellMask : 0);
}

bool
PrivateScreen::handlePingTimeout ()
{
    XEvent      ev;
    int		ping = lastPing + 1;

    ev.type		    = ClientMessage;
    ev.xclient.window	    = 0;
    ev.xclient.message_type = Atoms::wmProtocols;
    ev.xclient.format	    = 32;
    ev.xclient.data.l[0]    = Atoms::wmPing;
    ev.xclient.data.l[1]    = ping;
    ev.xclient.data.l[2]    = 0;
    ev.xclient.data.l[3]    = 0;
    ev.xclient.data.l[4]    = 0;

    foreach (CompWindow *w, windows)
    {
	if (w->priv->handlePingTimeout (lastPing))
	{
	    ev.xclient.window    = w->id ();
	    ev.xclient.data.l[2] = w->id ();

	    XSendEvent (dpy, w->id (), false, NoEventMask, &ev);
	}
    }

    lastPing = ping;

    return true;
}

CompOption::Vector &
CompScreen::getOptions ()
{
    return priv->getOptions ();
}

bool
CompScreen::setOption (const CompString  &name,
		       CompOption::Value &value)
{
    return priv->setOption (name, value);
}

bool
PrivateScreen::setOption (const CompString  &name,
			  CompOption::Value &value)
{
    unsigned int index;

    bool rv = CoreOptions::setOption (name, value);

    if (!rv)
	return false;

    if (!CompOption::findOption (getOptions (), name, &index))
        return false;

    switch (index) {
	case CoreOptions::ActivePlugins:
	    dirtyPluginList = true;
	    break;
	case CoreOptions::PingDelay:
	    pingTimer.setTimes (optionGetPingDelay (),
				optionGetPingDelay () + 500);
	    break;
	case CoreOptions::AudibleBell:
	    setAudibleBell (optionGetAudibleBell ());
	    break;
	case CoreOptions::DetectOutputs:
	    if (optionGetDetectOutputs ())
		detectOutputDevices ();
	    break;
	case CoreOptions::Hsize:
	case CoreOptions::Vsize:

	    if (optionGetHsize () * screen->width () > MAXSHORT)
		return false;
	    if (optionGetVsize () * screen->height () > MAXSHORT)
		return false;

	    setVirtualScreenSize (optionGetHsize (), optionGetVsize ());
	    break;
	case CoreOptions::NumberOfDesktops:
	    setNumberOfDesktops (optionGetNumberOfDesktops ());
	    break;
	case CoreOptions::DefaultIcon:
	    return screen->updateDefaultIcon ();
	    break;
	case CoreOptions::Outputs:
	    if (!noDetection && optionGetDetectOutputs ())
		return false;
	    updateOutputDevices ();
	    break;
	default:
	    break;
    }

    return rv;
}

void
PrivateScreen::processEvents ()
{
    XEvent event, peekEvent;

    /* remove destroyed windows */
    removeDestroyed ();

    if (dirtyPluginList)
	updatePlugins ();

    while (XPending (dpy))
    {
	XNextEvent (dpy, &event);

	switch (event.type) {
	case ButtonPress:
	case ButtonRelease:
	    pointerX = event.xbutton.x_root;
	    pointerY = event.xbutton.y_root;
	    pointerMods = event.xbutton.state;
	    break;
	case KeyPress:
	case KeyRelease:
	    pointerX = event.xkey.x_root;
	    pointerY = event.xkey.y_root;
	    pointerMods = event.xbutton.state;
	    break;
	case MotionNotify:
	    while (XPending (dpy))
	    {
		XPeekEvent (dpy, &peekEvent);

		if (peekEvent.type != MotionNotify)
		    break;

		XNextEvent (dpy, &event);
	    }

	    pointerX = event.xmotion.x_root;
	    pointerY = event.xmotion.y_root;
	    pointerMods = event.xbutton.state;
	    break;
	case EnterNotify:
	case LeaveNotify:
	    pointerX = event.xcrossing.x_root;
	    pointerY = event.xcrossing.y_root;
	    pointerMods = event.xbutton.state;
	    break;
	case ClientMessage:
	    if (event.xclient.message_type == Atoms::xdndPosition)
	    {
		pointerX = event.xclient.data.l[2] >> 16;
		pointerY = event.xclient.data.l[2] & 0xffff;
		/* FIXME: Xdnd provides us no way of getting the pointer mods
		 * without doing XQueryPointer, which is a round-trip */
		pointerMods = 0;
	    }
	    else if (event.xclient.message_type == Atoms::wmMoveResize)
	    {
		int i;
		Window child, root;
		/* _NET_WM_MOVERESIZE is most often sent by clients who provide
		 * a special "grab space" on a window for the user to initiate
		 * adjustment by the window manager. Since we don't have a
		 * passive grab on Button1 for active and raised windows, we
		 * need to update the pointer buffer here */

		XQueryPointer (screen->dpy (), screen->root (),
			       &root, &child, &pointerX, &pointerY,
			       &i, &i, &pointerMods);
	    }
	    break;
	default:
	    break;
        }

	sn_display_process_event (snDisplay, &event);

	inHandleEvent = true;
	screen->handleEvent (&event);
	inHandleEvent = false;

	lastPointerX = pointerX;
	lastPointerY = pointerY;
	lastPointerMods = pointerMods;
    }
}

void
PrivateScreen::updatePlugins ()
{
    CompPlugin                *p;
    unsigned int              nPop, i, j, pListCount = 1;
    CompOption::Value::Vector pList;
    CompPlugin::List          pop;
    bool                      failedPush;


    dirtyPluginList = false;

    CompOption::Value::Vector &list = optionGetActivePlugins ();

    /* Determine the number of plugins, which is core +
     * initial plugins + plugins in option list in addition
     * to initial plugins */
    foreach (CompString &pn, initialPlugins)
    {
	if (pn != "core")
	    pListCount++;
    }

    foreach (CompOption::Value &lp, list)
    {
	bool skip = false;
	if (lp.s () == "core")
	    continue;

	foreach (CompString &p, initialPlugins)
	{
	    if (p == lp.s ())
	    {
		skip = true;
		break;
	    }
	}

	/* plugin not in initial list */
	if (!skip)
	    pListCount++;
    }

    /* dupPluginCount is now the number of plugisn contained in both the
     * initial and new plugins list */
    pList.resize (pListCount);

    if (pList.empty ())
    {
	screen->setOptionForPlugin ("core", "active_plugins", plugin);
	return;
    }

    /* Must have core as first plugin */
    pList.at (0) = "core";
    j = 1;

    /* Add initial plugins */
    foreach (CompString &p, initialPlugins)
    {
	if (p == "core")
	    continue;
	pList.at (j).set (p);
	j++;
    }

    /* Add plugins not in the initial list */
    foreach (CompOption::Value &opt, list)
    {
	std::list <CompString>::iterator it = initialPlugins.begin ();
	bool				 skip = false;
	if (opt.s () == "core")
	    continue;

	for (; it != initialPlugins.end (); it++)
	{
	    if ((*it) == opt.s ())
	    {
		skip = true;
		break;
	    }
	}

	if (!skip)
	    pList.at (j++).set (opt.s ());
    }

    assert (j == pList.size ());

    /* j is initialized to 1 to make sure we never pop the core plugin */
    for (i = j = 1; j < plugin.list ().size () && i < pList.size (); i++, j++)
    {
	if (plugin.list ().at (j).s () != pList.at (i).s ())
	    break;
    }

    nPop = plugin.list ().size () - j;

    if (nPop)
    {
	for (j = 0; j < nPop; j++)
	{
	    pop.push_back (CompPlugin::pop ());
	    plugin.list ().pop_back ();
	}
    }

    for (; i < pList.size (); i++)
    {
	p = NULL;
	failedPush = false;
	foreach (CompPlugin *pp, pop)
	{
	    if (pList[i]. s () == pp->vTable->name ())
	    {
		if (CompPlugin::push (pp))
		{
		    p = pp;
		    pop.erase (std::find (pop.begin (), pop.end (), pp));
		    break;
		}
		else
		{
		    pop.erase (std::find (pop.begin (), pop.end (), pp));
		    CompPlugin::unload (pp);
		    p = NULL;
		    failedPush = true;
		    break;
		}
	    }
	}

	if (p == 0 && !failedPush)
	{
	    p = CompPlugin::load (pList[i].s ().c_str ());
	    if (p)
	    {
		if (!CompPlugin::push (p))
		{
		    CompPlugin::unload (p);
		    p = 0;
		}
	    }
	}

	if (p)
	    plugin.list ().push_back (p->vTable->name ());
    }

    foreach (CompPlugin *pp, pop)
	CompPlugin::unload (pp);

    if (!priv->dirtyPluginList)
	screen->setOptionForPlugin ("core", "active_plugins", plugin);
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static bool
convertProperty (Display *dpy,
		 Time    time,
		 Window  w,
		 Atom    target,
		 Atom    property)
{

#define N_TARGETS 4

    Atom conversionTargets[N_TARGETS];
    long icccmVersion[] = { 2, 0 };

    conversionTargets[0] = Atoms::targets;
    conversionTargets[1] = Atoms::multiple;
    conversionTargets[2] = Atoms::timestamp;
    conversionTargets[3] = Atoms::version;

    if (target == Atoms::targets)
	XChangeProperty (dpy, w, property,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) conversionTargets, N_TARGETS);
    else if (target == Atoms::timestamp)
	XChangeProperty (dpy, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) &time, 1);
    else if (target == Atoms::version)
	XChangeProperty (dpy, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) icccmVersion, 2);
    else
	return false;

    /* Be sure the PropertyNotify has arrived so we
     * can send SelectionNotify
     */
    XSync (dpy, false);

    return true;
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
void
PrivateScreen::handleSelectionRequest (XEvent *event)
{
    XSelectionEvent reply;

    if (wmSnSelectionWindow != event->xselectionrequest.owner ||
	wmSnAtom != event->xselectionrequest.selection)
	return;

    reply.type	    = SelectionNotify;
    reply.display   = dpy;
    reply.requestor = event->xselectionrequest.requestor;
    reply.selection = event->xselectionrequest.selection;
    reply.target    = event->xselectionrequest.target;
    reply.property  = None;
    reply.time	    = event->xselectionrequest.time;

    if (event->xselectionrequest.target == Atoms::multiple)
    {
	if (event->xselectionrequest.property != None)
	{
	    Atom	  type, *adata;
	    int		  i, format;
	    unsigned long num, rest;
	    unsigned char *data;

	    if (XGetWindowProperty (dpy,
				    event->xselectionrequest.requestor,
				    event->xselectionrequest.property,
				    0, 256, false,
				    Atoms::atomPair,
				    &type, &format, &num, &rest,
				    &data) != Success)
		return;

	    /* FIXME: to be 100% correct, should deal with rest > 0,
	     * but since we have 4 possible targets, we will hardly ever
	     * meet multiple requests with a length > 8
	     */
	    adata = (Atom *) data;
	    i = 0;
	    while (i < (int) num)
	    {
		if (!convertProperty (dpy, wmSnTimestamp,
				      event->xselectionrequest.requestor,
				      adata[i], adata[i + 1]))
		    adata[i + 1] = None;

		i += 2;
	    }

	    XChangeProperty (dpy,
			     event->xselectionrequest.requestor,
			     event->xselectionrequest.property,
			     Atoms::atomPair,
			     32, PropModeReplace, data, num);

	    if (data)
		XFree (data);
	}
    }
    else
    {
	if (event->xselectionrequest.property == None)
	    event->xselectionrequest.property = event->xselectionrequest.target;

	if (convertProperty (dpy, wmSnTimestamp,
			     event->xselectionrequest.requestor,
			     event->xselectionrequest.target,
			     event->xselectionrequest.property))
	    reply.property = event->xselectionrequest.property;
    }

    XSendEvent (dpy, event->xselectionrequest.requestor,
		false, 0L, (XEvent *) &reply);
}

void
PrivateScreen::handleSelectionClear (XEvent *event)
{
    /* We need to unmanage the screen on which we lost the selection */
    if (wmSnSelectionWindow != event->xselectionclear.window ||
	wmSnAtom != event->xselectionclear.selection)
	return;

    shutDown = true;
}

#define HOME_IMAGEDIR ".compiz-1/images"

bool
CompScreen::readImageFromFile (CompString &name,
			       CompString &pname,
			       CompSize   &size,
			       void       *&data)
{
    bool status;
    int  stride;

    status = fileToImage (name, size, stride, data);
    if (!status)
    {
	char       *home = getenv ("HOME");
	CompString path;
	if (home)
	{
	    path =  home;
	    path += "/";
	    path += HOME_IMAGEDIR;
	    path += "/";
	    path += pname;
	    path += "/";
	    path += name;

	    status = fileToImage (path, size, stride, data);

	    if (status)
		return true;
	}

	path = IMAGEDIR;
	path += "/";
	path += pname;
	path += "/";
	path += name;
	status = fileToImage (path, size, stride, data);
    }

    return status;
}

bool
CompScreen::writeImageToFile (CompString &path,
			      const char *format,
			      CompSize   &size,
			      void       *data)
{
    CompString formatString (format);
    return imageToFile (path, formatString, size, size.width () * 4, data);
}

Window
PrivateScreen::getActiveWindow (Window root)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    Window	  w = None;

    result = XGetWindowProperty (priv->dpy, root,
				 Atoms::winActive, 0L, 1L, false,
				 XA_WINDOW, &actual, &format,
				 &n, &left, &data);

    if (result == Success && data)
    {
	if (n)
	    memcpy (&w, data, sizeof (Window));
	XFree (data);
    }

    return w;
}

bool
CompScreen::fileToImage (CompString &name,
			 CompSize   &size,
			 int        &stride,
			 void       *&data)
{
    WRAPABLE_HND_FUNC_RETURN (8, bool, fileToImage, name, size, stride, data);
    return false;
}

bool
CompScreen::imageToFile (CompString &path,
			 CompString &format,
			 CompSize   &size,
			 int        stride,
			 void       *data)
{
    WRAPABLE_HND_FUNC_RETURN (9, bool, imageToFile, path, format, size,
			      stride, data)
    return false;
}

const char *
logLevelToString (CompLogLevel level)
{
    switch (level) {
    case CompLogLevelFatal:
	return "Fatal";
    case CompLogLevelError:
	return "Error";
    case CompLogLevelWarn:
	return "Warn";
    case CompLogLevelInfo:
	return "Info";
    case CompLogLevelDebug:
	return "Debug";
    default:
	break;
    }

    return "Unknown";
}

static void
logMessage (const char   *componentName,
	    CompLogLevel level,
	    const char   *message)
{
    if (!debugOutput && level >= CompLogLevelDebug)
	return;

    fprintf (stderr, "%s (%s) - %s: %s\n",
	     programName, componentName,
	     logLevelToString (level), message);
}

void
CompScreen::logMessage (const char   *componentName,
			CompLogLevel level,
			const char   *message)
{
    WRAPABLE_HND_FUNC (13, logMessage, componentName, level, message)
    ::logMessage (componentName, level, message);
}

void
compLogMessage (const char   *componentName,
	        CompLogLevel level,
	        const char   *format,
	        ...)
{
    va_list args;
    char    message[2048];

    va_start (args, format);

    vsnprintf (message, 2048, format, args);

    if (screen)
	screen->logMessage (componentName, level, message);
    else
	logMessage (componentName, level, message);

    va_end (args);
}

int
PrivateScreen::getWmState (Window id)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    unsigned long state = NormalState;

    result = XGetWindowProperty (priv->dpy, id,
				 Atoms::wmState, 0L, 2L, false,
				 Atoms::wmState, &actual, &format,
				 &n, &left, &data);

    if (result == Success && data)
    {
	if (n)
	    memcpy (&state, data, sizeof (unsigned long));
	XFree ((void *) data);
    }

    return state;
}

void
PrivateScreen::setWmState (int state, Window id)
{
    unsigned long data[2];

    data[0] = state;
    data[1] = None;

    XChangeProperty (priv->dpy, id,
		     Atoms::wmState, Atoms::wmState,
		     32, PropModeReplace, (unsigned char *) data, 2);
}

unsigned int
PrivateScreen::windowStateMask (Atom state)
{
    if (state == Atoms::winStateModal)
	return CompWindowStateModalMask;
    else if (state == Atoms::winStateSticky)
	return CompWindowStateStickyMask;
    else if (state == Atoms::winStateMaximizedVert)
	return CompWindowStateMaximizedVertMask;
    else if (state == Atoms::winStateMaximizedHorz)
	return CompWindowStateMaximizedHorzMask;
    else if (state == Atoms::winStateShaded)
	return CompWindowStateShadedMask;
    else if (state == Atoms::winStateSkipTaskbar)
	return CompWindowStateSkipTaskbarMask;
    else if (state == Atoms::winStateSkipPager)
	return CompWindowStateSkipPagerMask;
    else if (state == Atoms::winStateHidden)
	return CompWindowStateHiddenMask;
    else if (state == Atoms::winStateFullscreen)
	return CompWindowStateFullscreenMask;
    else if (state == Atoms::winStateAbove)
	return CompWindowStateAboveMask;
    else if (state == Atoms::winStateBelow)
	return CompWindowStateBelowMask;
    else if (state == Atoms::winStateDemandsAttention)
	return CompWindowStateDemandsAttentionMask;
    else if (state == Atoms::winStateDisplayModal)
	return CompWindowStateDisplayModalMask;

    return 0;
}

unsigned int
PrivateScreen::windowStateFromString (const char *str)
{
    if (strcasecmp (str, "modal") == 0)
	return CompWindowStateModalMask;
    else if (strcasecmp (str, "sticky") == 0)
	return CompWindowStateStickyMask;
    else if (strcasecmp (str, "maxvert") == 0)
	return CompWindowStateMaximizedVertMask;
    else if (strcasecmp (str, "maxhorz") == 0)
	return CompWindowStateMaximizedHorzMask;
    else if (strcasecmp (str, "shaded") == 0)
	return CompWindowStateShadedMask;
    else if (strcasecmp (str, "skiptaskbar") == 0)
	return CompWindowStateSkipTaskbarMask;
    else if (strcasecmp (str, "skippager") == 0)
	return CompWindowStateSkipPagerMask;
    else if (strcasecmp (str, "hidden") == 0)
	return CompWindowStateHiddenMask;
    else if (strcasecmp (str, "fullscreen") == 0)
	return CompWindowStateFullscreenMask;
    else if (strcasecmp (str, "above") == 0)
	return CompWindowStateAboveMask;
    else if (strcasecmp (str, "below") == 0)
	return CompWindowStateBelowMask;
    else if (strcasecmp (str, "demandsattention") == 0)
	return CompWindowStateDemandsAttentionMask;

    return 0;
}

unsigned int
PrivateScreen::getWindowState (Window id)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    unsigned int  state = 0;

    result = XGetWindowProperty (priv->dpy, id,
				 Atoms::winState,
				 0L, 1024L, false, XA_ATOM, &actual, &format,
				 &n, &left, &data);

    if (result == Success && data)
    {
	Atom *a = (Atom *) data;

	while (n--)
	    state |= windowStateMask (*a++);

	XFree ((void *) data);
    }

    return state;
}

void
PrivateScreen::setWindowState (unsigned int state, Window id)
{
    Atom data[32];
    int	 i = 0;

    if (state & CompWindowStateModalMask)
	data[i++] = Atoms::winStateModal;
    if (state & CompWindowStateStickyMask)
	data[i++] = Atoms::winStateSticky;
    if (state & CompWindowStateMaximizedVertMask)
	data[i++] = Atoms::winStateMaximizedVert;
    if (state & CompWindowStateMaximizedHorzMask)
	data[i++] = Atoms::winStateMaximizedHorz;
    if (state & CompWindowStateShadedMask)
	data[i++] = Atoms::winStateShaded;
    if (state & CompWindowStateSkipTaskbarMask)
	data[i++] = Atoms::winStateSkipTaskbar;
    if (state & CompWindowStateSkipPagerMask)
	data[i++] = Atoms::winStateSkipPager;
    if (state & CompWindowStateHiddenMask)
	data[i++] = Atoms::winStateHidden;
    if (state & CompWindowStateFullscreenMask)
	data[i++] = Atoms::winStateFullscreen;
    if (state & CompWindowStateAboveMask)
	data[i++] = Atoms::winStateAbove;
    if (state & CompWindowStateBelowMask)
	data[i++] = Atoms::winStateBelow;
    if (state & CompWindowStateDemandsAttentionMask)
	data[i++] = Atoms::winStateDemandsAttention;
    if (state & CompWindowStateDisplayModalMask)
	data[i++] = Atoms::winStateDisplayModal;

    XChangeProperty (priv->dpy, id, Atoms::winState,
		     XA_ATOM, 32, PropModeReplace,
		     (unsigned char *) data, i);
}

unsigned int
PrivateScreen::getWindowType (Window id)
{
    Atom	  actual, a = None;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;

    result = XGetWindowProperty (priv->dpy , id,
				 Atoms::winType,
				 0L, 1L, false, XA_ATOM, &actual, &format,
				 &n, &left, &data);

    if (result == Success && data)
    {
	if (n)
	    memcpy (&a, data, sizeof (Atom));
	XFree ((void *) data);
    }

    if (a)
    {
	if (a == Atoms::winTypeNormal)
	    return CompWindowTypeNormalMask;
	else if (a == Atoms::winTypeMenu)
	    return CompWindowTypeMenuMask;
	else if (a == Atoms::winTypeDesktop)
	    return CompWindowTypeDesktopMask;
	else if (a == Atoms::winTypeDock)
	    return CompWindowTypeDockMask;
	else if (a == Atoms::winTypeToolbar)
	    return CompWindowTypeToolbarMask;
	else if (a == Atoms::winTypeUtil)
	    return CompWindowTypeUtilMask;
	else if (a == Atoms::winTypeSplash)
	    return CompWindowTypeSplashMask;
	else if (a == Atoms::winTypeDialog)
	    return CompWindowTypeDialogMask;
	else if (a == Atoms::winTypeDropdownMenu)
	    return CompWindowTypeDropdownMenuMask;
	else if (a == Atoms::winTypePopupMenu)
	    return CompWindowTypePopupMenuMask;
	else if (a == Atoms::winTypeTooltip)
	    return CompWindowTypeTooltipMask;
	else if (a == Atoms::winTypeNotification)
	    return CompWindowTypeNotificationMask;
	else if (a == Atoms::winTypeCombo)
	    return CompWindowTypeComboMask;
	else if (a == Atoms::winTypeDnd)
	    return CompWindowTypeDndMask;
    }

    return CompWindowTypeUnknownMask;
}

void
PrivateScreen::getMwmHints (Window       id,
			    unsigned int *func,
			    unsigned int *decor)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;

    *func  = MwmFuncAll;
    *decor = MwmDecorAll;

    result = XGetWindowProperty (priv->dpy, id,
				 Atoms::mwmHints,
				 0L, 20L, false, Atoms::mwmHints,
				 &actual, &format, &n, &left, &data);

    if (result == Success && data)
    {
	MwmHints *mwmHints = (MwmHints *) data;

	if (n >= PropMotifWmHintElements)
	{
	    if (mwmHints->flags & MwmHintsDecorations)
		*decor = mwmHints->decorations;

	    if (mwmHints->flags & MwmHintsFunctions)
		*func = mwmHints->functions;
	}

	XFree (data);
    }
}

unsigned int
PrivateScreen::getProtocols (Window id)
{
    Atom         *protocol;
    int          count;
    unsigned int protocols = 0;

    if (XGetWMProtocols (priv->dpy, id, &protocol, &count))
    {
	int  i;

	for (i = 0; i < count; i++)
	{
	    if (protocol[i] == Atoms::wmDeleteWindow)
		protocols |= CompWindowProtocolDeleteMask;
	    else if (protocol[i] == Atoms::wmTakeFocus)
		protocols |= CompWindowProtocolTakeFocusMask;
	    else if (protocol[i] == Atoms::wmPing)
		protocols |= CompWindowProtocolPingMask;
	    else if (protocol[i] == Atoms::wmSyncRequest)
		protocols |= CompWindowProtocolSyncRequestMask;
	}

	XFree (protocol);
    }

    return protocols;
}

unsigned int
CompScreen::getWindowProp (Window       id,
			   Atom         property,
			   unsigned int defaultValue)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    unsigned int  retval = defaultValue;

    result = XGetWindowProperty (priv->dpy, id, property,
				 0L, 1L, false, XA_CARDINAL, &actual, &format,
				 &n, &left, &data);

    if (result == Success && data)
    {
	if (n)
	{
	    unsigned long value;
	    memcpy (&value, data, sizeof (unsigned long));
	    retval = (unsigned int) value;
	}

	XFree (data);
    }

    return retval;
}

void
CompScreen::setWindowProp (Window       id,
			   Atom         property,
			   unsigned int value)
{
    unsigned long data = value;

    XChangeProperty (priv->dpy, id, property,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

bool
PrivateScreen::readWindowProp32 (Window         id,
				 Atom           property,
				 unsigned short *returnValue)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    bool          retval = false;

    result = XGetWindowProperty (priv->dpy, id, property,
				 0L, 1L, false, XA_CARDINAL, &actual, &format,
				 &n, &left, &data);

    if (result == Success && data)
    {
	if (n)
	{
	    CARD32 value;

	    memcpy (&value, data, sizeof (CARD32));
	    retval       = true;
	    *returnValue = value >> 16;
	}

	XFree (data);
    }

    return retval;
}

unsigned short
CompScreen::getWindowProp32 (Window         id,
			     Atom           property,
			     unsigned short defaultValue)
{
    unsigned short result;

    if (priv->readWindowProp32 (id, property, &result))
	return result;

    return defaultValue;
}

void
CompScreen::setWindowProp32 (Window         id,
			     Atom           property,
			     unsigned short value)
{
    CARD32 value32;

    value32 = value << 16 | value;

    XChangeProperty (priv->dpy, id, property,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &value32, 1);
}

void
ScreenInterface::handleEvent (XEvent *event)
    WRAPABLE_DEF (handleEvent, event)

void
ScreenInterface::handleCompizEvent (const char         *plugin,
				    const char         *event,
				    CompOption::Vector &options)
    WRAPABLE_DEF (handleCompizEvent, plugin, event, options)

bool
ScreenInterface::fileToImage (CompString &name,
			      CompSize   &size,
			      int        &stride,
			      void       *&data)
    WRAPABLE_DEF (fileToImage, name, size, stride, data)

bool
ScreenInterface::imageToFile (CompString &path,
			      CompString &format,
			      CompSize   &size,
			      int        stride,
			      void       *data)
    WRAPABLE_DEF (imageToFile, path, format, size, stride, data)

CompMatch::Expression *
ScreenInterface::matchInitExp (const CompString& value)
    WRAPABLE_DEF (matchInitExp, value)

void
ScreenInterface::matchExpHandlerChanged ()
    WRAPABLE_DEF (matchExpHandlerChanged)

void
ScreenInterface::matchPropertyChanged (CompWindow *window)
    WRAPABLE_DEF (matchPropertyChanged, window)

void
ScreenInterface::logMessage (const char   *componentName,
			     CompLogLevel level,
			     const char   *message)
    WRAPABLE_DEF (logMessage, componentName, level, message)


bool
PrivateScreen::desktopHintEqual (unsigned long *data,
				 int           size,
				 int           offset,
				 int           hintSize)
{
    if (size != desktopHintSize)
	return false;

    if (memcmp (data + offset,
		desktopHintData + offset,
		hintSize * sizeof (unsigned long)) == 0)
	return true;

    return false;
}

void
PrivateScreen::setDesktopHints ()
{
    unsigned long *data;
    int		  dSize, offset, hintSize;
    unsigned int  i;

    dSize = nDesktop * 2 + nDesktop * 2 + nDesktop * 4 + 1;

    data = (unsigned long *) malloc (sizeof (unsigned long) * dSize);
    if (!data)
	return;

    offset   = 0;
    hintSize = nDesktop * 2;

    for (i = 0; i < nDesktop; i++)
    {
	data[offset + i * 2 + 0] = vp.x () * screen->width ();
	data[offset + i * 2 + 1] = vp.y () * screen->height ();
    }

    if (!desktopHintEqual (data, dSize, offset, hintSize))
	XChangeProperty (dpy, root,
			 Atoms::desktopViewport,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    offset += hintSize;

    for (i = 0; i < nDesktop; i++)
    {
	data[offset + i * 2 + 0] = screen->width () * vpSize.width ();
	data[offset + i * 2 + 1] = screen->height () * vpSize.height ();
    }

    if (!desktopHintEqual (data, dSize, offset, hintSize))
	XChangeProperty (dpy, root,
			 Atoms::desktopGeometry,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    offset += hintSize;
    hintSize = nDesktop * 4;

    for (i = 0; i < nDesktop; i++)
    {
	data[offset + i * 4 + 0] = workArea.x ();
	data[offset + i * 4 + 1] = workArea.y ();
	data[offset + i * 4 + 2] = workArea.width ();
	data[offset + i * 4 + 3] = workArea.height ();
    }

    if (!desktopHintEqual (data, dSize, offset, hintSize))
	XChangeProperty (dpy, root,
			 Atoms::workarea,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    offset += hintSize;

    data[offset] = nDesktop;
    hintSize = 1;

    if (!desktopHintEqual (data, dSize, offset, hintSize))
	XChangeProperty (dpy, root,
			 Atoms::numberOfDesktops,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    if (desktopHintData)
	free (desktopHintData);

    desktopHintData = data;
    desktopHintSize = dSize;
}

void
PrivateScreen::setVirtualScreenSize (int newh, int newv)
{
    /* if newh or newv is being reduced */
    if (newh < screen->vpSize ().width () ||
	newv < screen->vpSize ().height ())
    {
	int        tx = 0;
	int        ty = 0;

	if (screen->vp ().x () >= newh)
	    tx = screen->vp ().x () - (newh - 1);
	if (screen->vp ().y () >= newv)
	    ty = screen->vp ().y () - (newv - 1);

	if (tx != 0 || ty != 0)
	    screen->moveViewport (tx, ty, TRUE);

	/* Move windows that were in one of the deleted viewports into the
	   closest viewport */
	foreach (CompWindow *w, screen->windows ())
	{
	    int moveX = 0;
	    int moveY = 0;

	    if (w->onAllViewports ())
		continue;

	    /* Find which viewport the (inner) window's top-left corner falls
	       in, and check if it's outside the new viewport horizontal and
	       vertical index range */
	    if (newh < screen->vpSize ().width ())
	    {
		int vpX;   /* x index of a window's vp */

		vpX = w->serverX () / screen->width ();
		if (w->serverX () < 0)
		    vpX -= 1;

		vpX += screen->vp ().x (); /* Convert relative to absolute vp index */

		/* Move windows too far right to left */
		if (vpX >= newh)
		    moveX = ((newh - 1) - vpX) * screen->width ();
	    }
	    if (newv < screen->vpSize ().height ())
	    {
		int vpY;   /* y index of a window's vp */

		vpY = w->serverY () / screen->height ();
		if (w->serverY () < 0)
		    vpY -= 1;

		vpY += screen->vp ().y (); /* Convert relative to absolute vp index */

		/* Move windows too far right to left */
		if (vpY >= newv)
		    moveY = ((newv - 1) - vpY) * screen->height ();
	    }

	    if (moveX != 0 || moveY != 0)
	    {
		w->move (moveX, moveY, true);
		w->syncPosition ();
	    }
	}
    }

    vpSize.setWidth (newh);
    vpSize.setHeight (newv);

    setDesktopHints ();
}

void
PrivateScreen::updateOutputDevices ()
{
    CompOption::Value::Vector &list = optionGetOutputs ();
    unsigned int              nOutput = 0;
    int		              x, y, bits;
    unsigned int              uWidth, uHeight;
    int                       width, height;
    int		              x1, y1, x2, y2;
    char                      str[10];

    foreach (CompOption::Value &value, list)
    {
	x      = 0;
	y      = 0;
	uWidth  = (unsigned) screen->width ();
	uHeight = (unsigned) screen->height ();

	bits = XParseGeometry (value.s ().c_str (), &x, &y, &uWidth, &uHeight);
	width  = (int) uWidth;
	height = (int) uHeight;

	if (bits & XNegative)
	    x = screen->width () + x - width;

	if (bits & YNegative)
	    y = screen->height () + y - height;

	x1 = x;
	y1 = y;
	x2 = x + width;
	y2 = y + height;

	if (x1 < 0)
	    x1 = 0;
	if (y1 < 0)
	    y1 = 0;
	if (x2 > screen->width ())
	    x2 = screen->width ();
	if (y2 > screen->height ())
	    y2 = screen->height ();

	if (x1 < x2 && y1 < y2)
	{
	    if (outputDevs.size () < nOutput + 1)
		outputDevs.resize (nOutput + 1);

	    outputDevs[nOutput].setGeometry (x1, y1, x2 - x1, y2 - y1);
	    nOutput++;
	}
    }

    /* make sure we have at least one output */
    if (!nOutput)
    {
	if (outputDevs.size () < 1)
	    outputDevs.resize (1);

	outputDevs[0].setGeometry (0, 0, screen->width (), screen->height ());
	nOutput = 1;
    }

    if (outputDevs.size () > nOutput)
	outputDevs.resize (nOutput);

    /* set name, width, height and update rect pointers in all regions */
    for (unsigned int i = 0; i < nOutput; i++)
    {
	snprintf (str, 10, "Output %d", i);
	outputDevs[i].setId (str, i);
    }

    hasOverlappingOutputs = false;

    setCurrentOutput (currentOutputDev);

    /* clear out fullscreen monitor hints of all windows as
       suggested on monitor layout changes in EWMH */
    foreach (CompWindow *w, windows)
	if (w->priv->fullscreenMonitorsSet)
	    w->priv->setFullscreenMonitors (NULL);

    for (unsigned int i = 0; i < nOutput - 1; i++)
	for (unsigned int j = i + 1; j < nOutput; j++)
	    if (outputDevs[i].intersects (outputDevs[j]))
		hasOverlappingOutputs = true;

    screen->updateWorkarea ();

    screen->outputChangeNotify ();
}

void
PrivateScreen::detectOutputDevices ()
{
    if (!noDetection && optionGetDetectOutputs ())
    {
	CompString	  name;
	CompOption::Value value;

	if (screenInfo.size ())
	{
	    CompOption::Value::Vector l;
	    foreach (XineramaScreenInfo xi, screenInfo)
	    {
		l.push_back (compPrintf ("%dx%d+%d+%d", xi.width, xi.height,
					 xi.x_org, xi.y_org));
	    }

	    value.set (CompOption::TypeString, l);
	}
	else
	{
	    CompOption::Value::Vector l;
	    l.push_back (compPrintf ("%dx%d+%d+%d", screen->width (),
				     screen->height (), 0, 0));
	    value.set (CompOption::TypeString, l);
	}

	mOptions[CoreOptions::DetectOutputs].value ().set (false);
	screen->setOptionForPlugin ("core", "outputs", value);
	mOptions[CoreOptions::DetectOutputs].value ().set (true);

    }
    else
    {
	updateOutputDevices ();
    }
}


void
PrivateScreen::updateStartupFeedback ()
{
    if (!startupSequences.empty ())
	XDefineCursor (dpy, root, busyCursor);
    else
	XDefineCursor (dpy, root, normalCursor);
}

#define STARTUP_TIMEOUT_DELAY 15000

bool
PrivateScreen::handleStartupSequenceTimeout ()
{
    struct timeval	now, active;
    double		elapsed;

    gettimeofday (&now, NULL);

    foreach (CompStartupSequence *s, startupSequences)
    {
	sn_startup_sequence_get_last_active_time (s->sequence,
						  &active.tv_sec,
						  &active.tv_usec);

	elapsed = ((((double) now.tv_sec - active.tv_sec) * 1000000.0 +
		    (now.tv_usec - active.tv_usec))) / 1000.0;

	if (elapsed > STARTUP_TIMEOUT_DELAY)
	    sn_startup_sequence_complete (s->sequence);
    }

    return true;
}

void
PrivateScreen::addSequence (SnStartupSequence *sequence)
{
    CompStartupSequence *s;

    s = new CompStartupSequence ();
    if (!s)
	return;

    sn_startup_sequence_ref (sequence);

    s->sequence = sequence;
    s->viewportX = vp.x ();
    s->viewportY = vp.y ();

    startupSequences.push_front (s);

    if (!startupSequenceTimer.active ())
	startupSequenceTimer.start ();

    updateStartupFeedback ();
}

void
PrivateScreen::removeSequence (SnStartupSequence *sequence)
{
    CompStartupSequence *s = NULL;

    std::list<CompStartupSequence *>::iterator it = startupSequences.begin ();

    for (; it != startupSequences.end (); it++)
    {
	if ((*it)->sequence == sequence)
	{
	    s = (*it);
	    break;
	}
    }

    if (!s)
	return;

    sn_startup_sequence_unref (sequence);

    startupSequences.erase (it);

    delete s;

    if (startupSequences.empty () && startupSequenceTimer.active ())
	startupSequenceTimer.stop ();

    updateStartupFeedback ();
}

void
PrivateScreen::removeAllSequences ()
{
    foreach (CompStartupSequence *s, startupSequences)
    {
	sn_startup_sequence_unref (s->sequence);
	delete s;
    }

    startupSequences.clear ();

    if (startupSequenceTimer.active ())
	startupSequenceTimer.stop ();

    updateStartupFeedback ();
}

void
CompScreen::compScreenSnEvent (SnMonitorEvent *event,
			       void           *userData)
{
    CompScreen	      *screen = (CompScreen *) userData;
    SnStartupSequence *sequence;

    sequence = sn_monitor_event_get_startup_sequence (event);

    switch (sn_monitor_event_get_type (event)) {
    case SN_MONITOR_EVENT_INITIATED:
	screen->priv->addSequence (sequence);
	break;
    case SN_MONITOR_EVENT_COMPLETED:
	screen->priv->removeSequence (sequence);
	break;
    case SN_MONITOR_EVENT_CHANGED:
    case SN_MONITOR_EVENT_CANCELED:
	break;
    }
}

void
PrivateScreen::updateScreenEdges ()
{
    struct screenEdgeGeometry {
	int xw, x0;
	int yh, y0;
	int ww, w0;
	int hh, h0;
    } geometry[SCREEN_EDGE_NUM] = {
	{ 0,  0,   0,  2,   0,  2,   1, -4 }, /* left */
	{ 1, -2,   0,  2,   0,  2,   1, -4 }, /* right */
	{ 0,  2,   0,  0,   1, -4,   0,  2 }, /* top */
	{ 0,  2,   1, -2,   1, -4,   0,  2 }, /* bottom */
	{ 0,  0,   0,  0,   0,  2,   0,  2 }, /* top-left */
	{ 1, -2,   0,  0,   0,  2,   0,  2 }, /* top-right */
	{ 0,  0,   1, -2,   0,  2,   0,  2 }, /* bottom-left */
	{ 1, -2,   1, -2,   0,  2,   0,  2 }  /* bottom-right */
    };
    int i;

    for (i = 0; i < SCREEN_EDGE_NUM; i++)
    {
	if (screenEdge[i].id)
	    XMoveResizeWindow (dpy, screenEdge[i].id,
			       geometry[i].xw * screen->width () +
			       geometry[i].x0,
			       geometry[i].yh * screen->height () +
			       geometry[i].y0,
			       geometry[i].ww * screen->width () +
			       geometry[i].w0,
			       geometry[i].hh * screen->height () +
			       geometry[i].h0);
    }
}

void
PrivateScreen::setCurrentOutput (unsigned int outputNum)
{
    if (outputNum >= priv->outputDevs.size ())
	outputNum = 0;

    priv->currentOutputDev = outputNum;
}

void
PrivateScreen::reshape (int w, int h)
{
    updateScreenInfo ();

    region = CompRegion (0, 0, w, h);

    screen->setWidth (w);
    screen->setHeight (h);

    fullscreenOutput.setId ("fullscreen", ~0);
    fullscreenOutput.setGeometry (0, 0, w, h);

    updateScreenEdges ();
}

void
PrivateScreen::configure (XConfigureEvent *ce)
{
    if (priv->attrib.width  != ce->width ||
	priv->attrib.height != ce->height)
    {
	priv->attrib.width  = ce->width;
	priv->attrib.height = ce->height;

	priv->reshape (ce->width, ce->height);

	priv->detectOutputDevices ();
    }
}

void
PrivateScreen::setSupportingWmCheck ()
{
    XChangeProperty (dpy, grabWindow,
		     Atoms::supportingWmCheck,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *) &grabWindow, 1);

    XChangeProperty (dpy, grabWindow, Atoms::wmName,
		     Atoms::utf8String, 8, PropModeReplace,
		     (unsigned char *) PACKAGE, strlen (PACKAGE));
    XChangeProperty (dpy, grabWindow, Atoms::winState,
		     XA_ATOM, 32, PropModeReplace,
		     (unsigned char *) &Atoms::winStateSkipTaskbar,
		     1);
    XChangeProperty (dpy, grabWindow, Atoms::winState,
		     XA_ATOM, 32, PropModeAppend,
		     (unsigned char *) &Atoms::winStateSkipPager, 1);
    XChangeProperty (dpy, grabWindow, Atoms::winState,
		     XA_ATOM, 32, PropModeAppend,
		     (unsigned char *) &Atoms::winStateHidden, 1);

    XChangeProperty (dpy, root, Atoms::supportingWmCheck,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *) &grabWindow, 1);
}

void
CompScreen::updateSupportedWmHints ()
{
    std::vector<Atom> atoms;

    addSupportedAtoms (atoms);

    XChangeProperty (dpy (), root (), Atoms::supported,
		     XA_ATOM, 32, PropModeReplace,
		     (const unsigned char *) &atoms.at (0), atoms.size ());
}

void
CompScreen::addSupportedAtoms (std::vector<Atom> &atoms)
{
    WRAPABLE_HND_FUNC (17, addSupportedAtoms, atoms);

    atoms.push_back (Atoms::supported);
    atoms.push_back (Atoms::supportingWmCheck);

    atoms.push_back (Atoms::utf8String);

    atoms.push_back (Atoms::clientList);
    atoms.push_back (Atoms::clientListStacking);

    atoms.push_back (Atoms::winActive);

    atoms.push_back (Atoms::desktopViewport);
    atoms.push_back (Atoms::desktopGeometry);
    atoms.push_back (Atoms::currentDesktop);
    atoms.push_back (Atoms::numberOfDesktops);
    atoms.push_back (Atoms::showingDesktop);

    atoms.push_back (Atoms::workarea);

    atoms.push_back (Atoms::wmName);
/*
    atoms.push_back (Atoms::wmVisibleName);
*/

    atoms.push_back (Atoms::wmStrut);
    atoms.push_back (Atoms::wmStrutPartial);

/*
    atoms.push_back (Atoms::wmPid);
*/

    atoms.push_back (Atoms::wmUserTime);
    atoms.push_back (Atoms::frameExtents);
    atoms.push_back (Atoms::frameWindow);

    atoms.push_back (Atoms::winState);
    atoms.push_back (Atoms::winStateModal);
    atoms.push_back (Atoms::winStateSticky);
    atoms.push_back (Atoms::winStateMaximizedVert);
    atoms.push_back (Atoms::winStateMaximizedHorz);
    atoms.push_back (Atoms::winStateShaded);
    atoms.push_back (Atoms::winStateSkipTaskbar);
    atoms.push_back (Atoms::winStateSkipPager);
    atoms.push_back (Atoms::winStateHidden);
    atoms.push_back (Atoms::winStateFullscreen);
    atoms.push_back (Atoms::winStateAbove);
    atoms.push_back (Atoms::winStateBelow);
    atoms.push_back (Atoms::winStateDemandsAttention);

    atoms.push_back (Atoms::winOpacity);
    atoms.push_back (Atoms::winBrightness);

#warning fixme
#if 0
    if (canDoSaturated)
    {
	atoms.push_back (Atoms::winSaturation);
	atoms.push_back (Atoms::winStateDisplayModal);
    }
#endif

    atoms.push_back (Atoms::wmAllowedActions);

    atoms.push_back (Atoms::winActionMove);
    atoms.push_back (Atoms::winActionResize);
    atoms.push_back (Atoms::winActionStick);
    atoms.push_back (Atoms::winActionMinimize);
    atoms.push_back (Atoms::winActionMaximizeHorz);
    atoms.push_back (Atoms::winActionMaximizeVert);
    atoms.push_back (Atoms::winActionFullscreen);
    atoms.push_back (Atoms::winActionClose);
    atoms.push_back (Atoms::winActionShade);
    atoms.push_back (Atoms::winActionChangeDesktop);
    atoms.push_back (Atoms::winActionAbove);
    atoms.push_back (Atoms::winActionBelow);

    atoms.push_back (Atoms::winType);
    atoms.push_back (Atoms::winTypeDesktop);
    atoms.push_back (Atoms::winTypeDock);
    atoms.push_back (Atoms::winTypeToolbar);
    atoms.push_back (Atoms::winTypeMenu);
    atoms.push_back (Atoms::winTypeSplash);
    atoms.push_back (Atoms::winTypeDialog);
    atoms.push_back (Atoms::winTypeUtil);
    atoms.push_back (Atoms::winTypeNormal);

    atoms.push_back (Atoms::wmDeleteWindow);
    atoms.push_back (Atoms::wmPing);

    atoms.push_back (Atoms::wmMoveResize);
    atoms.push_back (Atoms::moveResizeWindow);
    atoms.push_back (Atoms::restackWindow);

    atoms.push_back (Atoms::wmFullscreenMonitors);
}

void
PrivateScreen::getDesktopHints ()
{
    unsigned long data[2];
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *propData;

    if (useDesktopHints)
    {
	result = XGetWindowProperty (dpy, root,
				     Atoms::numberOfDesktops,
				     0L, 1L, false, XA_CARDINAL, &actual,
				     &format, &n, &left, &propData);

	if (result == Success && propData)
	{
	    if (n)
	    {
		memcpy (data, propData, sizeof (unsigned long));
		if (data[0] > 0 && data[0] < 0xffffffff)
		    nDesktop = data[0];
	    }

	    XFree (propData);
	}

	result = XGetWindowProperty (dpy, root,
				     Atoms::desktopViewport, 0L, 2L,
				     false, XA_CARDINAL, &actual, &format,
				     &n, &left, &propData);

	if (result == Success && propData)
	{
	    if (n == 2)
	    {
		memcpy (data, propData, sizeof (unsigned long) * 2);

		if (data[0] / (unsigned int) screen->width () <
					     (unsigned int) vpSize.width () - 1)
		    vp.setX (data[0] / screen->width ());

		if (data[1] / (unsigned int) screen->height () <
					    (unsigned int) vpSize.height () - 1)
		    vp.setY (data[1] / screen->height ());
	    }

	    XFree (propData);
	}

	result = XGetWindowProperty (dpy, root,
				     Atoms::currentDesktop,
				     0L, 1L, false, XA_CARDINAL, &actual,
				     &format, &n, &left, &propData);

	if (result == Success && propData)
	{
	    if (n)
	    {
		memcpy (data, propData, sizeof (unsigned long));
		if (data[0] < nDesktop)
		    currentDesktop = data[0];
	    }

	    XFree (propData);
	}
    }

    result = XGetWindowProperty (dpy, root,
				 Atoms::showingDesktop,
				 0L, 1L, false, XA_CARDINAL, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && propData)
    {
	if (n)
	{
	    memcpy (data, propData, sizeof (unsigned long));
	    if (data[0])
		screen->enterShowDesktopMode ();
	}

	XFree (propData);
    }

    data[0] = currentDesktop;

    XChangeProperty (dpy, root, Atoms::currentDesktop,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 1);

    data[0] = showingDesktopMask ? true : false;

    XChangeProperty (dpy, root, Atoms::showingDesktop,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 1);
}

void
CompScreen::enterShowDesktopMode ()
{
    WRAPABLE_HND_FUNC (14, enterShowDesktopMode)

    unsigned long data = 1;
    int		  count = 0;
    bool          st = priv->optionGetHideSkipTaskbarWindows ();

    priv->showingDesktopMask = ~(CompWindowTypeDesktopMask |
				 CompWindowTypeDockMask);

    foreach (CompWindow *w, priv->windows)
    {
	if ((priv->showingDesktopMask & w->wmType ()) &&
	    (!(w->state () & CompWindowStateSkipTaskbarMask) || st))
	{
	    if (!w->inShowDesktopMode () && !w->grabbed () &&
		w->managed () && w->focus ())
	    {
		w->setShowDesktopMode (true);
		w->windowNotify (CompWindowNotifyEnterShowDesktopMode);
		w->priv->hide ();
	    }
	}

	if (w->inShowDesktopMode ())
	    count++;
    }

    if (!count)
    {
	priv->showingDesktopMask = 0;
	data = 0;
    }

    XChangeProperty (priv->dpy, priv->root,
		     Atoms::showingDesktop,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

void
CompScreen::leaveShowDesktopMode (CompWindow *window)
{
    WRAPABLE_HND_FUNC (15, leaveShowDesktopMode, window)

    unsigned long data = 0;

    if (window)
    {
	if (!window->inShowDesktopMode ())
	    return;

	window->setShowDesktopMode (false);
	window->windowNotify (CompWindowNotifyLeaveShowDesktopMode);
	window->priv->show ();

	/* return if some other window is still in show desktop mode */
	foreach (CompWindow *w, priv->windows)
	    if (w->inShowDesktopMode ())
		return;

	priv->showingDesktopMask = 0;
    }
    else
    {
	priv->showingDesktopMask = 0;

	foreach (CompWindow *w, priv->windows)
	{
	    if (!w->inShowDesktopMode ())
		continue;

	    w->setShowDesktopMode (false);
	    w->windowNotify (CompWindowNotifyLeaveShowDesktopMode);
	    w->priv->show ();
	}

	/* focus default window - most likely this will be the window
	   which had focus before entering showdesktop mode */
	focusDefaultWindow ();
    }

    XChangeProperty (priv->dpy, priv->root,
		     Atoms::showingDesktop,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

void
CompScreen::forEachWindow (CompWindow::ForEach proc)
{
    foreach (CompWindow *w, priv->windows)
	proc (w);
}

void
CompScreen::focusDefaultWindow ()
{
    CompWindow  *w;
    CompWindow  *focus = NULL;

    if (!priv->optionGetClickToFocus ())
    {
	w = findTopLevelWindow (priv->below);

	if (w && w->focus ())
	{
	    if (!(w->type () & (CompWindowTypeDesktopMask |
				CompWindowTypeDockMask)))
		focus = w;
	}
	else
	{
	    bool         status;
	    Window       rootReturn, childReturn;
	    int          dummyInt;
	    unsigned int dummyUInt;

	    /* huh, we didn't find d->below ... perhaps it's out of date;
	       try grabbing it through the server */

	    status = XQueryPointer (dpy (), priv->root, &rootReturn,
				    &childReturn, &dummyInt, &dummyInt,
				    &dummyInt, &dummyInt, &dummyUInt);

	    if (status && rootReturn == priv->root)
	    {
		w = findTopLevelWindow (childReturn);

		if (w && w->focus ())
		{
		    if (!(w->type () & (CompWindowTypeDesktopMask |
					CompWindowTypeDockMask)))
			focus = w;
		}
	    }
	}
    }

    if (!focus)
    {
	for (CompWindowList::reverse_iterator rit = priv->windows.rbegin ();
	     rit != priv->windows.rend (); rit++)
	{
	    w = (*rit);

	    if (w->type () & CompWindowTypeDockMask)
		continue;

	    if (w->focus ())
	    {
		if (focus)
		{
		    if (w->type () & (CompWindowTypeNormalMask |
				      CompWindowTypeDialogMask |
				      CompWindowTypeModalDialogMask))
		    {
			if (PrivateWindow::compareWindowActiveness (focus, w) < 0)
			    focus = w;
		    }
		}
		else
		    focus = w;
	    }
	}
    }

    if (focus)
    {
	if (focus->id () != priv->activeWindow)
	    focus->moveInputFocusTo ();
    }
    else
    {
	XSetInputFocus (priv->dpy, priv->root, RevertToPointerRoot,
			CurrentTime);
    }
}

CompWindow *
CompScreen::findWindow (Window id)
{
    if (lastFoundWindow && lastFoundWindow->id () == id)
    {
	return lastFoundWindow;
    }
    else
    {
        CompWindow::Map::iterator it = priv->windowsMap.find (id);

        if (it != priv->windowsMap.end ())
            return (lastFoundWindow = it->second);
    }

    return 0;
}

CompWindow *
CompScreen::findTopLevelWindow (Window id, bool override_redirect)
{
    CompWindow *w;

    w = findWindow (id);

    if (w)
    {
	if (w->overrideRedirect () && !override_redirect)
	    return NULL;
	else
	    return w;
    }

    foreach (CompWindow *w, priv->windows)
	if (w->frame () == id)
	{
	    if (w->overrideRedirect () && !override_redirect)
		return NULL;
	    else
		return w;
	}

    return NULL;
}

void
CompScreen::insertWindow (CompWindow *w, Window	aboveId)
{
    w->prev = NULL;
    w->next = NULL;

    if (!aboveId || priv->windows.empty ())
    {
	if (!priv->windows.empty ())
	{
	    priv->windows.front ()->prev = w;
	    w->next = priv->windows.front ();
	}
	priv->windows.push_front (w);
        if (w->id () != 1)
            priv->windowsMap[w->id ()] = w;

	return;
    }

    CompWindowList::iterator it = priv->windows.begin ();

    while (it != priv->windows.end ())
    {
	if ((*it)->id () == aboveId ||
	    ((*it)->frame () && (*it)->frame () == aboveId))
	{
	    break;
	}
	it++;
    }

    if (it == priv->windows.end ())
    {
#ifdef DEBUG
	abort ();
#endif
	return;
    }

    w->next = (*it)->next;
    w->prev = (*it);
    (*it)->next = w;

    if (w->next)
    {
	w->next->prev = w;
    }

    priv->windows.insert (++it, w);
    if (w->id () != 1)
        priv->windowsMap[w->id ()] = w;
}

void
PrivateScreen::eraseWindowFromMap (Window id)
{
    if (id != 1)
        priv->windowsMap.erase (id);
}

void
CompScreen::unhookWindow (CompWindow *w)
{
    CompWindowList::iterator it =
	std::find (priv->windows.begin (), priv->windows.end (), w);

    priv->windows.erase (it);
    priv->eraseWindowFromMap (w->id ());

    if (w->next)
	w->next->prev = w->prev;

    if (w->prev)
	w->prev->next = w->next;

    w->next = NULL;
    w->prev = NULL;

    if (w == lastFoundWindow)
	lastFoundWindow = NULL;
}

Cursor
CompScreen::normalCursor ()
{
    return priv->normalCursor;
}

Cursor
CompScreen::invisibleCursor ()
{
    return priv->invisibleCursor;
}

#define POINTER_GRAB_MASK (ButtonReleaseMask | \
			   ButtonPressMask   | \
			   PointerMotionMask)
CompScreen::GrabHandle
CompScreen::pushGrab (Cursor cursor, const char *name)
{
    if (priv->grabs.empty ())
    {
	int status;

	status = XGrabPointer (priv->dpy, priv->grabWindow, true,
			       POINTER_GRAB_MASK,
			       GrabModeAsync, GrabModeAsync,
			       priv->root, cursor,
			       CurrentTime);

	if (status == GrabSuccess)
	{
	    status = XGrabKeyboard (priv->dpy,
				    priv->grabWindow, true,
				    GrabModeAsync, GrabModeAsync,
				    CurrentTime);
	    if (status != GrabSuccess)
	    {
		XUngrabPointer (priv->dpy, CurrentTime);
		return NULL;
	    }
	}
	else
	    return NULL;
    }
    else
    {
	XChangeActivePointerGrab (priv->dpy, POINTER_GRAB_MASK,
				  cursor, CurrentTime);
    }

    PrivateScreen::Grab *grab = new PrivateScreen::Grab ();
    grab->cursor = cursor;
    grab->name   = name;

    priv->grabs.push_back (grab);

    return grab;
}

void
CompScreen::updateGrab (CompScreen::GrabHandle handle, Cursor cursor)
{
    if (!handle)
	return;

    XChangeActivePointerGrab (priv->dpy, POINTER_GRAB_MASK,
			      cursor, CurrentTime);

    ((PrivateScreen::Grab *) handle)->cursor = cursor;
}

void
CompScreen::removeGrab (CompScreen::GrabHandle handle,
			CompPoint *restorePointer)
{
    if (!handle)
	return;

    std::list<PrivateScreen::Grab *>::iterator it;

    it = std::find (priv->grabs.begin (), priv->grabs.end (), handle);

    if (it != priv->grabs.end ())
    {
	priv->grabs.erase (it);
	delete (static_cast<PrivateScreen::Grab *> (handle));
    }
    if (!priv->grabs.empty ())
    {
	XChangeActivePointerGrab (priv->dpy,
				  POINTER_GRAB_MASK,
				  priv->grabs.back ()->cursor,
				  CurrentTime);
    }
    else
    {
	if (restorePointer)
	    warpPointer (restorePointer->x () - pointerX,
			 restorePointer->y () - pointerY);

	XUngrabPointer (priv->dpy, CurrentTime);
	XUngrabKeyboard (priv->dpy, CurrentTime);
    }
}

/* otherScreenGrabExist takes a series of strings terminated by a NULL.
   It returns true if a grab exists but it is NOT held by one of the
   plugins listed, returns false otherwise. */

bool
CompScreen::otherGrabExist (const char *first, ...)
{
    va_list    ap;
    const char *name;

    std::list<PrivateScreen::Grab *>::iterator it;

    for (it = priv->grabs.begin (); it != priv->grabs.end (); it++)
    {
	va_start (ap, first);

	name = first;
	while (name)
	{
	    if (strcmp (name, (*it)->name) == 0)
		break;

	    name = va_arg (ap, const char *);
	}

	va_end (ap);

	if (!name)
	return true;
    }

    return false;
}

bool
CompScreen::grabExist (const char *grab)
{
    foreach (PrivateScreen::Grab* g, priv->grabs)
    {
	if (strcmp (g->name, grab) == 0)
	    return true;
    }
    return false;
}

bool
CompScreen::grabbed ()
{
    return priv->grabbed;
}

void
PrivateScreen::grabUngrabOneKey (unsigned int modifiers,
				 int          keycode,
				 bool         grab)
{
    if (grab)
    {
	XGrabKey (dpy,
		  keycode,
		  modifiers,
		  root,
		  true,
		  GrabModeAsync,
		  GrabModeAsync);
    }
    else
    {
	XUngrabKey (dpy,
		    keycode,
		    modifiers,
		    root);
    }
}

bool
PrivateScreen::grabUngrabKeys (unsigned int modifiers,
			       int          keycode,
			       bool         grab)
{
    int             mod, k;
    unsigned int    ignore;

    CompScreen::checkForError (dpy);

    for (ignore = 0; ignore <= modHandler->ignoredModMask (); ignore++)
    {
	if (ignore & ~modHandler->ignoredModMask ())
	    continue;

	if (keycode != 0)
	{
	    grabUngrabOneKey (modifiers | ignore, keycode, grab);
	}
	else
	{
	    for (mod = 0; mod < 8; mod++)
	    {
		if (modifiers & (1 << mod))
		{
		    for (k = mod * modHandler->modMap ()->max_keypermod;
			 k < (mod + 1) * modHandler->modMap ()->max_keypermod;
			 k++)
		    {
			if (modHandler->modMap ()->modifiermap[k])
			{
			    grabUngrabOneKey ((modifiers & ~(1 << mod)) |
					      ignore,
					      modHandler->modMap ()->modifiermap[k],
					      grab);
			}
		    }
		}
	    }
	}

	if (CompScreen::checkForError (dpy))
	    return false;
    }

    return true;
}

bool
PrivateScreen::addPassiveKeyGrab (CompAction::KeyBinding &key)
{
    KeyGrab                      newKeyGrab;
    unsigned int                 mask;
    std::list<KeyGrab>::iterator it;

    mask = modHandler->virtualToRealModMask (key.modifiers ());

    for (it = keyGrabs.begin (); it != keyGrabs.end (); it++)
    {
	if (key.keycode () == (*it).keycode &&
	    mask           == (*it).modifiers)
	{
	    (*it).count++;
	    return true;
	}
    }



    if (!(mask & CompNoMask))
    {
	if (!grabUngrabKeys (mask, key.keycode (), true))
	    return false;
    }

    newKeyGrab.keycode   = key.keycode ();
    newKeyGrab.modifiers = mask;
    newKeyGrab.count     = 1;

    keyGrabs.push_back (newKeyGrab);

    return true;
}

void
PrivateScreen::removePassiveKeyGrab (CompAction::KeyBinding &key)
{
    unsigned int                 mask;
    std::list<KeyGrab>::iterator it;

    mask = modHandler->virtualToRealModMask (key.modifiers ());

    for (it = keyGrabs.begin (); it != keyGrabs.end (); it++)
    {
	if (key.keycode () == (*it).keycode &&
	    mask           == (*it).modifiers)
	{
	    (*it).count--;
	    if ((*it).count)
		return;

	    it = keyGrabs.erase (it);

	    if (!(mask & CompNoMask))
		grabUngrabKeys (mask, key.keycode (), false);
	}
    }
}

void
PrivateScreen::updatePassiveKeyGrabs ()
{
    std::list<KeyGrab>::iterator it;

    XUngrabKey (dpy, AnyKey, AnyModifier, root);

    for (it = keyGrabs.begin (); it != keyGrabs.end (); it++)
    {
	if (!((*it).modifiers & CompNoMask))
	{
	    grabUngrabKeys ((*it).modifiers,
			    (*it).keycode, true);
	}
    }
}

bool
PrivateScreen::addPassiveButtonGrab (CompAction::ButtonBinding &button)
{
    ButtonGrab                      newButtonGrab;
    std::list<ButtonGrab>::iterator it;

    for (it = buttonGrabs.begin (); it != buttonGrabs.end (); it++)
    {
	if (button.button ()    == (*it).button &&
	    button.modifiers () == (*it).modifiers)
	{
	    (*it).count++;
	    return true;
	}
    }

    newButtonGrab.button    = button.button ();
    newButtonGrab.modifiers = button.modifiers ();
    newButtonGrab.count     = 1;

    buttonGrabs.push_back (newButtonGrab);

    foreach (CompWindow *w, screen->windows ())
	w->priv->updatePassiveButtonGrabs ();

    return true;
}

void
PrivateScreen::removePassiveButtonGrab (CompAction::ButtonBinding &button)
{
    std::list<ButtonGrab>::iterator it;

    for (it = buttonGrabs.begin (); it != buttonGrabs.end (); it++)
    {
	if (button.button ()    == (*it).button &&
	    button.modifiers () == (*it).modifiers)
	{
	    (*it).count--;
	    if ((*it).count)
		return;

	    it = buttonGrabs.erase (it);

	    foreach (CompWindow *w, screen->windows ())
		w->priv->updatePassiveButtonGrabs ();
	}
    }
}

/* add actions that should be automatically added as no screens
   existed when they were initialized. */
void
PrivateScreen::addScreenActions ()
{
    foreach (CompOption &o, mOptions)
    {
	if (!o.isAction ())
	    continue;

	if (o.value ().action ().state () & CompAction::StateAutoGrab)
	    screen->addAction (&o.value ().action ());
    }
}

bool
CompScreen::addAction (CompAction *action)
{
    if (!screenInitalized || !priv->initialized)
	return false;

    if (action->active ())
	return false;

    if (action->type () & CompAction::BindingTypeKey)
    {
	if (!priv->addPassiveKeyGrab (action->key ()))
	    return false;
    }

    if (action->type () & CompAction::BindingTypeButton)
    {
	if (!priv->addPassiveButtonGrab (action->button ()))
	{
	    if (action->type () & CompAction::BindingTypeKey)
		priv->removePassiveKeyGrab (action->key ());

	    return false;
	}
    }

    if (action->edgeMask ())
    {
	int i;

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    if (action->edgeMask () & (1 << i))
		priv->enableEdge (i);
    }

    action->priv->active = true;

    return true;
}

void
CompScreen::removeAction (CompAction *action)
{
    if (!priv->initialized)
	return;

    if (!action->active ())
	return;

    if (action->type () & CompAction::BindingTypeKey)
	priv->removePassiveKeyGrab (action->key ());

    if (action->type () & CompAction::BindingTypeButton)
	priv->removePassiveButtonGrab (action->button ());

    if (action->edgeMask ())
    {
	int i;

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    if (action->edgeMask () & (1 << i))
		priv->disableEdge (i);
    }

    action->priv->active = false;
}

CompRect
PrivateScreen::computeWorkareaForBox (const CompRect& box)
{
    CompRegion region;
    int        x1, y1, x2, y2;

    region += box;

    foreach (CompWindow *w, windows)
    {
	if (!w->isMapped ())
	    continue;

	if (w->struts ())
	{
	    x1 = w->struts ()->left.x;
	    y1 = w->struts ()->left.y;
	    x2 = x1 + w->struts ()->left.width;
	    y2 = y1 + w->struts ()->left.height;

	    if (y1 < box.y2 () && y2 > box.y1 ())
		region -= CompRect (x1, box.y1 (), x2 - x1, box.height ());

	    x1 = w->struts ()->right.x;
	    y1 = w->struts ()->right.y;
	    x2 = x1 + w->struts ()->right.width;
	    y2 = y1 + w->struts ()->right.height;

	    if (y1 < box.y2 () && y2 > box.y1 ())
		region -= CompRect (x1, box.y1 (), x2 - x1, box.height ());

	    x1 = w->struts ()->top.x;
	    y1 = w->struts ()->top.y;
	    x2 = x1 + w->struts ()->top.width;
	    y2 = y1 + w->struts ()->top.height;

	    if (x1 < box.x2 () && x2 > box.x1 ())
		region -= CompRect (box.x1 (), y1, box.width (), y2 - y1);

	    x1 = w->struts ()->bottom.x;
	    y1 = w->struts ()->bottom.y;
	    x2 = x1 + w->struts ()->bottom.width;
	    y2 = y1 + w->struts ()->bottom.height;

	    if (x1 < box.x2 () && x2 > box.x1 ())
		region -= CompRect (box.x1 (), y1, box.width (), y2 - y1);
	}
    }

    if (region.isEmpty ())
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Empty box after applying struts, ignoring struts");
	return box;
    }

    return region.boundingRect ();
}

void
CompScreen::updateWorkarea ()
{
    CompRect workArea;
    bool     workAreaChanged = false;

    for (unsigned int i = 0; i < priv->outputDevs.size (); i++)
    {
	CompRect oldWorkArea = priv->outputDevs[i].workArea ();

	workArea = priv->computeWorkareaForBox (priv->outputDevs[i]);

	if (workArea != oldWorkArea)
	{
	    workAreaChanged = true;
	    priv->outputDevs[i].setWorkArea (workArea);
	}
    }

    workArea = priv->computeWorkareaForBox (CompRect (0, 0,
						      screen->width (),
						      screen->height ()));

    if (priv->workArea != workArea)
    {
	workAreaChanged = true;
	priv->workArea = workArea;

	priv->setDesktopHints ();
    }

    if (workAreaChanged)
    {
	/* as work area changed, update all maximized windows on this
	   screen to snap to the new work area */
	foreach (CompWindow *w, priv->windows)
	    w->priv->updateSize ();
    }
}

static bool
isClientListWindow (CompWindow *w)
{
    /* windows with client id less than 2 have been destroyed and only exists
       because some plugin keeps a reference to them. they should not be in
       client lists */
    if (w->id () < 2)
	return false;

    if (w->overrideRedirect ())
	return false;

    if (!w->isViewable ())
    {
	if (!(w->state () & CompWindowStateHiddenMask))
	    return false;
    }

    return true;
}

static void
countClientListWindow (CompWindow *w,
		       int        *n)
{
    if (isClientListWindow (w))
    {
	*n = *n + 1;
    }
}

static bool
compareMappingOrder (const CompWindow *w1,
		     const CompWindow *w2)
{
    return w1->mapNum () < w2->mapNum ();
}

void
PrivateScreen::updateClientList ()
{
    bool   updateClientList = false;
    bool   updateClientListStacking = false;
    int	   n = 0;

    screen->forEachWindow (boost::bind (countClientListWindow, _1, &n));

    if (n == 0)
    {
	if ((unsigned int) n != priv->clientList.size ())
	{
	    priv->clientList.clear ();
	    priv->clientListStacking.clear ();
	    priv->clientIdList.clear ();
	    priv->clientIdListStacking.clear ();

	    XChangeProperty (priv->dpy, priv->root,
			     Atoms::clientList,
			     XA_WINDOW, 32, PropModeReplace,
			     (unsigned char *) &priv->grabWindow, 1);
	    XChangeProperty (priv->dpy, priv->root,
			     Atoms::clientListStacking,
			     XA_WINDOW, 32, PropModeReplace,
			     (unsigned char *) &priv->grabWindow, 1);
	}

	return;
    }

    if ((unsigned int) n != priv->clientList.size ())
    {
	priv->clientIdList.resize (n);
	priv->clientIdListStacking.resize (n);

	updateClientList = updateClientListStacking = true;
    }

    priv->clientListStacking.clear ();

    foreach (CompWindow *w, priv->windows)
	if (isClientListWindow (w))
	    priv->clientListStacking.push_back (w);

    /* clear clientList and copy clientListStacking into clientList */
    priv->clientList = priv->clientListStacking;

    /* sort clientList in mapping order */
    sort (priv->clientList.begin (), priv->clientList.end (),
	  compareMappingOrder);

    /* make sure client id lists are up-to-date */
    for (int i = 0; i < n; i++)
    {
	if (!updateClientList &&
	    priv->clientIdList[i] != priv->clientList[i]->id ())
	{
	    updateClientList = true;
	}

	priv->clientIdList[i] = priv->clientList[i]->id ();
    }
    for (int i = 0; i < n; i++)
    {
	if (!updateClientListStacking &&
	    priv->clientIdListStacking[i] != priv->clientListStacking[i]->id ())
	{
	    updateClientListStacking = true;
	}

	priv->clientIdListStacking[i] = priv->clientListStacking[i]->id ();
    }

    if (updateClientList)
	XChangeProperty (priv->dpy, priv->root,
			 Atoms::clientList,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) &priv->clientIdList.at (0), n);

    if (updateClientListStacking)
	XChangeProperty (priv->dpy, priv->root,
			 Atoms::clientListStacking,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) &priv->clientIdListStacking.at (0),
			 n);
}

const CompWindowVector &
CompScreen::clientList (bool stackingOrder)
{
   return stackingOrder ? priv->clientListStacking : priv->clientList;
}

void
CompScreen::toolkitAction (Atom   toolkitAction,
			   Time   eventTime,
			   Window window,
			   long   data0,
			   long   data1,
			   long   data2)
{
    XEvent ev;

    ev.type		    = ClientMessage;
    ev.xclient.window	    = window;
    ev.xclient.message_type = Atoms::toolkitAction;
    ev.xclient.format	    = 32;
    ev.xclient.data.l[0]    = toolkitAction;
    ev.xclient.data.l[1]    = eventTime;
    ev.xclient.data.l[2]    = data0;
    ev.xclient.data.l[3]    = data1;
    ev.xclient.data.l[4]    = data2;

    XUngrabPointer (priv->dpy, CurrentTime);
    XUngrabKeyboard (priv->dpy, CurrentTime);

    XSendEvent (priv->dpy, priv->root, false,
		StructureNotifyMask, &ev);
}

void
CompScreen::runCommand (CompString command)
{
    if (command.size () == 0)
	return;

    if (fork () == 0)
    {
	size_t     pos;
	CompString env (priv->displayString);

	setsid ();

	pos = env.find (':');
	if (pos != std::string::npos)
	{
	    if (env.find ('.', pos) != std::string::npos)
	    {
		env.erase (env.find ('.', pos));
	    }
	    else
	    {
		env.erase (pos);
		env.append (":0");
	    }
	}

	env.append (compPrintf (".%d", priv->screenNum));

	putenv (const_cast<char *> (env.c_str ()));

	exit (execl ("/bin/sh", "/bin/sh", "-c", command.c_str (), NULL));
    }
}

void
CompScreen::moveViewport (int tx, int ty, bool sync)
{
    CompPoint pnt;

    tx = priv->vp.x () - tx;
    tx = MOD (tx, priv->vpSize.width ());
    tx -= priv->vp.x ();

    ty = priv->vp.y () - ty;
    ty = MOD (ty, priv->vpSize.height ());
    ty -= priv->vp.y ();

    if (!tx && !ty)
	return;

    priv->vp.setX (priv->vp.x () + tx);
    priv->vp.setY (priv->vp.y () + ty);

    tx *= -width ();
    ty *= -height ();

    foreach (CompWindow *w, priv->windows)
    {
	if (w->onAllViewports ())
	    continue;

	pnt = w->getMovementForOffset (CompPoint (tx, ty));

	if (w->saveMask () & CWX)
	    w->saveWc ().x += pnt.x ();

	if (w->saveMask () & CWY)
	    w->saveWc ().y += pnt.y ();

	/* move */
	w->move (pnt.x (), pnt.y ());

	if (sync)
	    w->syncPosition ();
    }

    if (sync)
    {
	CompWindow *w;

	priv->setDesktopHints ();

	priv->setCurrentActiveWindowHistory (priv->vp.x (), priv->vp.y ());

	w = findWindow (priv->activeWindow);
	if (w)
	{
	    CompPoint dvp;

	    dvp = w->defaultViewport ();

	    /* add window to current history if it's default viewport is
	       still the current one. */
	    if (priv->vp.x () == dvp.x () && priv->vp.y () == dvp.y ())
		priv->addToCurrentActiveWindowHistory (w->id ());
	}
    }
}

CompGroup *
PrivateScreen::addGroup (Window id)
{
    CompGroup *group = new CompGroup ();

    group->refCnt = 1;
    group->id     = id;

    priv->groups.push_back (group);

    return group;
}

void
PrivateScreen::removeGroup (CompGroup *group)
{
    group->refCnt--;
    if (group->refCnt)
	return;

    std::list<CompGroup *>::iterator it =
	std::find (priv->groups.begin (), priv->groups.end (), group);

    if (it != priv->groups.end ())
    {
	priv->groups.erase (it);
    }

    delete group;
}

CompGroup *
PrivateScreen::findGroup (Window id)
{
    foreach (CompGroup *g, priv->groups)
	if (g->id == id)
	    return g;

    return NULL;
}

void
PrivateScreen::applyStartupProperties (CompWindow *window)
{
    CompStartupSequence *s = NULL;
    const char	        *startupId = window->startupId ();

    if (!startupId)
    {
	CompWindow *leader;

	leader = screen->findWindow (window->clientLeader ());
	if (leader)
	    startupId = leader->startupId ();

	if (!startupId)
	    return;
    }

    foreach (CompStartupSequence *ss, priv->startupSequences)
    {
	const char *id;

	id = sn_startup_sequence_get_id (ss->sequence);
	if (strcmp (id, startupId) == 0)
	{
	    s = ss;
	    break;
	}
    }

    if (s)
	window->priv->applyStartupProperties (s);
}

void
CompScreen::sendWindowActivationRequest (Window id)
{
    XEvent xev;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = priv->dpy;
    xev.xclient.format  = 32;

    xev.xclient.message_type = Atoms::winActive;
    xev.xclient.window	     = id;

    xev.xclient.data.l[0] = ClientTypePager;
    xev.xclient.data.l[1] = 0;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent (priv->dpy, priv->root, false,
		SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

void
PrivateScreen::enableEdge (int edge)
{
    priv->screenEdge[edge].count++;
    if (priv->screenEdge[edge].count == 1)
	XMapRaised (priv->dpy, priv->screenEdge[edge].id);
}

void
PrivateScreen::disableEdge (int edge)
{
    priv->screenEdge[edge].count--;
    if (priv->screenEdge[edge].count == 0)
	XUnmapWindow (priv->dpy, priv->screenEdge[edge].id);
}

Window
PrivateScreen::getTopWindow ()
{
    /* return first window that has not been destroyed */
    for (CompWindowList::reverse_iterator rit = priv->windows.rbegin ();
	     rit != priv->windows.rend (); rit++)
    {
	if ((*rit)->id () > 1)
	    return (*rit)->id ();
    }

    return None;
}

int
CompScreen::outputDeviceForPoint (const CompPoint &point)
{
    return outputDeviceForPoint (point.x (), point.y ());
}

int
CompScreen::outputDeviceForPoint (int x, int y)
{
    CompWindow::Geometry geom (x, y, 1, 1, 0);

    return outputDeviceForGeometry (geom);
}

CompRect
CompScreen::getCurrentOutputExtents ()
{
    return priv->outputDevs[priv->currentOutputDev];
}

void
PrivateScreen::setNumberOfDesktops (unsigned int nDesktop)
{
    if (nDesktop < 1 || nDesktop >= 0xffffffff)
	return;

    if (nDesktop == priv->nDesktop)
	return;

    if (priv->currentDesktop >= nDesktop)
	priv->currentDesktop = nDesktop - 1;

    foreach (CompWindow *w, priv->windows)
    {
	if (w->desktop () == 0xffffffff)
	    continue;

	if (w->desktop () >= nDesktop)
	    w->setDesktop (nDesktop - 1);
    }

    priv->nDesktop = nDesktop;

    priv->setDesktopHints ();
}

void
PrivateScreen::setCurrentDesktop (unsigned int desktop)
{
    unsigned long data;

    if (desktop >= priv->nDesktop)
	return;

    if (desktop == priv->currentDesktop)
	return;

    priv->currentDesktop = desktop;

    foreach (CompWindow *w, priv->windows)
    {
	if (w->desktop () == 0xffffffff)
	    continue;

	if (w->desktop () == desktop)
	    w->priv->show ();
	else
	    w->priv->hide ();
    }

    data = desktop;

    XChangeProperty (priv->dpy, priv->root, Atoms::currentDesktop,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

const CompRect&
CompScreen::getWorkareaForOutput (unsigned int outputNum) const
{
    return priv->outputDevs[outputNum].workArea ();
}

void
CompScreen::outputChangeNotify ()
    WRAPABLE_HND_FUNC (16, outputChangeNotify)



/* Returns default viewport for some window geometry. If the window spans
   more than one viewport the most appropriate viewport is returned. How the
   most appropriate viewport is computed can be made optional if necessary. It
   is currently computed as the viewport where the center of the window is
   located. */
void
CompScreen::viewportForGeometry (const CompWindow::Geometry& gm,
				 CompPoint&                  viewport)
{
    CompRect rect (gm);
    int      offset;

    rect.setWidth  (rect.width () + (gm.border () * 2));
    rect.setHeight (rect.height () + (gm.border () * 2));

    offset = rect.centerX () < 0 ? -1 : 0;
    viewport.setX (priv->vp.x () + ((rect.centerX () / width ()) + offset) %
		   priv->vpSize.width ());

    offset = rect.centerY () < 0 ? -1 : 0;
    viewport.setY (priv->vp.y () + ((rect.centerY () / height ()) + offset ) %
		   priv->vpSize.height ());
}

int
CompScreen::outputDeviceForGeometry (const CompWindow::Geometry& gm)
{
    int          overlapAreas[priv->outputDevs.size ()];
    int          highest, seen, highestScore;
    int          x, y, strategy;
    unsigned int i;
    CompRect     geomRect;

    if (priv->outputDevs.size () == 1)
	return 0;

    strategy = priv->optionGetOverlappingOutputs ();

    if (strategy == CoreOptions::OverlappingOutputsSmartMode)
    {
	int centerX, centerY;

	/* for smart mode, calculate the overlap of the whole rectangle
	   with the output device rectangle */
	geomRect.setWidth (gm.width () + 2 * gm.border ());
	geomRect.setHeight (gm.height () + 2 * gm.border ());

	x = gm.x () % width ();
	centerX = (x + (geomRect.width () / 2));
	if (centerX < 0)
	    x += width ();
	else if (centerX > width ())
	    x -= width ();
	geomRect.setX (x);

	y = gm.y () % height ();
	centerY = (y + (geomRect.height () / 2));
	if (centerY < 0)
	    y += height ();
	else if (centerY > height ())
	    y -= height ();
	geomRect.setY (y);
    }
    else
    {
	/* for biggest/smallest modes, only use the window center to determine
	   the correct output device */
	x = (gm.x () + (gm.width () / 2) + gm.border ()) % width ();
	if (x < 0)
	    x += width ();
	y = (gm.y () + (gm.height () / 2) + gm.border ()) % height ();
	if (y < 0)
	    y += height ();

	geomRect.setGeometry (x, y, 1, 1);
    }

    /* get amount of overlap on all output devices */
    for (i = 0; i < priv->outputDevs.size (); i++)
    {
	CompRect overlap = priv->outputDevs[i] & geomRect;
	overlapAreas[i] = overlap.area ();
    }

    /* find output with largest overlap */
    for (i = 0, highest = 0, highestScore = 0;
	 i < priv->outputDevs.size (); i++)
    {
	if (overlapAreas[i] > highestScore)
	{
	    highest = i;
	    highestScore = overlapAreas[i];
	}
    }

    /* look if the highest score is unique */
    for (i = 0, seen = 0; i < priv->outputDevs.size (); i++)
	if (overlapAreas[i] == highestScore)
	    seen++;

    if (seen > 1)
    {
	/* it's not unique, select one output of the matching ones and use the
	   user preferred strategy for that */
	unsigned int currentSize, bestOutputSize;
	bool         searchLargest;

	searchLargest =
	    (strategy != CoreOptions::OverlappingOutputsPreferSmallerOutput);

	if (searchLargest)
	    bestOutputSize = 0;
	else
	    bestOutputSize = UINT_MAX;

	for (i = 0, highest = 0; i < priv->outputDevs.size (); i++)
	    if (overlapAreas[i] == highestScore)
	    {
		bool bestFit;

		currentSize = priv->outputDevs[i].area ();

		if (searchLargest)
		    bestFit = (currentSize > bestOutputSize);
		else
		    bestFit = (currentSize < bestOutputSize);

		if (bestFit)
		{
		    highest = i;
		    bestOutputSize = currentSize;
		}
	    }
    }

    return highest;
}

CompIcon *
CompScreen::defaultIcon () const
{
    return priv->defaultIcon;
}

bool
CompScreen::updateDefaultIcon ()
{
    CompString file = priv->optionGetDefaultIcon ();
    CompString pname = "";
    void       *data;
    CompSize   size;

    if (priv->defaultIcon)
    {
	delete priv->defaultIcon;
	priv->defaultIcon = NULL;
    }

    if (!readImageFromFile (file, pname, size, data))
	return false;

    priv->defaultIcon = new CompIcon (screen, size.width (), size.height ());

    memcpy (priv->defaultIcon->data (), data,
	    size.width () * size.height () * sizeof (CARD32));

    free (data);

    return true;
}

void
PrivateScreen::setCurrentActiveWindowHistory (int x, int y)
{
    int	i, min = 0;

    for (i = 0; i < ACTIVE_WINDOW_HISTORY_NUM; i++)
    {
	if (priv->history[i].x == x && priv->history[i].y == y)
	{
	    priv->currentHistory = i;
	    return;
	}
    }

    for (i = 1; i < ACTIVE_WINDOW_HISTORY_NUM; i++)
	if (priv->history[i].activeNum < priv->history[min].activeNum)
	    min = i;

    priv->currentHistory = min;

    priv->history[min].activeNum = priv->activeNum;
    priv->history[min].x         = x;
    priv->history[min].y         = y;

    memset (priv->history[min].id, 0, sizeof (priv->history[min].id));
}

void
PrivateScreen::addToCurrentActiveWindowHistory (Window id)
{
    CompActiveWindowHistory *history = &priv->history[priv->currentHistory];
    Window		    tmp, next = id;
    int			    i;

    /* walk and move history */
    for (i = 0; i < ACTIVE_WINDOW_HISTORY_SIZE; i++)
    {
	tmp = history->id[i];
	history->id[i] = next;
	next = tmp;

	/* we're done when we find an old instance or an empty slot */
	if (tmp == id || tmp == None)
	    break;
    }

    history->activeNum = priv->activeNum;
}

void
ScreenInterface::enterShowDesktopMode ()
    WRAPABLE_DEF (enterShowDesktopMode)

void
ScreenInterface::leaveShowDesktopMode (CompWindow *window)
    WRAPABLE_DEF (leaveShowDesktopMode, window)

void
ScreenInterface::outputChangeNotify ()
    WRAPABLE_DEF (outputChangeNotify)

void
ScreenInterface::addSupportedAtoms (std::vector<Atom>& atoms)
    WRAPABLE_DEF (addSupportedAtoms, atoms)


Window
CompScreen::root ()
{
    return priv->root;
}

int
CompScreen::xkbEvent ()
{
    return priv->xkbEvent;
}

void
CompScreen::warpPointer (int dx,
			 int dy)
{
    XEvent      event;

    pointerX += dx;
    pointerY += dy;

    if (pointerX >= width ())
	pointerX = width () - 1;
    else if (pointerX < 0)
	pointerX = 0;

    if (pointerY >= height ())
	pointerY = height () - 1;
    else if (pointerY < 0)
	pointerY = 0;

    XWarpPointer (priv->dpy,
		  None, priv->root,
		  0, 0, 0, 0,
		  pointerX, pointerY);

    XSync (priv->dpy, false);

    while (XCheckMaskEvent (priv->dpy,
			    LeaveWindowMask |
			    EnterWindowMask |
			    PointerMotionMask,
			    &event));

    if (!inHandleEvent)
    {
	lastPointerX = pointerX;
	lastPointerY = pointerY;
    }
}

CompWindowList &
CompScreen::windows ()
{
    return priv->windows;
}

Time
CompScreen::getCurrentTime ()
{
    XEvent event;

    XChangeProperty (priv->dpy, priv->grabWindow,
		     XA_PRIMARY, XA_STRING, 8,
		     PropModeAppend, NULL, 0);
    XWindowEvent (priv->dpy, priv->grabWindow,
		  PropertyChangeMask,
		  &event);

    return event.xproperty.time;
}

Window
CompScreen::selectionWindow ()
{
    return priv->wmSnSelectionWindow;
}

int
CompScreen::screenNum ()
{
    return priv->screenNum;
}

CompPoint
CompScreen::vp ()
{
    return priv->vp;
}

CompSize
CompScreen::vpSize ()
{
    return priv->vpSize;
}

int
CompScreen::desktopWindowCount ()
{
    return priv->desktopWindowCount;
}

unsigned int
CompScreen::activeNum () const
{
    return priv->activeNum;
}

CompOutput::vector &
CompScreen::outputDevs ()
{
    return priv->outputDevs;
}

CompOutput &
CompScreen::currentOutputDev () const
{
    return priv->outputDevs [priv->currentOutputDev];
}

const CompRect &
CompScreen::workArea () const
{
    return priv->workArea;
}

unsigned int
CompScreen::currentDesktop ()
{
    return priv->currentDesktop;
}

unsigned int
CompScreen::nDesktop ()
{
    return priv->nDesktop;
}

CompActiveWindowHistory *
CompScreen::currentHistory ()
{
    return &priv->history[priv->currentHistory];
}

bool
CompScreen::shouldSerializePlugins ()
{
    return priv->optionGetDoSerialize ();
}

void
PrivateScreen::removeDestroyed ()
{
    while (pendingDestroys)
    {
	foreach (CompWindow *w, windows)
	{
	    if (w->destroyed ())
	    {
		delete w;
		break;
	    }
	}

	pendingDestroys--;
    }
}

const CompRegion &
CompScreen::region () const
{
    return priv->region;
}

bool
CompScreen::hasOverlappingOutputs ()
{
    return priv->hasOverlappingOutputs;
}

CompOutput &
CompScreen::fullscreenOutput ()
{
    return priv->fullscreenOutput;
}


XWindowAttributes
CompScreen::attrib ()
{
    return priv->attrib;
}

std::vector<XineramaScreenInfo> &
CompScreen::screenInfo ()
{
    return priv->screenInfo;
}

bool
PrivateScreen::createFailed ()
{
    return !screenInitalized;
}

CompScreen::CompScreen ():
    PluginClassStorage (screenPluginClassIndices),
    priv (NULL)
{
    CompPrivate p;
    CompOption::Value::Vector vList;
    CompPlugin  *corePlugin;

    priv = new PrivateScreen (this);
    assert (priv);

    screenInitalized = true;

    corePlugin = CompPlugin::load ("core");
    if (!corePlugin)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't load core plugin");
	screenInitalized = false;
    }

    if (!CompPlugin::push (corePlugin))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't activate core plugin");
	screenInitalized = false;
    }

    p.uval = CORE_ABIVERSION;
    storeValue ("core_ABI", p);

    vList.push_back ("core");

    priv->plugin.set (CompOption::TypeString, vList);
}

bool
CompScreen::init (const char *name)
{
    Window               focus;
    int                  revertTo, i;
    int                  xkbOpcode;
    Display              *dpy;
    Window               root;
    Window               newWmSnOwner = None;
    Atom                 wmSnAtom = 0;
    Time                 wmSnTimestamp = 0;
    XEvent               event;
    XSetWindowAttributes attr;
    Window               currentWmSnOwner;
    char                 buf[128];
    static char          data = 0;
    XColor               black;
    Pixmap               bitmap;
    XVisualInfo          templ;
    XVisualInfo          *visinfo;
    Window               rootReturn, parentReturn;
    Window               *children;
    unsigned int         nchildren;
    int                  nvisinfo;
    XSetWindowAttributes attrib;

    dpy = priv->dpy = XOpenDisplay (name);
    if (!priv->dpy)
    {
	compLogMessage ("core", CompLogLevelFatal,
		        "Couldn't open display %s", XDisplayName (name));
	return false;
    }

//    priv->connection = XGetXCBConnection (priv->dpy);

    snprintf (priv->displayString, 255, "DISPLAY=%s",
	      DisplayString (dpy));

#ifdef DEBUG
    XSynchronize (priv->dpy, true);
#endif

    Atoms::init (priv->dpy);

    XSetErrorHandler (errorHandler);

    priv->snDisplay = sn_display_new (dpy, NULL, NULL);
    if (!priv->snDisplay)
	return true;

    priv->lastPing = 1;

    if (!XSyncQueryExtension (dpy, &priv->syncEvent, &priv->syncError))
    {
	compLogMessage ("core", CompLogLevelFatal,
		        "No sync extension");
	return false;
    }

    priv->randrExtension = XRRQueryExtension (dpy, &priv->randrEvent,
					      &priv->randrError);

    priv->shapeExtension = XShapeQueryExtension (dpy, &priv->shapeEvent,
						 &priv->shapeError);

    priv->xkbExtension = XkbQueryExtension (dpy, &xkbOpcode,
					    &priv->xkbEvent, &priv->xkbError,
					    NULL, NULL);
    if (priv->xkbExtension)
    {
	XkbSelectEvents (dpy, XkbUseCoreKbd,
			 XkbBellNotifyMask | XkbStateNotifyMask,
			 XkbAllEventsMask);
    }
    else
    {
	compLogMessage ("core", CompLogLevelFatal,
		        "No XKB extension");

	priv->xkbEvent = priv->xkbError = -1;
    }

    priv->xineramaExtension = XineramaQueryExtension (dpy,
						      &priv->xineramaEvent,
						      &priv->xineramaError);

    priv->updateScreenInfo ();

    priv->escapeKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Escape"));
    priv->returnKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Return"));

    sprintf (buf, "WM_S%d", DefaultScreen (dpy));
    wmSnAtom = XInternAtom (dpy, buf, 0);

    currentWmSnOwner = XGetSelectionOwner (dpy, wmSnAtom);

    if (currentWmSnOwner != None)
    {
	if (!replaceCurrentWm)
	{
	    compLogMessage ("core", CompLogLevelError,
			    "Screen %d on display \"%s\" already "
			    "has a window manager; try using the "
			    "--replace option to replace the current "
			    "window manager.",
			    DefaultScreen (dpy), DisplayString (dpy));

	    return false;
	}

	XSelectInput (dpy, currentWmSnOwner, StructureNotifyMask);
    }

    root = XRootWindow (dpy, DefaultScreen (dpy));

    attr.override_redirect = true;
    attr.event_mask        = PropertyChangeMask;

    newWmSnOwner =
	XCreateWindow (dpy, root, -100, -100, 1, 1, 0,
		       CopyFromParent, CopyFromParent,
		       CopyFromParent,
		       CWOverrideRedirect | CWEventMask,
		       &attr);

    XChangeProperty (dpy, newWmSnOwner, Atoms::wmName, Atoms::utf8String, 8,
		     PropModeReplace, (unsigned char *) PACKAGE,
		     strlen (PACKAGE));

    XWindowEvent (dpy, newWmSnOwner, PropertyChangeMask, &event);

    wmSnTimestamp = event.xproperty.time;

    XSetSelectionOwner (dpy, wmSnAtom, newWmSnOwner, wmSnTimestamp);

    if (XGetSelectionOwner (dpy, wmSnAtom) != newWmSnOwner)
    {
	compLogMessage ("core", CompLogLevelError,
			"Could not acquire window manager "
			"selection on screen %d display \"%s\"",
			DefaultScreen (dpy), DisplayString (dpy));

	XDestroyWindow (dpy, newWmSnOwner);

	return false;
    }

    /* Send client message indicating that we are now the window manager */
    event.xclient.type         = ClientMessage;
    event.xclient.window       = root;
    event.xclient.message_type = Atoms::manager;
    event.xclient.format       = 32;
    event.xclient.data.l[0]    = wmSnTimestamp;
    event.xclient.data.l[1]    = wmSnAtom;
    event.xclient.data.l[2]    = 0;
    event.xclient.data.l[3]    = 0;
    event.xclient.data.l[4]    = 0;

    XSendEvent (dpy, root, FALSE, StructureNotifyMask, &event);

    /* Wait for old window manager to go away */
    if (currentWmSnOwner != None)
    {
	do {
	    XWindowEvent (dpy, currentWmSnOwner, StructureNotifyMask, &event);
	} while (event.type != DestroyNotify);
    }

    modHandler->updateModifierMappings ();

    CompScreen::checkForError (dpy);

    XGrabServer (dpy);

    XSelectInput (dpy, root,
		  SubstructureRedirectMask |
		  SubstructureNotifyMask   |
		  StructureNotifyMask      |
		  PropertyChangeMask       |
		  LeaveWindowMask          |
		  EnterWindowMask          |
		  KeyPressMask             |
		  KeyReleaseMask           |
		  ButtonPressMask          |
		  ButtonReleaseMask        |
		  FocusChangeMask          |
		  ExposureMask);

    /* We need to register for EnterWindowMask |
     * ButtonPressMask | FocusChangeMask on other
     * root windows as well because focus happens
     * on a display level and we need to check
     * if the screen we are running on lost focus */

    for (int i = 0; i <= ScreenCount (dpy) - 1; i++)
    {
	Window rt = XRootWindow (dpy, i);

	if (rt == root)
	    continue;

	XSelectInput (dpy, rt,
		      FocusChangeMask |
		      SubstructureNotifyMask);
    }

    if (CompScreen::checkForError (dpy))
    {
	compLogMessage ("core", CompLogLevelError,
		        "Another window manager is "
		        "already running on screen: %d", DefaultScreen (dpy));

	XUngrabServer (dpy);
	return false;
    }

    /* We only care about windows we're not going to
     * get a CreateNotify for later, so query the tree
     * here, init plugin screens, and then init windows */

    XQueryTree (dpy, root,
		&rootReturn, &parentReturn,
		&children, &nchildren);

    for (i = 0; i < SCREEN_EDGE_NUM; i++)
    {
	priv->screenEdge[i].id    = None;
	priv->screenEdge[i].count = 0;
    }

    priv->screenNum = DefaultScreen (dpy);
    priv->colormap  = DefaultColormap (dpy, priv->screenNum);
    priv->root	    = root;

    priv->snContext = sn_monitor_context_new (priv->snDisplay, priv->screenNum,
					      compScreenSnEvent, this, NULL);

    priv->wmSnSelectionWindow = newWmSnOwner;
    priv->wmSnAtom            = wmSnAtom;
    priv->wmSnTimestamp       = wmSnTimestamp;

    if (!XGetWindowAttributes (dpy, priv->root, &priv->attrib))
	return false;

    priv->workArea.setWidth (priv->attrib.width);
    priv->workArea.setHeight (priv->attrib.height);

    priv->grabWindow = None;

    templ.visualid = XVisualIDFromVisual (priv->attrib.visual);

    visinfo = XGetVisualInfo (dpy, VisualIDMask, &templ, &nvisinfo);
    if (!nvisinfo)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't get visual info for default visual");
	return false;
    }

    black.red = black.green = black.blue = 0;

    if (!XAllocColor (dpy, priv->colormap, &black))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't allocate color");
	XFree (visinfo);
	return false;
    }

    bitmap = XCreateBitmapFromData (dpy, priv->root, &data, 1, 1);
    if (!bitmap)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't create bitmap");
	XFree (visinfo);
	return false;
    }

    priv->invisibleCursor = XCreatePixmapCursor (dpy, bitmap, bitmap,
						 &black, &black, 0, 0);
    if (!priv->invisibleCursor)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't create invisible cursor");
	XFree (visinfo);
	return false;
    }

    XFreePixmap (dpy, bitmap);
    XFreeColors (dpy, priv->colormap, &black.pixel, 1, 0);

    XFree (visinfo);

    priv->reshape (priv->attrib.width, priv->attrib.height);

    priv->detectOutputDevices ();
    priv->updateOutputDevices ();

    priv->getDesktopHints ();

    attrib.override_redirect = 1;
    attrib.event_mask	     = PropertyChangeMask;

    priv->grabWindow = XCreateWindow (dpy, priv->root, -100, -100, 1, 1, 0,
				      CopyFromParent, InputOnly, CopyFromParent,
				      CWOverrideRedirect | CWEventMask,
				      &attrib);
    XMapWindow (dpy, priv->grabWindow);

    for (i = 0; i < SCREEN_EDGE_NUM; i++)
    {
	long xdndVersion = 3;

	priv->screenEdge[i].id = XCreateWindow (dpy, priv->root,
						-100, -100, 1, 1, 0,
					        CopyFromParent, InputOnly,
					        CopyFromParent,
						CWOverrideRedirect,
					        &attrib);

	XChangeProperty (dpy, priv->screenEdge[i].id, Atoms::xdndAware,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) &xdndVersion, 1);

	XSelectInput (dpy, priv->screenEdge[i].id,
		      EnterWindowMask   |
		      LeaveWindowMask   |
		      ButtonPressMask   |
		      ButtonReleaseMask |
		      PointerMotionMask);
    }

    priv->updateScreenEdges ();

    priv->setDesktopHints ();
    priv->setSupportingWmCheck ();
    updateSupportedWmHints ();

    priv->normalCursor = XCreateFontCursor (dpy, XC_left_ptr);
    priv->busyCursor   = XCreateFontCursor (dpy, XC_watch);

    XDefineCursor (dpy, priv->root, priv->normalCursor);

    XUngrabServer (dpy);
    XSync (dpy, FALSE);

    priv->setAudibleBell (priv->optionGetAudibleBell ());

    priv->pingTimer.setTimes (priv->optionGetPingDelay (),
			      priv->optionGetPingDelay () + 500);

    priv->pingTimer.start ();

    priv->addScreenActions ();

    /* Need to set a default here so that the value isn't uninitialized
     * when loading plugins FIXME: Should find a way to initialize options
     * first and then set this value, or better yet, tie this value directly
     * to the option */
    priv->vpSize.setWidth (priv->optionGetHsize ());
    priv->vpSize.setHeight (priv->optionGetVsize ());

    priv->initialized = true;

    /* TODO: Bailout properly when screenInitPlugins fails
     * TODO: It would be nicer if this line could mean
     * "init all the screens", but unfortunately it only inits
     * plugins loaded on the command line screen's and then
     * we need to call updatePlugins () to init the remaining
     * screens from option changes */
    assert (CompPlugin::screenInitPlugins (this));

    /* The active plugins list might have been changed - load any
     * new plugins */

    priv->vpSize.setWidth (priv->optionGetHsize ());
    priv->vpSize.setHeight (priv->optionGetVsize ());

    if (priv->dirtyPluginList)
	priv->updatePlugins ();

    /* Start initializing windows here */

    for (unsigned int i = 0; i < nchildren; i++)
    {
	XWindowAttributes attrib;

	/* Failure means the window has been destroyed, but
	 * still add it to the window list anyways since we
	 * will soon handle the DestroyNotify event for it
	 * and in between CreateNotify time and DestroyNotify
	 * time there might be ConfigureRequests asking us
	 * to stack windows relative to it
	 */

	if (!XGetWindowAttributes (screen->dpy (), children[i], &attrib))
	    priv->setDefaultWindowAttributes (&attrib);

	CoreWindow *cw = new CoreWindow (children[i]);

	if (cw)
	{
	    cw->manage (i ? children[i - 1] : 0, attrib);
	    delete cw;
	}
    }

    i = 0;

    /* enforce restack on all windows */
    i = 0;
    for (CompWindowList::reverse_iterator rit = priv->windows.rbegin ();
	 rit != priv->windows.rend (); rit++)
	children[i++] = (*rit)->id ();

    XRestackWindows (dpy, children, i);

    XFree (children);

    foreach (CompWindow *w, priv->windows)
    {
	if (w->isViewable ())
	    w->priv->activeNum = priv->activeNum++;
    }

    XGetInputFocus (dpy, &focus, &revertTo);

    /* move input focus to root window so that we get a FocusIn event when
       moving it to the default window */
    XSetInputFocus (dpy, priv->root, RevertToPointerRoot, CurrentTime);

    if (focus == None || focus == PointerRoot)
    {
	focusDefaultWindow ();
    }
    else
    {
	CompWindow *w;

	w = findWindow (focus);
	if (w)
	    w->moveInputFocusTo ();
	else
	    focusDefaultWindow ();
    }

    return true;
}

CompScreen::~CompScreen ()
{
    CompPlugin  *p;

    priv->removeAllSequences ();

    while (!priv->windows.empty ())
	delete priv->windows.front ();

    while ((p = CompPlugin::pop ()))
	CompPlugin::unload (p);

    XUngrabKey (priv->dpy, AnyKey, AnyModifier, priv->root);

    priv->initialized = false;

    for (int i = 0; i < SCREEN_EDGE_NUM; i++)
	XDestroyWindow (priv->dpy, priv->screenEdge[i].id);

    XDestroyWindow (priv->dpy, priv->grabWindow);

    if (priv->defaultIcon)
	delete priv->defaultIcon;

    XFreeCursor (priv->dpy, priv->invisibleCursor);

    if (priv->desktopHintData)
	free (priv->desktopHintData);

    if (priv->snContext)
	sn_monitor_context_unref (priv->snContext);

    if (priv->snDisplay)
	sn_display_unref (priv->snDisplay);

    XSync (priv->dpy, False);
    XCloseDisplay (priv->dpy);

    delete priv;

    screen = NULL;
}

PrivateScreen::PrivateScreen (CompScreen *screen) :
    priv (this),
    fileWatch (0),
    lastFileWatchHandle (1),
    watchFds (0),
    lastWatchFdHandle (1),
    valueMap (),
    screenInfo (0),
    activeWindow (0),
    below (None),
    autoRaiseTimer (),
    autoRaiseWindow (0),
    edgeDelayTimer (),
    plugin (),
    dirtyPluginList (true),
    screen (screen),
    windows (),
    vp (0, 0),
    vpSize (1, 1),
    nDesktop (1),
    currentDesktop (0),
    root (None),
    grabWindow (None),
    desktopWindowCount (0),
    mapNum (1),
    activeNum (1),
    outputDevs (0),
    currentOutputDev (0),
    hasOverlappingOutputs (false),
    currentHistory (0),
    snContext (0),
    startupSequences (0),
    startupSequenceTimer (),
    groups (0),
    defaultIcon (0),
    buttonGrabs (0),
    keyGrabs (0),
    grabs (0),
    grabbed (false),
    pendingDestroys (0),
    showingDesktopMask (0),
    desktopHintData (0),
    desktopHintSize (0),
    initialized (false)
{
    gettimeofday (&lastTimeout, 0);

    pingTimer.setCallback (
	boost::bind (&PrivateScreen::handlePingTimeout, this));

    startupSequenceTimer.setCallback (
	boost::bind (&PrivateScreen::handleStartupSequenceTimeout, this));
    startupSequenceTimer.setTimes (1000, 1500);

    optionSetCloseWindowKeyInitiate (CompScreen::closeWin);
    optionSetCloseWindowButtonInitiate (CompScreen::closeWin);
    optionSetRaiseWindowKeyInitiate (CompScreen::raiseWin);
    optionSetRaiseWindowButtonInitiate (CompScreen::raiseWin);
    optionSetLowerWindowKeyInitiate (CompScreen::lowerWin);
    optionSetLowerWindowButtonInitiate (CompScreen::lowerWin);

    optionSetUnmaximizeWindowKeyInitiate (CompScreen::unmaximizeWin);

    optionSetMinimizeWindowKeyInitiate (CompScreen::minimizeWin);
    optionSetMinimizeWindowButtonInitiate (CompScreen::minimizeWin);
    optionSetMaximizeWindowKeyInitiate (CompScreen::maximizeWin);
    optionSetMaximizeWindowHorizontallyKeyInitiate (
	CompScreen::maximizeWinHorizontally);
    optionSetMaximizeWindowVerticallyKeyInitiate (
	CompScreen::maximizeWinVertically);

    optionSetWindowMenuKeyInitiate (CompScreen::windowMenu);
    optionSetWindowMenuButtonInitiate (CompScreen::windowMenu);

    optionSetShowDesktopKeyInitiate (CompScreen::showDesktop);
    optionSetShowDesktopEdgeInitiate (CompScreen::showDesktop);

    optionSetToggleWindowMaximizedKeyInitiate (CompScreen::toggleWinMaximized);
    optionSetToggleWindowMaximizedButtonInitiate (CompScreen::toggleWinMaximized);

    optionSetToggleWindowMaximizedHorizontallyKeyInitiate (
	CompScreen::toggleWinMaximizedHorizontally);
    optionSetToggleWindowMaximizedVerticallyKeyInitiate (
	CompScreen::toggleWinMaximizedVertically);

    optionSetToggleWindowShadedKeyInitiate (CompScreen::shadeWin);
}

PrivateScreen::~PrivateScreen ()
{
}
