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
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
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

#include <core/screen.h>
#include <core/icon.h>
#include <core/atoms.h>
#include "privatescreen.h"
#include "privatewindow.h"
#include "privateaction.h"
#include "privatestackdebugger.h"

CompOutput *targetOutput;

int lastPointerX = 0;
int lastPointerY = 0;
unsigned int lastPointerMods = 0;
int pointerX     = 0;
int pointerY     = 0;
unsigned int pointerMods = 0;

namespace
{
bool inHandleEvent = false;

bool screenInitalized = false;
}

#define MwmHintsFunctions   (1L << 0)
#define MwmHintsDecorations (1L << 1)
#define PropMotifWmHintElements 3

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
} MwmHints;

namespace cps = compiz::private_screen;



CompScreen *screen;
ModifierHandler *modHandler;

PluginClassStorage::Indices screenPluginClassIndices (0);

void CompScreenImpl::sizePluginClasses(unsigned int size)
{
    if(size != pluginClasses.size ())
 	pluginClasses.resize (size);
}

void CompScreenImpl::setWindowState (unsigned int state, Window id)
{
    priv->setWindowState (state, id);
}

void CompScreenImpl::addToDestroyedWindows(CompWindow * cw)
{
    priv->destroyedWindows.push_back (cw);
}

void CompScreenImpl::processEvents () { priv->processEvents (); }

unsigned int
CompScreen::allocPluginClassIndex ()
{
    unsigned int i = PluginClassStorage::allocatePluginClassIndex (screenPluginClassIndices);

    sizePluginClasses(screenPluginClassIndices.size());

    return i;
}

void
CompScreen::freePluginClassIndex (unsigned int index)
{
    PluginClassStorage::freePluginClassIndex (screenPluginClassIndices, index);

    sizePluginClasses(screenPluginClassIndices.size());
}

void
cps::EventManager::handleSignal (int signum)
{
    switch (signum)
    {
	case SIGINT:
	case SIGTERM:
	    mainloop->quit ();
	    break;
	case SIGHUP:
	    restartSignal = true;
	    mainloop->quit ();
	    break;
	default:
	    break;
    }

    mainloop->quit ();
}

void
CompScreenImpl::eventLoop ()
{
    priv->startEventLoop ();
}

void
cps::EventManager::startEventLoop()
{
    source = CompEventSource::create ();
    timeout = CompTimeoutSource::create (ctx);

    source->attach (ctx);

    XFlush (screen->dpy());

    mainloop->run();
}

CompFileWatchHandle
CompScreenImpl::addFileWatch (const char        *path,
			  int               mask,
			  FileWatchCallBack callBack)
{
    CompFileWatch *fileWatch = priv->addFileWatch (path, mask, callBack);

    if (!fileWatch)
	return 0;

    fileWatchAdded (fileWatch);

    return fileWatch->handle;
}

CompFileWatch*
cps::EventManager::addFileWatch (
    const char        *path,
    int               mask,
    FileWatchCallBack callBack)
{
    CompFileWatch *fw = new CompFileWatch ();
    if (!fw)
	return 0;

    fw->path	= path;
    fw->mask	= mask;
    fw->callBack = callBack;
    fw->handle   = lastFileWatchHandle++;

    if (lastFileWatchHandle == MAXSHORT)
	lastFileWatchHandle = 1;

    fileWatch.push_front (fw);

    return fw;
}

void
CompScreenImpl::removeFileWatch (CompFileWatchHandle handle)
{
    if (CompFileWatch* w = priv->removeFileWatch (handle))
    {
	fileWatchRemoved (w);

	delete w;
    }
}

CompFileWatch*
cps::EventManager::removeFileWatch (CompFileWatchHandle handle)
{
    std::list<CompFileWatch *>::iterator it;

    for (it = fileWatch.begin (); it != fileWatch.end (); it++)
	if ((*it)->handle == handle)
	    break;

    if (it == fileWatch.end ())
	return 0;

    CompFileWatch* w = (*it);
    fileWatch.erase (it);

    return w;
}

const CompFileWatchList &
CompScreenImpl::getFileWatches () const
{
    return priv->getFileWatches ();
}

const CompFileWatchList &
cps::EventManager::getFileWatches () const
{
    return fileWatch;
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

CompWatchFd::~CompWatchFd ()
{
}

CompWatchFd *
CompWatchFd::create (int               fd,
		     Glib::IOCondition events,
		     FdWatchCallBack   callback)
{
    return new CompWatchFd (fd, events, callback);
}

CompWatchFdHandle
CompScreenImpl::addWatchFd (int             fd,
			short int       events,
			FdWatchCallBack callBack)
{
    return priv->addWatchFd (fd, events, callBack);
}

CompWatchFdHandle
cps::EventManager::addWatchFd (int             fd,
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

    CompWatchFd *watchFd = CompWatchFd::create (fd, gEvents, callBack);

    watchFd->attach (ctx);

    if (!watchFd)
	return 0;
    watchFd->mHandle   = lastWatchFdHandle++;

    if (lastWatchFdHandle == MAXSHORT)
	lastWatchFdHandle = 1;

    watchFds.push_front (watchFd);

    return watchFd->mHandle;
}

void
CompScreenImpl::removeWatchFd (CompWatchFdHandle handle)
{
    priv->removeWatchFd (handle);
}

void
cps::EventManager::removeWatchFd (CompWatchFdHandle handle)
{
    std::list<CompWatchFd * >::iterator it;
    CompWatchFd *			w;

    for (it = watchFds.begin();
	 it != watchFds.end (); it++)
    {
	if ((*it)->mHandle == handle)
	    break;
    }

    if (it == watchFds.end ())
	return;

    w = (*it);

    if (w->mExecuting)
    {
	w->mForceFail = true;
	return;
    }

    delete w;
    watchFds.erase (it);
}

void
CompScreenImpl::storeValue (CompString key, CompPrivate value)
{
    ValueHolder::Default ()->storeValue (key, value);
}

bool
CompScreenImpl::hasValue (CompString key)
{
    return ValueHolder::Default ()->hasValue (key);
}

CompPrivate
CompScreenImpl::getValue (CompString key)
{
    return ValueHolder::Default ()->getValue (key);
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
CompScreenImpl::eraseValue (CompString key)
{
    ValueHolder::Default ()->eraseValue (key);
}

void
CompScreen::fileWatchAdded (CompFileWatch *watch)
{
    WRAPABLE_HND_FUNCTN (fileWatchAdded, watch);
    _fileWatchAdded (watch);
}

void
CompScreenImpl::_fileWatchAdded (CompFileWatch *watch)
{
}

void
CompScreen::fileWatchRemoved (CompFileWatch *watch)
{
    WRAPABLE_HND_FUNCTN (fileWatchRemoved, watch);
    _fileWatchRemoved (watch);
}

void
CompScreenImpl::_fileWatchRemoved (CompFileWatch *watch)
{
}

bool
CompScreen::setOptionForPlugin (const char        *plugin,
				const char        *name,
				CompOption::Value &value)
{
    WRAPABLE_HND_FUNCTN_RETURN (bool, setOptionForPlugin,
			      plugin, name, value)

    return _setOptionForPlugin(plugin, name, value);
}

bool
CompScreenImpl::_setOptionForPlugin (const char        *plugin,
				const char        *name,
				CompOption::Value &value)
{
    CompPlugin *p = CompPlugin::find (plugin);
    if (p)
	return p->vTable->setOption (name, value);

    return false;
}

void
CompScreen::sessionEvent (CompSession::Event event,
			  CompOption::Vector &arguments)
{
    WRAPABLE_HND_FUNCTN (sessionEvent, event, arguments);
    _sessionEvent(event, arguments);
}

void
CompScreenImpl::_sessionEvent (CompSession::Event event,
			  CompOption::Vector &arguments)
{
}

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
CompScreenImpl::dpy ()
{
    return priv->dpy;
}

bool
CompScreenImpl::XRandr ()
{
    return priv->randrExtension;
}

int
CompScreenImpl::randrEvent ()
{
    return priv->randrEvent;
}

bool
CompScreenImpl::XShape ()
{
    return priv->shapeExtension;
}

int
CompScreenImpl::shapeEvent ()
{
    return priv->shapeEvent;
}

int
CompScreenImpl::syncEvent ()
{
    return priv->syncEvent;
}


SnDisplay *
CompScreenImpl::snDisplay ()
{
    return priv->snDisplay;
}

Window
CompScreenImpl::activeWindow ()
{
    return priv->activeWindow;
}

Window
CompScreenImpl::autoRaiseWindow ()
{
    return priv->autoRaiseWindow;
}

const char *
CompScreenImpl::displayString ()
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
CompScreenImpl::getOptions ()
{
    return priv->getOptions ();
}

bool
CompScreenImpl::setOption (const CompString  &name,
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
	    setDirtyPluginList ();
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
	    if (optionGetDetectOutputs ())
		return false;
	    updateOutputDevices ();
	    break;
	default:
	    break;
    }

    return rv;
}

std::list <XEvent>
PrivateScreen::queueEvents ()
{
    std::list <XEvent> events;

    while (XPending (dpy))
    {
	XEvent event, peekEvent;
	XNextEvent (dpy, &event);

	/* Skip to the last MotionNotify
	 * event in this sequence */
	if (event.type == MotionNotify)
	{
	    while (XPending (dpy))
	    {
		XPeekEvent (dpy, &peekEvent);

		if (peekEvent.type != MotionNotify)
		    break;

		XNextEvent (dpy, &event);
	    }
	}

	events.push_back (event);
    }

    return events;
}

void
PrivateScreen::processEvents ()
{
    std::list <XEvent> events;
    StackDebugger *dbg = StackDebugger::Default ();

    if (isDirtyPluginList ())
	updatePlugins ();

    /* Restacks recently processed, ensure that
     * plugins use the stack last received from
     * the server */
    if (stackIsFresh)
    {
	serverWindows.clear ();

	foreach (CompWindow *sw, windows)
	{
	    sw->serverPrev = sw->prev;
	    sw->serverNext = sw->next;
	    serverWindows.push_back (sw);
	}
    }

    if (dbg)
    {
	dbg->windowsChanged (false);
	dbg->serverWindowsChanged (false);
	events = dbg->loadStack (serverWindows);
    }
    else
	events = queueEvents ();

    stackIsFresh = false;

    foreach (XEvent &event, events)
    {
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
	    pointerMods = event.xkey.state;
	    break;
	case MotionNotify:

	    pointerX = event.xmotion.x_root;
	    pointerY = event.xmotion.y_root;
	    pointerMods = event.xmotion.state;
	    break;
	case EnterNotify:
	case LeaveNotify:
	    pointerX = event.xcrossing.x_root;
	    pointerY = event.xcrossing.y_root;
	    pointerMods = event.xcrossing.state;
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
	screen->alwaysHandleEvent (&event);
	inHandleEvent = false;

	XFlush (dpy);

	lastPointerX = pointerX;
	lastPointerY = pointerY;
	lastPointerMods = pointerMods;
    }

    /* remove destroyed windows */
    removeDestroyed ();

    if (dbg)
    {
	if (dbg->windowsChanged () && dbg->cmpStack (windows, serverWindows))
	{
	    compLogMessage ("core", CompLogLevelDebug, "stacks are out of sync");
	    if (dbg->timedOut ())
		compLogMessage ("core", CompLogLevelDebug, "however, this may be a false positive");
	}

	if (dbg->serverWindowsChanged () && dbg->checkSanity (windows))
	    compLogMessage ("core", CompLogLevelDebug, "windows are stacked incorrectly");
    }
}

CompOption::Value::Vector
cps::PluginManager::mergedPluginList ()
{
    std::list<CompString> availablePlugins(CompPlugin::availablePlugins ());

    CompOption::Value::Vector result;

    CompOption::Value::Vector const& extraPluginsRequested = optionGetActivePlugins();

    /* Must have core as first plugin */
    result.push_back("core");

    /* Add initial plugins */
    foreach(CompString & p, initialPlugins)
    {
	if (p == "core")
	    continue;

	if (blacklist.find (p) != blacklist.end ())
	    continue;

	if (availablePlugins.end() != std::find(availablePlugins.begin(), availablePlugins.end(), p))
	    result.push_back(p);
    }

    /* Add plugins not in the initial list */
    foreach(CompOption::Value const& opt, extraPluginsRequested)
    {
	if (opt.s() == "core")
	    continue;

	if (blacklist.find (opt.s()) != blacklist.end ())
	    continue;

	typedef std::list<CompString>::iterator iterator;
	bool skip = false;

	for (iterator it = initialPlugins.begin(); it != initialPlugins.end();
		it++)
	{
	    if ((*it) == opt.s())
	    {
		skip = true;
		break;
	    }
	}

	if (!skip)
	{
	    if (availablePlugins.end() != std::find(availablePlugins.begin(), availablePlugins.end(), opt.s()))
		result.push_back(opt.s());
	}
    }
    return result;
}


void
cps::PluginManager::updatePlugins ()
{
    possibleTap = NULL;
    dirtyPluginList = false;

    CompOption::Value::Vector const desiredPlugins(mergedPluginList());

    unsigned int pluginIndex;
    for (pluginIndex = 1;
	pluginIndex < plugin.list ().size () && pluginIndex < desiredPlugins.size ();
	pluginIndex++)
    {
	if (plugin.list ().at (pluginIndex).s () != desiredPlugins.at (pluginIndex).s ())
	    break;
    }

    unsigned int desireIndex = pluginIndex;

    // We have pluginIndex pointing at first difference (or end).
    // Now pop plugins off stack to this point, but keep track that they are loaded
    CompPlugin::List alreadyLoaded;
    if (const unsigned int nPop = plugin.list().size() - pluginIndex)
    {
	for (pluginIndex = 0; pluginIndex < nPop; pluginIndex++)
	{
	    alreadyLoaded.push_back(CompPlugin::pop());
	    plugin.list().pop_back();
	}
    }

    // Now work forward through requested plugins
    for (; desireIndex < desiredPlugins.size(); desireIndex++)
    {
	CompPlugin *p = NULL;
	bool failedPush = false;

	// If already loaded, just try to push it...
	foreach(CompPlugin * pp, alreadyLoaded)
	{
	    if (desiredPlugins[desireIndex].s() == pp->vTable->name())
	    {
		if (CompPlugin::push (pp))
		{
		    p = pp;
		    alreadyLoaded.erase(
			    std::find(alreadyLoaded.begin(),
				    alreadyLoaded.end(), pp));
		    break;
		}
		else
		{
		    alreadyLoaded.erase(
			    std::find(alreadyLoaded.begin(),
				    alreadyLoaded.end(), pp));
		    blacklist.insert (desiredPlugins[desireIndex].s ());
		    CompPlugin::unload(pp);
		    p = NULL;
		    failedPush = true;
		    break;
		}
	    }
	}

	// ...otherwise, try to load and push
	if (p == 0 && !failedPush)
	{
	    p = CompPlugin::load(desiredPlugins[desireIndex].s ().c_str ());

	    if (p)
	    {
		if (!CompPlugin::push(p))
		{
		    blacklist.insert (desiredPlugins[desireIndex].s ());
		    CompPlugin::unload(p);
		    p = 0;
		}
	    }
	    else
	    {
		blacklist.insert (desiredPlugins[desireIndex].s ());
	    }
	}

	if (p)
	    plugin.list().push_back(p->vTable->name());
    }

    // Any plugins that are loaded, but were not re-initialized can be unloaded.
    foreach(CompPlugin * pp, alreadyLoaded)
	CompPlugin::unload (pp);

    if (!dirtyPluginList)
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

    quit ();
}

#define IMAGEDIR "images"
#define HOMECOMPIZDIR ".compiz-1"

bool
CompScreenImpl::readImageFromFile (CompString &name,
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
	    path += HOMECOMPIZDIR;
	    path += "/";
	    path += pname;
	    path += "/";
	    path += IMAGEDIR;
	    path += "/";
	    path += name;

	    status = fileToImage (path, size, stride, data);

	    if (status)
		return true;
	}

	path = SHAREDIR;
	path += "/";
	path += pname;
	path += "/";
	path += IMAGEDIR;
	path += "/";
	path += name;
	status = fileToImage (path, size, stride, data);
    }

    return status;
}

bool
CompScreenImpl::writeImageToFile (CompString &path,
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

    result = XGetWindowProperty (dpy, root,
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
    WRAPABLE_HND_FUNCTN_RETURN (bool, fileToImage, name, size, stride, data);
    return _fileToImage(name, size, stride, data);
}

bool
CompScreenImpl::_fileToImage (CompString &name,
			 CompSize   &size,
			 int        &stride,
			 void       *&data)
{
    return false;
}

bool
CompScreen::imageToFile (CompString &path,
			 CompString &format,
			 CompSize   &size,
			 int        stride,
			 void       *data)
{
    WRAPABLE_HND_FUNCTN_RETURN (bool, imageToFile, path, format, size,
			      stride, data);
    return _imageToFile (path, format, size, stride, data);
}

bool
CompScreenImpl::_imageToFile (CompString &path,
			 CompString &format,
			 CompSize   &size,
			 int        stride,
			 void       *data)
{
    return false;
}

void
CompScreen::logMessage (const char   *componentName,
			CompLogLevel level,
			const char   *message)
{
    WRAPABLE_HND_FUNCTN (logMessage, componentName, level, message)
    _logMessage (componentName, level, message);
}

void
CompScreenImpl::_logMessage (const char   *componentName,
			CompLogLevel level,
			const char   *message)
{
    ::logMessage (componentName, level, message);
}

int
PrivateScreen::getWmState (Window id)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    unsigned long state = NormalState;

    result = XGetWindowProperty (dpy, id,
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

    XChangeProperty (dpy, id,
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
cps::PseudoNamespace::windowStateFromString (const char *str)
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

    result = XGetWindowProperty (dpy, id,
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

unsigned int
compiz::window::fillStateData (unsigned int state, Atom *data)
{
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
    if (state & CompWindowStateFocusedMask)
	data[i++] = Atoms::winStateFocused;

    return i;
}

void
PrivateScreen::setWindowState (unsigned int state, Window id)
{
    int i = 0;
    Atom data[32];

    i = compiz::window::fillStateData (state, data);
    XChangeProperty (dpy, id, Atoms::winState,
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

    result = XGetWindowProperty (dpy , id,
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

    result = XGetWindowProperty (dpy, id,
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

    if (XGetWMProtocols (dpy, id, &protocol, &count))
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
CompScreenImpl::getWindowProp (Window       id,
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
CompScreenImpl::setWindowProp (Window       id,
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

    result = XGetWindowProperty (dpy, id, property,
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
CompScreenImpl::getWindowProp32 (Window         id,
			     Atom           property,
			     unsigned short defaultValue)
{
    unsigned short result;

    if (priv->readWindowProp32 (id, property, &result))
	return result;

    return defaultValue;
}

void
CompScreenImpl::setWindowProp32 (Window         id,
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
		unsigned int valueMask = CWX | CWY;
		XWindowChanges xwc;

		xwc.x = w->serverGeometry ().x () + moveX;
		xwc.y = w->serverGeometry ().y () + moveY;

		w->configureXWindow (valueMask, &xwc);
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
    if (optionGetDetectOutputs ())
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
    if (initialized)
    {
	if (!startupSequences.empty ())
	    XDefineCursor (dpy, root, busyCursor);
	else
	    XDefineCursor (dpy, root, normalCursor);
    }
}

#define STARTUP_TIMEOUT_DELAY 15000

bool
cps::StartupSequence::handleStartupSequenceTimeout ()
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
cps::StartupSequence::addSequence (SnStartupSequence *sequence)
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
cps::StartupSequence::removeSequence (SnStartupSequence *sequence)
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
cps::StartupSequence::removeAllSequences ()
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
PrivateScreen::compScreenSnEvent (SnMonitorEvent *event,
			       void           *userData)
{
    PrivateScreen	      *self = (PrivateScreen *) userData;
    SnStartupSequence *sequence;

    sequence = sn_monitor_event_get_startup_sequence (event);

    switch (sn_monitor_event_get_type (event)) {
    case SN_MONITOR_EVENT_INITIATED:
	self->addSequence (sequence);
	break;
    case SN_MONITOR_EVENT_COMPLETED:
	self->removeSequence (sequence);
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
	{ 0, -1,   0,  2,   0,  2,   1, -4 }, /* left */
	{ 1, -1,   0,  2,   0,  2,   1, -4 }, /* right */
	{ 0,  2,   0, -1,   1, -4,   0,  2 }, /* top */
	{ 0,  2,   1, -1,   1, -4,   0,  2 }, /* bottom */
	{ 0, -1,   0, -1,   0,  2,   0,  2 }, /* top-left */
	{ 1, -1,   0, -1,   0,  2,   0,  2 }, /* top-right */
	{ 0, -1,   1, -1,   0,  2,   0,  2 }, /* bottom-left */
	{ 1, -1,   1, -1,   0,  2,   0,  2 }  /* bottom-right */
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
    if (outputNum >= outputDevs.size ())
	outputNum = 0;

    currentOutputDev = outputNum;
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
    if (attrib.width  != ce->width ||
	attrib.height != ce->height)
    {
	attrib.width  = ce->width;
	attrib.height = ce->height;
    }

	reshape (ce->width, ce->height);

	detectOutputDevices ();

	updateOutputDevices ();
}

void
cps::EventManager::setSupportingWmCheck (Display* dpy, Window root)
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

    XChangeProperty (dpy, screen->root(), Atoms::supportingWmCheck,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *) &grabWindow, 1);
}

void
cps::EventManager::createGrabWindow (Display* dpy, Window root, XSetWindowAttributes* attrib)
{
    grabWindow = XCreateWindow (dpy, root, -100, -100, 1, 1, 0,
				  CopyFromParent, InputOnly, CopyFromParent,
				  CWOverrideRedirect | CWEventMask,
				  attrib);
    XMapWindow (dpy, grabWindow);
}


void
CompScreenImpl::updateSupportedWmHints ()
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
    WRAPABLE_HND_FUNCTN (addSupportedAtoms, atoms);
    _addSupportedAtoms (atoms);
}

void
CompScreenImpl::_addSupportedAtoms (std::vector<Atom> &atoms)
{
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
    atoms.push_back (Atoms::winStateFocused);

    atoms.push_back (Atoms::winOpacity);
    atoms.push_back (Atoms::winBrightness);

/* FIXME */
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
    WRAPABLE_HND_FUNCTN (enterShowDesktopMode)
    _enterShowDesktopMode ();
}

unsigned int CompScreenImpl::showingDesktopMask() const
{
    return priv->showingDesktopMask;
}

bool CompScreenImpl::grabsEmpty() const
{
    return priv->grabsEmpty();
}

void
CompScreenImpl::_enterShowDesktopMode ()
{
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
    WRAPABLE_HND_FUNCTN (leaveShowDesktopMode, window)
    _leaveShowDesktopMode (window);
}

void
CompScreenImpl::_leaveShowDesktopMode (CompWindow *window)
{
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
CompScreenImpl::forEachWindow (CompWindow::ForEach proc)
{
    foreach (CompWindow *w, priv->windows)
	proc (w);
}

void
CompScreenImpl::focusDefaultWindow ()
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
	/* Traverse down the stack */
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
			if (!priv->optionGetClickToFocus ())
			{
			    /* We should favor the more active window in the mouse focus
			     * case since the user does not care if the focused window is on top */
			    if (PrivateWindow::compareWindowActiveness (focus, w) < 0)
				focus = w;
			}
			else
			{
			    focus = w;
			    break;
			}
		    }
		}
		else
		{
		    focus = w;

		    if (priv->optionGetClickToFocus ())
			break;
		}
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
CompScreenImpl::findWindow (Window id)
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
CompScreenImpl::findTopLevelWindow (Window id, bool override_redirect)
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
	if (w->priv->frame == id)
	{
	    if (w->overrideRedirect () && !override_redirect)
		return NULL;
	    else
		return w;
	}

    return NULL;
}

void
CompScreenImpl::insertWindow (CompWindow *w, Window	aboveId)
{
    StackDebugger *dbg = StackDebugger::Default ();

    if (dbg)
	dbg->windowsChanged (true);

    priv->stackIsFresh = true;

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
	    ((*it)->priv->frame && (*it)->priv->frame == aboveId))
	{
	    break;
	}
	it++;
    }

    if (it == priv->windows.end ())
    {
	compLogMessage ("core", CompLogLevelDebug, "could not insert 0x%x above 0x%x",
			(unsigned int) w->priv->serverId, aboveId);
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
CompScreenImpl::insertServerWindow (CompWindow *w, Window	aboveId)
{
    StackDebugger *dbg = StackDebugger::Default ();

    if (dbg)
	dbg->serverWindowsChanged (true);

    w->serverPrev = NULL;
    w->serverNext = NULL;

    if (!aboveId || priv->serverWindows.empty ())
    {
	if (!priv->serverWindows.empty ())
	{
	    priv->serverWindows.front ()->serverPrev = w;
	    w->serverNext = priv->serverWindows.front ();
	}
	priv->serverWindows.push_front (w);

	return;
    }

    CompWindowList::iterator it = priv->serverWindows.begin ();

    while (it != priv->serverWindows.end ())
    {
	if ((*it)->priv->serverId == aboveId ||
	    ((*it)->priv->serverFrame && (*it)->priv->serverFrame == aboveId))
	{
	    break;
	}
	it++;
    }

    if (it == priv->serverWindows.end ())
    {
	compLogMessage ("core", CompLogLevelWarn, "could not insert 0x%x above 0x%x",
			(unsigned int) w->priv->serverId, aboveId);
#ifdef DEBUG
	abort ();
#endif
	return;
    }

    w->serverNext = (*it)->serverNext;
    w->serverPrev = (*it);
    (*it)->serverNext = w;

    if (w->serverNext)
    {
	w->serverNext->serverPrev = w;
    }

    priv->serverWindows.insert (++it, w);
}

void
cps::WindowManager::eraseWindowFromMap (Window id)
{
    if (id != 1)
        windowsMap.erase (id);
}

void
CompScreenImpl::unhookWindow (CompWindow *w)
{
    StackDebugger *dbg = StackDebugger::Default ();

    if (dbg)
	dbg->windowsChanged (true);

    CompWindowList::iterator it =
	std::find (priv->windows.begin (), priv->windows.end (), w);

    if (it == priv->windows.end ())
    {
	compLogMessage ("core", CompLogLevelWarn, "a broken plugin tried to remove a window twice, we won't allow that!");
	return;
    }

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

void
CompScreenImpl::unhookServerWindow (CompWindow *w)
{
    StackDebugger *dbg = StackDebugger::Default ();

    if (dbg)
	dbg->serverWindowsChanged (true);

    CompWindowList::iterator it =
	std::find (priv->serverWindows.begin (), priv->serverWindows.end (), w);

    if (it == priv->serverWindows.end ())
    {
	compLogMessage ("core", CompLogLevelWarn, "a broken plugin tried to remove a window twice, we won't allow that!");
	return;
    }

    priv->serverWindows.erase (it);

    if (w->serverNext)
	w->serverNext->serverPrev = w->serverPrev;

    if (w->serverPrev)
	w->serverPrev->serverNext = w->serverNext;

    w->serverNext = NULL;
    w->serverPrev = NULL;
}

Cursor
CompScreenImpl::normalCursor ()
{
    return priv->normalCursor;
}

Cursor
CompScreenImpl::invisibleCursor ()
{
    return priv->invisibleCursor;
}

#define POINTER_GRAB_MASK (ButtonReleaseMask | \
			   ButtonPressMask   | \
			   PointerMotionMask)
CompScreenImpl::GrabHandle
CompScreenImpl::pushGrab (Cursor cursor, const char *name)
{
    if (priv->grabsEmpty ())
    {
	int status;

	status = XGrabPointer (priv->dpy, priv->getGrabWindow(), true,
			       POINTER_GRAB_MASK,
			       GrabModeAsync, GrabModeAsync,
			       priv->root, cursor,
			       CurrentTime);

	if (status == GrabSuccess)
	{
	    status = XGrabKeyboard (priv->dpy,
				    priv->getGrabWindow(), true,
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

    cps::Grab *grab = new cps::Grab (cursor, name);
    priv->grabsPush (grab);

    return grab;
}

void
CompScreenImpl::updateGrab (CompScreen::GrabHandle handle, Cursor cursor)
{
    if (!handle)
	return;

    XChangeActivePointerGrab (priv->dpy, POINTER_GRAB_MASK,
			      cursor, CurrentTime);

    handle->cursor = cursor;
}

void
CompScreenImpl::removeGrab (CompScreen::GrabHandle handle,
			CompPoint *restorePointer)
{
    if (!handle)
	return;

    priv-> grabsRemove(handle);

    if (!priv->grabsEmpty ())
    {
	XChangeActivePointerGrab (priv->dpy,
				  POINTER_GRAB_MASK,
				  priv->grabsBack ()->cursor,
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

void
cps::GrabList::grabsRemove(Grab* handle)
{
    PrivateScreen::GrabIterator it;

    it = std::find (grabsBegin (), grabsEnd (), handle);

    if (it != grabsEnd ())
    {
	grabs.erase (it);
	delete (handle);
    }
}

/* otherScreenGrabExist takes a series of strings terminated by a NULL.
   It returns true if a grab exists but it is NOT held by one of the
   plugins listed, returns false otherwise. */

bool
CompScreenImpl::otherGrabExist (const char *first, ...)
{
    va_list    ap;
    const char *name;

    std::list<cps::Grab *>::iterator it;

    for (it = priv->grabsBegin (); it != priv->grabsEnd (); it++)
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
CompScreenImpl::grabExist (const char *grab)
{
    return priv->grabExist (grab);
}

bool
cps::GrabList::grabExist (const char *grab)
{
    foreach (cps::Grab* g, grabs)
    {
	if (strcmp (g->name, grab) == 0)
	    return true;
    }
    return false;
}

bool
CompScreenImpl::grabbed ()
{
    return priv->isGrabbed();
}

void
cps::GrabManager::grabUngrabOneKey (unsigned int modifiers,
				 int          keycode,
				 bool         grab)
{
    if (grab)
    {
	/*
	 * Always grab the keyboard Sync-ronously. This is so that we can
	 * choose to ReplayKeyboard in alwaysHandleEvent if need be.
	 */
	XGrabKey (screen->dpy(),
		  keycode,
		  modifiers,
		  screen->root(),
		  true,
		  GrabModeAsync,
		  GrabModeSync);
    }
    else
    {
	XUngrabKey (screen->dpy(),
		    keycode,
		    modifiers,
		    screen->root());
    }
}

bool
cps::GrabManager::grabUngrabKeys (unsigned int modifiers,
			       int          keycode,
			       bool         grab)
{
    int             mod, k;
    unsigned int    ignore;

    CompScreen::checkForError (screen->dpy());

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

	    /*
	     * keycode == 0, so this is a modifier-only keybinding.
	     * Until now I have been trying to:
	     *     grabUngrabOneKey (modifiers | ignore, AnyKey, grab);
	     * which does not seem to work at all.
	     * However, binding to each keycode individually does work.
	     * This is so that we can detect taps on individual modifier
	     * keys, and know to cancel the tap if <modifier>+k is pressed.
	     */
	    int minCode, maxCode;
	    XDisplayKeycodes (screen->dpy(), &minCode, &maxCode);
	    for (k = minCode; k <= maxCode; k++)
	        grabUngrabOneKey (modifiers | ignore, k, grab);
	}

	if (CompScreen::checkForError (screen->dpy()))
	    return false;
    }

    return true;
}

bool
cps::GrabManager::addPassiveKeyGrab (CompAction::KeyBinding &key)
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
cps::GrabManager::removePassiveKeyGrab (CompAction::KeyBinding &key)
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

    /*
     * Removing modifier-only grabs is tricky. Because it also removes grabs
     * for modifier+all_other_keys. See XDisplayKeycodes above to find out why.
     * So we need to refresh all grabs...
     */
    if (!(mask & CompNoMask) && key.keycode () == 0)
	updatePassiveKeyGrabs ();
}

void
cps::GrabManager::updatePassiveKeyGrabs ()
{
    std::list<cps::KeyGrab>::iterator it;

    XUngrabKey (screen->dpy(), AnyKey, AnyModifier, screen->root());

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
cps::GrabManager::addPassiveButtonGrab (CompAction::ButtonBinding &button)
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

void cps::GrabManager::updatePassiveButtonGrabs(Window serverFrame)
{
    /* Grab only we have bindings on */
    foreach (ButtonGrab &bind, buttonGrabs)
    {
	unsigned int mods = modHandler->virtualToRealModMask (bind.modifiers);

	if (mods & CompNoMask)
	    continue;

	for (unsigned int ignore = 0;
		 ignore <= modHandler->ignoredModMask (); ignore++)
	{
	    if (ignore & ~modHandler->ignoredModMask ())
		continue;

	    XGrabButton (screen->dpy(),
			 bind.button,
			 mods | ignore,
			 serverFrame,
			 false,
			 ButtonPressMask | ButtonReleaseMask |
			    ButtonMotionMask,
			 GrabModeSync,
			 GrabModeAsync,
			 None,
			 None);
	}
    }
}

void
cps::GrabManager::removePassiveButtonGrab (CompAction::ButtonBinding &button)
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

bool
CompScreenImpl::addAction (CompAction *action)
{
    assert (priv->initialized);
    if (!priv->initialized)
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
CompScreenImpl::removeAction (CompAction *action)
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
CompScreenImpl::updateWorkarea ()
{
    CompRect workArea;
    CompRegion allWorkArea = CompRegion ();
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

	allWorkArea += workArea;
    }

    workArea = allWorkArea.boundingRect ();

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
	if ((unsigned int) n != clientList.size ())
	{
	    clientList.clear ();
	    clientListStacking.clear ();
	    clientIdList.clear ();
	    clientIdListStacking.clear ();

	    XChangeProperty (dpy, root,
			     Atoms::clientList,
			     XA_WINDOW, 32, PropModeReplace,
			     (unsigned char *) &getGrabWindow(), 1);
	    XChangeProperty (dpy, root,
			     Atoms::clientListStacking,
			     XA_WINDOW, 32, PropModeReplace,
			     (unsigned char *) &getGrabWindow(), 1);
	}

	return;
    }

    if ((unsigned int) n != clientList.size ())
    {
	clientIdList.resize (n);
	clientIdListStacking.resize (n);

	updateClientList = updateClientListStacking = true;
    }

    clientListStacking.clear ();

    foreach (CompWindow *w, windows)
	if (isClientListWindow (w))
	    clientListStacking.push_back (w);

    /* clear clientList and copy clientListStacking into clientList */
    clientList = clientListStacking;

    /* sort clientList in mapping order */
    sort (clientList.begin (), clientList.end (),
	  compareMappingOrder);

    /* make sure client id lists are up-to-date */
    for (int i = 0; i < n; i++)
    {
	if (!updateClientList &&
	    clientIdList[i] != clientList[i]->id ())
	{
	    updateClientList = true;
	}

	clientIdList[i] = clientList[i]->id ();
    }
    for (int i = 0; i < n; i++)
    {
	if (!updateClientListStacking &&
	    clientIdListStacking[i] != clientListStacking[i]->id ())
	{
	    updateClientListStacking = true;
	}

	clientIdListStacking[i] = clientListStacking[i]->id ();
    }

    if (updateClientList)
	XChangeProperty (dpy, root,
			 Atoms::clientList,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) &clientIdList.at (0), n);

    if (updateClientListStacking)
	XChangeProperty (dpy, root,
			 Atoms::clientListStacking,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) &clientIdListStacking.at (0),
			 n);
}

const CompWindowVector &
CompScreenImpl::clientList (bool stackingOrder)
{
   return stackingOrder ? priv->clientListStacking : priv->clientList;
}

void
CompScreenImpl::toolkitAction (Atom   toolkitAction,
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
CompScreenImpl::runCommand (CompString command)
{
    if (command.size () == 0)
	return;

    if (fork () == 0)
    {
	size_t       pos;
	CompString   env (priv->displayString);

	setsid ();

	pos = env.find (':');
	if (pos != std::string::npos)
	{
            size_t pointPos = env.find ('.', pos);

	    if (pointPos != std::string::npos)
	    {
		env.erase (pointPos);
	    }
	    else
	    {
                unsigned int displayNum = atoi (env.substr (pos + 1).c_str ());
		env.erase (pos);
		env.append (compPrintf (":%i", displayNum));
	    }
	}

	env.append (compPrintf (".%d", priv->screenNum));

	putenv (const_cast<char *> (env.c_str ()));

	exit (execl ("/bin/sh", "/bin/sh", "-c", command.c_str (), NULL));
    }
}

void
CompScreenImpl::moveViewport (int tx, int ty, bool sync)
{
    CompPoint pnt;

    tx = priv->vp.x () - tx;
    tx = compiz::core::screen::wraparound_mod (tx, priv->vpSize.width ());
    tx -= priv->vp.x ();

    ty = priv->vp.y () - ty;
    ty = compiz::core::screen::wraparound_mod (ty, priv->vpSize.height ());
    ty -= priv->vp.y ();

    if (!tx && !ty)
	return;

    priv->vp.setX (priv->vp.x () + tx);
    priv->vp.setY (priv->vp.y () + ty);

    tx *= -width ();
    ty *= -height ();

    foreach (CompWindow *w, priv->windows)
    {
	unsigned int valueMask = CWX | CWY;
	XWindowChanges xwc;

	if (w->onAllViewports ())
	    continue;

	pnt = w->getMovementForOffset (CompPoint (tx, ty));

	if (w->saveMask () & CWX)
	    w->saveWc ().x += pnt.x ();

	if (w->saveMask () & CWY)
	    w->saveWc ().y += pnt.y ();

	xwc.x = w->serverGeometry ().x () + pnt.x ();
	xwc.y = w->serverGeometry ().y () + pnt.y ();

	w->configureXWindow (valueMask, &xwc);
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
cps::WindowManager::addGroup (Window id)
{
    CompGroup *group = new CompGroup ();

    group->refCnt = 1;
    group->id     = id;

    groups.push_back (group);

    return group;
}

void
cps::WindowManager::removeGroup (CompGroup *group)
{
    group->refCnt--;
    if (group->refCnt)
	return;

    std::list<CompGroup *>::iterator it =
	std::find (groups.begin (), groups.end (), group);

    if (it != groups.end ())
    {
	groups.erase (it);
    }

    delete group;
}

CompGroup *
cps::WindowManager::findGroup (Window id)
{
    foreach (CompGroup *g, groups)
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

    foreach (CompStartupSequence *ss, startupSequences)
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
CompScreenImpl::sendWindowActivationRequest (Window id)
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
    screenEdge[edge].count++;
    if (screenEdge[edge].count == 1)
	XMapRaised (dpy, screenEdge[edge].id);
}

void
PrivateScreen::disableEdge (int edge)
{
    screenEdge[edge].count--;
    if (screenEdge[edge].count == 0)
	XUnmapWindow (dpy, screenEdge[edge].id);
}

Window
PrivateScreen::getTopWindow ()
{
    /* return first window that has not been destroyed */
    if (windows.size ())
	return windows.back ()->id ();

    return None;
}

int
CompScreenImpl::outputDeviceForPoint (const CompPoint &point)
{
    return outputDeviceForPoint (point.x (), point.y ());
}

int
CompScreenImpl::outputDeviceForPoint (int x, int y)
{
    CompWindow::Geometry geom (x, y, 1, 1, 0);

    return outputDeviceForGeometry (geom);
}

CompRect
CompScreenImpl::getCurrentOutputExtents ()
{
    return priv->outputDevs[priv->currentOutputDev];
}

void
PrivateScreen::setNumberOfDesktops (unsigned int nDesktop)
{
    if (nDesktop < 1 || nDesktop >= 0xffffffff)
	return;

    if (nDesktop == this->nDesktop)
	return;

    if (currentDesktop >= nDesktop)
	currentDesktop = nDesktop - 1;

    foreach (CompWindow *w, windows)
    {
	if (w->desktop () == 0xffffffff)
	    continue;

	if (w->desktop () >= nDesktop)
	    w->setDesktop (nDesktop - 1);
    }

    this->nDesktop = nDesktop;

    setDesktopHints ();
}

void
PrivateScreen::setCurrentDesktop (unsigned int desktop)
{
    unsigned long data;

    if (desktop >= nDesktop)
	return;

    if (desktop == currentDesktop)
	return;

    currentDesktop = desktop;

    foreach (CompWindow *w, windows)
    {
	if (w->desktop () == 0xffffffff)
	    continue;

	if (w->desktop () == desktop)
	    w->priv->show ();
	else
	    w->priv->hide ();
    }

    data = desktop;

    XChangeProperty (dpy, root, Atoms::currentDesktop,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

const CompRect&
CompScreenImpl::getWorkareaForOutput (unsigned int outputNum) const
{
    return priv->outputDevs[outputNum].workArea ();
}

void
CompScreen::outputChangeNotify ()
{
    WRAPABLE_HND_FUNCTN (outputChangeNotify);
    _outputChangeNotify ();
}

void
CompScreenImpl::_outputChangeNotify ()
{
}

/* Returns default viewport for some window geometry. If the window spans
   more than one viewport the most appropriate viewport is returned. How the
   most appropriate viewport is computed can be made optional if necessary. It
   is currently computed as the viewport where the center of the window is
   located. */
void
CompScreenImpl::viewportForGeometry (const CompWindow::Geometry& gm,
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
CompScreenImpl::outputDeviceForGeometry (const CompWindow::Geometry& gm)
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
CompScreenImpl::defaultIcon () const
{
    return priv->defaultIcon;
}

bool
CompScreenImpl::updateDefaultIcon ()
{
    CompString file = priv->optionGetDefaultIcon ();
    CompString pname = "core/";
    void       *data;
    CompSize   size;

    if (priv->defaultIcon)
    {
	delete priv->defaultIcon;
	priv->defaultIcon = NULL;
    }

    if (!readImageFromFile (file, pname, size, data))
	return false;

    priv->defaultIcon = new CompIcon (size.width (), size.height ());

    memcpy (priv->defaultIcon->data (), data,
	    size.width () * size.height () * sizeof (CARD32));

    free (data);

    return true;
}

void
cps::History::setCurrentActiveWindowHistory (int x, int y)
{
    int	i, min = 0;

    for (i = 0; i < ACTIVE_WINDOW_HISTORY_NUM; i++)
    {
	if (history[i].x == x && history[i].y == y)
	{
	    currentHistory = i;
	    return;
	}
    }

    for (i = 1; i < ACTIVE_WINDOW_HISTORY_NUM; i++)
	if (history[i].activeNum < history[min].activeNum)
	    min = i;

    currentHistory = min;

    history[min].activeNum = activeNum;
    history[min].x         = x;
    history[min].y         = y;

    memset (history[min].id, 0, sizeof (history[min].id));
}

void
cps::History::addToCurrentActiveWindowHistory (Window id)
{
    CompActiveWindowHistory *history = &this->history[currentHistory];
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

    history->activeNum = activeNum;
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
CompScreenImpl::root ()
{
    return priv->root;
}

int
CompScreenImpl::xkbEvent ()
{
    return priv->xkbEvent;
}

void
CompScreenImpl::warpPointer (int dx,
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

    /* XWarpPointer will generate Leave, Enter and PointerMotion
     * events as if the user had instantaneously moved the cursor
     * from one position to another. Because most of this is
     * useless to process, we just throw out the events and update
     * the pointer position. However, we do need to process some
     * crossing events since they tell us which edge windows are
     * hovered. Note that we don't actually trigger the bindings
     * in the case where we warped from one edge window to
     * another.
     *
     * FIXME: Probably don't need to process *all* the crossing
     * events here ... maybe there is a way to check only the last
     * event in the output buffer without roundtripping a lot */
    while (XCheckMaskEvent (priv->dpy,
			    LeaveWindowMask |
			    EnterWindowMask |
			    PointerMotionMask,
			    &event))
    {
	if (event.type == EnterNotify)
	{
	    if (event.xcrossing.mode != NotifyGrab ||
		event.xcrossing.mode != NotifyUngrab ||
		event.xcrossing.mode != NotifyInferior)
	    {
		priv->edgeWindow = 0;

		for (unsigned int i = 0; i < SCREEN_EDGE_NUM; i++)
		{
		    if (event.xcrossing.window == priv->screenEdge[i].id)
		    {
			priv->edgeWindow = 1 << i;
			break;
		    }
		}
	    }
	}
    }

    if (!inHandleEvent)
    {
	lastPointerX = pointerX;
	lastPointerY = pointerY;
    }
}

CompWindowList &
CompScreenImpl::windows ()
{
    return priv->windows;
}

CompWindowList &
CompScreenImpl::serverWindows ()
{
    return priv->serverWindows;
}

CompWindowList &
CompScreenImpl::destroyedWindows ()
{
    return priv->destroyedWindows;
}


Time
CompScreenImpl::getCurrentTime ()
{
    return priv->getCurrentTime (priv->dpy);
}

Time
cps::EventManager::getCurrentTime (Display* dpy) const
{
    XEvent event;

    XChangeProperty (dpy, grabWindow,
		     XA_PRIMARY, XA_STRING, 8,
		     PropModeAppend, NULL, 0);
    XWindowEvent (dpy, grabWindow,
		  PropertyChangeMask,
		  &event);

    return event.xproperty.time;
}

Window
CompScreenImpl::selectionWindow ()
{
    return priv->wmSnSelectionWindow;
}

int
CompScreenImpl::screenNum ()
{
    return priv->screenNum;
}

const CompPoint &
CompScreenImpl::vp () const
{
    return priv->vp;
}

const CompSize &
CompScreenImpl::vpSize () const
{
    return priv->vpSize;
}

int
CompScreenImpl::desktopWindowCount ()
{
    return priv->desktopWindowCount;
}

unsigned int
CompScreenImpl::activeNum () const
{
    return priv->getActiveNum();
}

CompOutput::vector &
CompScreenImpl::outputDevs ()
{
    return priv->outputDevs;
}

CompOutput &
CompScreenImpl::currentOutputDev () const
{
    return priv->outputDevs [priv->currentOutputDev];
}

const CompRect &
CompScreenImpl::workArea () const
{
    return priv->workArea;
}

unsigned int
CompScreenImpl::currentDesktop ()
{
    return priv->currentDesktop;
}

unsigned int
CompScreenImpl::nDesktop ()
{
    return priv->nDesktop;
}

CompActiveWindowHistory *
CompScreenImpl::currentHistory ()
{
    return priv->getCurrentHistory ();
}

bool
CompScreenImpl::shouldSerializePlugins ()
{
    return priv->optionGetDoSerialize ();
}

void
cps::WindowManager::removeDestroyed ()
{
    while (pendingDestroys)
    {
	foreach (CompWindow *w, destroyedWindows)
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
CompScreenImpl::region () const
{
    return priv->region;
}

bool
CompScreenImpl::hasOverlappingOutputs ()
{
    return priv->hasOverlappingOutputs;
}

CompOutput &
CompScreenImpl::fullscreenOutput ()
{
    return priv->fullscreenOutput;
}


XWindowAttributes
CompScreenImpl::attrib ()
{
    return priv->attrib;
}

std::vector<XineramaScreenInfo> &
CompScreenImpl::screenInfo ()
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
    priv ()
{
}

CompScreenImpl::CompScreenImpl () :
    eventHandled (false)
{
    CompPrivate p;
    CompOption::Value::Vector vList;
    CompPlugin  *corePlugin;

    priv.reset (new PrivateScreen (this));

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

    priv->setPlugins (vList);
}

bool
CompScreenImpl::init (const char *name)
{
    return priv->init(name);
}

bool
cps::EventManager::init (const char *name)
{
    ctx = Glib::MainContext::get_default ();
    mainloop = Glib::MainLoop::create (ctx, false);
    sighupSource = CompSignalSource::create (SIGHUP, boost::bind (&PrivateScreen::handleSignal, this, _1));
    sigintSource = CompSignalSource::create (SIGINT, boost::bind (&PrivateScreen::handleSignal, this, _1));
    sigtermSource = CompSignalSource::create (SIGTERM, boost::bind (&PrivateScreen::handleSignal, this, _1));

    if (!initDisplay(name)) return false;

    optionSetCloseWindowKeyInitiate (CompScreenImpl::closeWin);
    optionSetCloseWindowButtonInitiate (CompScreenImpl::closeWin);
    optionSetRaiseWindowKeyInitiate (CompScreenImpl::raiseWin);
    optionSetRaiseWindowButtonInitiate (CompScreenImpl::raiseWin);
    optionSetLowerWindowKeyInitiate (CompScreenImpl::lowerWin);
    optionSetLowerWindowButtonInitiate (CompScreenImpl::lowerWin);

    optionSetUnmaximizeWindowKeyInitiate (CompScreenImpl::unmaximizeWin);

    optionSetMinimizeWindowKeyInitiate (CompScreenImpl::minimizeWin);
    optionSetMinimizeWindowButtonInitiate (CompScreenImpl::minimizeWin);
    optionSetMaximizeWindowKeyInitiate (CompScreenImpl::maximizeWin);
    optionSetMaximizeWindowHorizontallyKeyInitiate (
	CompScreenImpl::maximizeWinHorizontally);
    optionSetMaximizeWindowVerticallyKeyInitiate (
	CompScreenImpl::maximizeWinVertically);

    optionSetWindowMenuKeyInitiate (CompScreenImpl::windowMenu);
    optionSetWindowMenuButtonInitiate (CompScreenImpl::windowMenu);

    optionSetShowDesktopKeyInitiate (CompScreenImpl::showDesktop);
    optionSetShowDesktopEdgeInitiate (CompScreenImpl::showDesktop);

    optionSetToggleWindowMaximizedKeyInitiate (CompScreenImpl::toggleWinMaximized);
    optionSetToggleWindowMaximizedButtonInitiate (CompScreenImpl::toggleWinMaximized);

    optionSetToggleWindowMaximizedHorizontallyKeyInitiate (
	CompScreenImpl::toggleWinMaximizedHorizontally);
    optionSetToggleWindowMaximizedVerticallyKeyInitiate (
	CompScreenImpl::toggleWinMaximizedVertically);

    optionSetToggleWindowShadedKeyInitiate (CompScreenImpl::shadeWin);

    return true;
}

bool
cps::EventManager::initDisplay (const char *name)
{
    return true;
}

bool CompScreen::displayInitialised() const
{
    return priv && priv->initialized;
}

bool
PrivateScreen::initDisplay (const char *name)
{
    dpy = XOpenDisplay (name);
    if (!dpy)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't open display %s", XDisplayName (name));
	return false;
    }

    XSynchronize (dpy, synchronousX ? True : False);

    snprintf (displayString, 255, "DISPLAY=%s",
	      DisplayString (dpy));

    Atoms::init (dpy);

    XSetErrorHandler (errorHandler);

    snDisplay = sn_display_new (dpy, NULL, NULL);
    if (!snDisplay)
	return true;

    lastPing = 1;

    if (!XSyncQueryExtension (dpy, &syncEvent, &syncError))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No sync extension");
	return false;
    }

    randrExtension = XRRQueryExtension (dpy, &randrEvent,
					      &randrError);

    shapeExtension = XShapeQueryExtension (dpy, &shapeEvent,
						 &shapeError);

    int                  xkbOpcode;
    xkbExtension = XkbQueryExtension (dpy, &xkbOpcode,
					    &xkbEvent, &xkbError,
					    NULL, NULL);
    if (xkbExtension)
    {
	XkbSelectEvents (dpy, XkbUseCoreKbd,
			 XkbBellNotifyMask | XkbStateNotifyMask,
			 XkbAllEventsMask);
    }
    else
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No XKB extension");

	xkbEvent = xkbError = -1;
    }

    xineramaExtension = XineramaQueryExtension (dpy,
						      &xineramaEvent,
						      &xineramaError);

    updateScreenInfo ();

    escapeKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Escape"));
    returnKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Return"));

    char                 buf[128];
    sprintf (buf, "WM_S%d", DefaultScreen (dpy));
    Atom wmSnAtom = XInternAtom (dpy, buf, 0);

    Window currentWmSnOwner = XGetSelectionOwner (dpy, wmSnAtom);

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

    Window root_tmp = XRootWindow (dpy, DefaultScreen (dpy));

    XSetWindowAttributes attr;
    attr.override_redirect = true;
    attr.event_mask        = PropertyChangeMask;

    Window newWmSnOwner =
	XCreateWindow (dpy, root_tmp, -100, -100, 1, 1, 0,
		       CopyFromParent, CopyFromParent,
		       CopyFromParent,
		       CWOverrideRedirect | CWEventMask,
		       &attr);

    XChangeProperty (dpy, newWmSnOwner, Atoms::wmName, Atoms::utf8String, 8,
		     PropModeReplace, (unsigned char *) PACKAGE,
		     strlen (PACKAGE));

    XEvent event;
    XWindowEvent (dpy, newWmSnOwner, PropertyChangeMask, &event);

    Time wmSnTimestamp = event.xproperty.time;

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
    event.xclient.window       = root_tmp;
    event.xclient.message_type = Atoms::manager;
    event.xclient.format       = 32;
    event.xclient.data.l[0]    = wmSnTimestamp;
    event.xclient.data.l[1]    = wmSnAtom;
    event.xclient.data.l[2]    = 0;
    event.xclient.data.l[3]    = 0;
    event.xclient.data.l[4]    = 0;

    XSendEvent (dpy, root_tmp, FALSE, StructureNotifyMask, &event);

    /* Wait for old window manager to go away */
    if (currentWmSnOwner != None)
    {
	do {
	    XWindowEvent (dpy, currentWmSnOwner, StructureNotifyMask, &event);
	} while (event.type != DestroyNotify);
    }

    modHandler->updateModifierMappings ();

    CompScreenImpl::checkForError (dpy);

    XGrabServer (dpy);

    /* Don't select for SubstructureRedirectMask or
     * SubstructureNotifyMask yet since we need to
     * act in complete synchronization here when
     * doing the initial window init in order to ensure
     * that windows don't receive invalid stack positions
     * but we don't want to receive an invalid CreateNotify
     * when we create windows like edge windows here either */
    XSelectInput (dpy, root_tmp,
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

	if (rt == root_tmp)
	    continue;

	XSelectInput (dpy, rt,
		      FocusChangeMask |
		      SubstructureNotifyMask);
    }

    if (CompScreenImpl::checkForError (dpy))
    {
	compLogMessage ("core", CompLogLevelError,
			"Another window manager is "
			"already running on screen: %d", DefaultScreen (dpy));

	XUngrabServer (dpy);
	XSync (dpy, false);
	return false;
    }

    for (int i = 0; i < SCREEN_EDGE_NUM; i++)
    {
	screenEdge[i].id    = None;
	screenEdge[i].count = 0;
    }

    screenNum = DefaultScreen (dpy);
    colormap  = DefaultColormap (dpy, screenNum);
    root = root_tmp;

    snContext = sn_monitor_context_new (snDisplay, screenNum,
					      compScreenSnEvent, this, NULL);

    wmSnSelectionWindow = newWmSnOwner;
    this->wmSnAtom            = wmSnAtom;
    this->wmSnTimestamp       = wmSnTimestamp;

    if (!XGetWindowAttributes (dpy, root, &attrib))
	return false;

    workArea.setWidth (attrib.width);
    workArea.setHeight (attrib.height);

    XVisualInfo          templ;
    templ.visualid = XVisualIDFromVisual (attrib.visual);

    int                  nvisinfo;
    XVisualInfo* visinfo = XGetVisualInfo (dpy, VisualIDMask, &templ, &nvisinfo);
    if (!nvisinfo)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't get visual info for default visual");
	return false;
    }

    XColor               black;
    black.red = black.green = black.blue = 0;

    if (!XAllocColor (dpy, colormap, &black))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't allocate color");
	XFree (visinfo);
	return false;
    }

    static char          data = 0;
    Pixmap bitmap = XCreateBitmapFromData (dpy, root, &data, 1, 1);
    if (!bitmap)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't create bitmap");
	XFree (visinfo);
	return false;
    }

    invisibleCursor = XCreatePixmapCursor (dpy, bitmap, bitmap,
						 &black, &black, 0, 0);
    if (!invisibleCursor)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't create invisible cursor");
	XFree (visinfo);
	return false;
    }

    XFreePixmap (dpy, bitmap);
    XFreeColors (dpy, colormap, &black.pixel, 1, 0);

    XFree (visinfo);

    reshape (attrib.width, attrib.height);

    initialized = true;
    initOptions ();
    detectOutputDevices ();
    updateOutputDevices ();

    getDesktopHints ();

    {
	XSetWindowAttributes attrib;
	attrib.override_redirect = 1;
	attrib.event_mask = PropertyChangeMask;

	createGrabWindow(dpy, root, &attrib);

	for (int i = 0; i < SCREEN_EDGE_NUM; i++)
	{
	    long xdndVersion = 3;

	    screenEdge[i].id = XCreateWindow (dpy, root,
						    -100, -100, 1, 1, 0,
						    CopyFromParent, InputOnly,
						    CopyFromParent,
						    CWOverrideRedirect,
						    &attrib);

	    XChangeProperty (dpy, screenEdge[i].id, Atoms::xdndAware,
			     XA_ATOM, 32, PropModeReplace,
			     (unsigned char *) &xdndVersion, 1);

	    /* CompWindow::CompWindow will select for
	     * crossing events when it gets called on
	     * CreateNotify of this window, so no need
	     * to select for them here */
	    XSelectInput (dpy, screenEdge[i].id,
			  StructureNotifyMask |
			  ButtonPressMask   |
			  ButtonReleaseMask |
			  PointerMotionMask);
	}
    }

    updateScreenEdges ();

    setDesktopHints ();
    setSupportingWmCheck (dpy, root);
    screen->updateSupportedWmHints ();

    normalCursor = XCreateFontCursor (dpy, XC_left_ptr);
    busyCursor   = XCreateFontCursor (dpy, XC_watch);

    XDefineCursor (dpy, root, normalCursor);

    /* We should get DestroyNotify events for any windows that were
     * destroyed while initializing windows here now */
    XSelectInput (dpy, root, attrib.your_event_mask |
		  SubstructureRedirectMask | SubstructureNotifyMask);

    Window rootReturn, parentReturn;
    Window               *children;
    unsigned int         nchildren;
    XQueryTree (dpy, root,
		&rootReturn, &parentReturn,
		&children, &nchildren);

    XUngrabServer (dpy);
    XSync (dpy, FALSE);

    /* Start initializing windows here */

    for (unsigned int i = 0; i < nchildren; i++)
    {
	XWindowAttributes attrib;

	/* Failure means the window has been destroyed, do not
	 * manage this window since we will not receive a DestroyNotify
	 * for it
	 */

	if (!XGetWindowAttributes (screen->dpy (), children[i], &attrib))
	    setDefaultWindowAttributes(&attrib);

	PrivateWindow::createCompWindow (i ? children[i - 1] : 0, attrib, children[i]);
    }

    /* enforce restack on all windows
     * using list last sent to server
    i = 0;
    for (CompWindowList::reverse_iterator rit = serverWindows.rbegin ();
	 rit != serverWindows.rend (); rit++)
	children[i++] = ROOTPARENT ((*rit));

    XRestackWindows (dpy, children, i);
    */
    XFree (children);

    foreach (CompWindow *w, windows)
    {
	if (w->isViewable ())
	    w->priv->activeNum = nextActiveNum ();
    }

    Window               focus;
    int                  revertTo;
    XGetInputFocus (dpy, &focus, &revertTo);

    /* move input focus to root window so that we get a FocusIn event when
       moving it to the default window */
    XSetInputFocus (dpy, root, RevertToPointerRoot, CurrentTime);

    if (focus == None || focus == PointerRoot)
    {
	screen->focusDefaultWindow ();
    }
    else
    {
	CompWindow *w;

	w = screen->findWindow (focus);
	if (w)
	    w->moveInputFocusTo ();
	else
	    screen->focusDefaultWindow ();
    }

    /* Need to set a default here so that the value isn't uninitialized
     * when loading plugins FIXME: Should find a way to initialize options
     * first and then set this value, or better yet, tie this value directly
     * to the option */
    vpSize.setWidth (optionGetHsize ());
    vpSize.setHeight (optionGetVsize ());

    /* TODO: Bailout properly when screenInitPlugins fails
     * TODO: It would be nicer if this line could mean
     * "init all the screens", but unfortunately it only inits
     * plugins loaded on the command line screen's and then
     * we need to call updatePlugins () to init the remaining
     * screens from option changes */
    assert (CompPlugin::screenInitPlugins (screen));

    /* The active plugins list might have been changed - load any
     * new plugins */

    vpSize.setWidth (optionGetHsize ());
    vpSize.setHeight (optionGetVsize ());

    setAudibleBell (optionGetAudibleBell ());


    pingTimer.setTimes (optionGetPingDelay (),
			      optionGetPingDelay () + 500);

    pingTimer.start ();

    return true;
}

CompScreenImpl::~CompScreenImpl ()
{
    priv->removeAllSequences ();

    while (!priv->windows.empty ())
        delete priv->windows.front ();

    while (CompPlugin* p = CompPlugin::pop ())
	CompPlugin::unload (p);

    screen = NULL;
}

cps::GrabManager::GrabManager (CompScreen *screen) :
    ScreenUser(screen),
    buttonGrabs (),
    keyGrabs ()
{
}

cps::ViewPort::ViewPort() :
    vp (0, 0),
    vpSize (1, 1)
{
}

cps::StartupSequence::StartupSequence() :
    startupSequences (),
    startupSequenceTimer ()
{
}

PrivateScreen::PrivateScreen (CompScreen *screen) :
    ScreenUser (screen),
    EventManager (screen),
    GrabManager (screen),
    screenInfo (0),
    snDisplay(0),
    windows (),
    nDesktop (1),
    currentDesktop (0),
    root (None),
    outputDevs (0),
    currentOutputDev (0),
    hasOverlappingOutputs (false),
    snContext (0),
    showingDesktopMask (0),
    desktopHintData (0),
    desktopHintSize (0),
    initialized (false),
    edgeWindow (None),
    edgeDelayTimer (),
    xdndWindow (None)
{
    pingTimer.setCallback (
	boost::bind (&PrivateScreen::handlePingTimeout, this));

    startupSequenceTimer.setCallback (
	boost::bind (&PrivateScreen::handleStartupSequenceTimeout, this));
    startupSequenceTimer.setTimes (1000, 1500);
}

cps::History::History() :
    currentHistory(0),
    activeNum (1)
{
    memset (&history[currentHistory], 0, sizeof history[currentHistory]);
}

cps::WindowManager::WindowManager() :
    activeWindow (0),
    below (None),
    autoRaiseTimer (),
    autoRaiseWindow (0),
    serverWindows (),
    destroyedWindows (),
    stackIsFresh (false),
    groups (0),
    pendingDestroys (0)
{
}

cps::PluginManager::PluginManager(CompScreen *screen) :
    ScreenUser (screen),
    CoreOptions (false),
    plugin (),
    dirtyPluginList (true)
{
}

cps::EventManager::EventManager (CompScreen *screen) :
    ScreenUser (screen),
    PluginManager (screen),
    source(0),
    timeout(0),
    fileWatch (0),
    lastFileWatchHandle (1),
    watchFds (0),
    lastWatchFdHandle (1),
    grabWindow (None)
{
    ValueHolder::SetDefault (static_cast<ValueHolder *> (this));
    TimeoutHandler *dTimeoutHandler = new TimeoutHandler ();
    TimeoutHandler::SetDefault (dTimeoutHandler);
}

cps::OrphanData::OrphanData() :
    desktopWindowCount (0),
    mapNum (1),
    defaultIcon (0)
{
}

cps::OrphanData::~OrphanData()
{
    if (defaultIcon)
	delete defaultIcon;
}

cps::EventManager::~EventManager ()
{
    delete timeout;
    delete source;

    foreach (CompWatchFd *fd, watchFds)
	delete fd;

    watchFds.clear ();
}

PrivateScreen::~PrivateScreen ()
{
    if (initialized)
    {
	XUngrabKey (dpy, AnyKey, AnyModifier, root);

	initialized = false;

	for (int i = 0; i < SCREEN_EDGE_NUM; i++)
	    XDestroyWindow (dpy, screenEdge[i].id);

	destroyGrabWindow (dpy);

	XFreeCursor (dpy, invisibleCursor);
	XSync (dpy, False);

	if (snContext)
	    sn_monitor_context_unref (snContext);

	XCloseDisplay (dpy);
    }

    if (desktopHintData)
	free (desktopHintData);

    if (snDisplay)
	sn_display_unref (snDisplay);
}
