/*
 * Copyright © 2008 Dennis Kasprzyk
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Dennis Kasprzyk not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Dennis Kasprzyk makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DENNIS KASPRZYK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Dennis Kasprzyk <onestone@compiz-fusion.org>
 *          David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>

#include "privates.h"

#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>

CompWindow *lastDamagedWindow = 0;

void
PrivateCompositeScreen::handleEvent (XEvent *event)
{
    CompWindow      *w;

    switch (event->type) {

	case CreateNotify:
	    if (screen->root () == event->xcreatewindow.parent)
	    {
		/* The first time some client asks for the composite
		 * overlay window, the X server creates it, which causes
		 * an errorneous CreateNotify event.  We catch it and
		 * ignore it. */
		if (overlay == event->xcreatewindow.window)
		    return;
	    }
	    break;
	case PropertyNotify:
	    if (event->xproperty.atom == Atoms::winOpacity)
	    {
		w = screen->findWindow (event->xproperty.window);
		if (w)
		    CompositeWindow::get (w)->updateOpacity ();
	    }
	    else if (event->xproperty.atom == Atoms::winBrightness)
	    {
		w = screen->findWindow (event->xproperty.window);
		if (w)
		    CompositeWindow::get (w)->updateBrightness ();
	    }
	    else if (event->xproperty.atom == Atoms::winSaturation)
	    {
		w = screen->findWindow (event->xproperty.window);
		if (w)
		    CompositeWindow::get (w)->updateSaturation ();
	    }
	    break;
	default:
	    if (shapeExtension &&
		event->type == shapeEvent + ShapeNotify)
	    {
		w = screen->findWindow (((XShapeEvent *) event)->window);
		if (w)
		{
		    if (w->mapNum ())
		    {
		        CompositeWindow::get (w)->addDamage ();
		    }
		}
	    }
	    break;
    }

    screen->handleEvent (event);

    switch (event->type) {
	case Expose:
	    handleExposeEvent (&event->xexpose);
	break;
	case ClientMessage:
	    if (event->xclient.message_type == Atoms::winOpacity)
	    {
		w = screen->findWindow (event->xclient.window);
		if (w && (w->type () & CompWindowTypeDesktopMask) == 0)
		{
		    unsigned short opacity = event->xclient.data.l[0] >> 16;

		    screen->setWindowProp32 (w->id (),
			Atoms::winOpacity, opacity);
		}
	    }
	    else if (event->xclient.message_type ==
		     Atoms::winBrightness)
	    {
		w = screen->findWindow (event->xclient.window);
		if (w)
		{
		    unsigned short brightness = event->xclient.data.l[0] >> 16;

		    screen->setWindowProp32 (w->id (),
			Atoms::winBrightness, brightness);
		}
	    }
	    else if (event->xclient.message_type ==
		     Atoms::winSaturation)
	    {
		w = screen->findWindow (event->xclient.window);
		if (w)
		{
		    unsigned short saturation = event->xclient.data.l[0] >> 16;

		    screen->setWindowProp32 (w->id (),
			Atoms::winSaturation, saturation);
		}
	    }
	    break;
	default:
	    if (event->type == damageEvent + XDamageNotify)
	    {
		XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;

		if (lastDamagedWindow && de->drawable == lastDamagedWindow->id ())
		{
		    w = lastDamagedWindow;
		}
		else
		{
		    w = screen->findWindow (de->drawable);
		    if (w)
			lastDamagedWindow = w;
		}

		if (w)
		    CompositeWindow::get (w)->processDamage (de);
	    }
	    else if (shapeExtension &&
		     event->type == shapeEvent + ShapeNotify)
	    {
		w = screen->findWindow (((XShapeEvent *) event)->window);
		if (w)
		{
		    if (w->mapNum ())
		    {
		        CompositeWindow::get (w)->addDamage ();
		    }
		}
	    }
	    else if (randrExtension &&
		     event->type == randrEvent + RRScreenChangeNotify)
	    {
		XRRScreenChangeNotifyEvent *rre;

		rre = (XRRScreenChangeNotifyEvent *) event;

		if (screen->root () == rre->root)
		    detectRefreshRate ();
	    }
	    break;
    }
}

int
CompositeScreen::damageEvent ()
{
    return priv->damageEvent;
}


CompositeScreen::CompositeScreen (CompScreen *s) :
    PluginClassHandler<CompositeScreen, CompScreen, COMPIZ_COMPOSITE_ABI> (s),
    priv (new PrivateCompositeScreen (this))
{
    int	compositeMajor, compositeMinor;

    if (!XQueryExtension (s->dpy (), COMPOSITE_NAME,
			  &priv->compositeOpcode,
			  &priv->compositeEvent,
			  &priv->compositeError))
    {
	compLogMessage ("core", CompLogLevelFatal,
		        "No composite extension");
	setFailed ();
	return;
    }

    XCompositeQueryVersion (s->dpy (), &compositeMajor, &compositeMinor);
    if (compositeMajor == 0 && compositeMinor < 2)
    {
	compLogMessage ("core", CompLogLevelFatal,
		        "Old composite extension");
	setFailed ();
	return;
    }

    if (!XDamageQueryExtension (s->dpy (), &priv->damageEvent,
				&priv->damageError))
    {
	compLogMessage ("core", CompLogLevelFatal,
		        "No damage extension");
	setFailed ();
	return;
    }

    if (!XFixesQueryExtension (s->dpy (), &priv->fixesEvent, &priv->fixesError))
    {
	compLogMessage ("core", CompLogLevelFatal,
		        "No fixes extension");
	setFailed ();
	return;
    }

    priv->shapeExtension = XShapeQueryExtension (s->dpy (), &priv->shapeEvent,
						 &priv->shapeError);
    priv->randrExtension = XRRQueryExtension (s->dpy (), &priv->randrEvent,
					      &priv->randrError);

    priv->makeOutputWindow ();

    priv->detectRefreshRate ();

    priv->slowAnimations = false;

    if (!priv->init ())
    {
        setFailed ();
    }

}

CompositeScreen::~CompositeScreen ()
{
    priv->paintTimer.stop ();

#ifdef USE_COW
    if (useCow)
	XCompositeReleaseOverlayWindow (screen->dpy (),
					screen->root ());
#endif

    delete priv;
}


PrivateCompositeScreen::PrivateCompositeScreen (CompositeScreen *cs) :
    cScreen (cs),
    damageMask (COMPOSITE_SCREEN_DAMAGE_ALL_MASK),
    overlay (None),
    output (None),
    exposeRects (),
    windowPaintOffset (0, 0),
    overlayWindowCount (0),
    nextRedraw (0),
    redrawTime (1000 / 50),
    optimalRedrawTime (1000 / 50),
    frameStatus (0),
    timeMult (1),
    idle (true),
    timeLeft (0),
    slowAnimations (false),
    active (false),
    pHnd (NULL),
    FPSLimiterMode (CompositeFPSLimiterModeDefault),
    frameTimeAccumulator (0)
{
    gettimeofday (&lastRedraw, 0);
    // wrap outputChangeNotify
    ScreenInterface::setHandler (screen);

    optionSetSlowAnimationsKeyInitiate (CompositeScreen::toggleSlowAnimations);
}

PrivateCompositeScreen::~PrivateCompositeScreen ()
{
}

bool
PrivateCompositeScreen::init ()
{
    Display              *dpy = screen->dpy ();
    Window               newCmSnOwner = None;
    Atom                 cmSnAtom = 0;
    Time                 cmSnTimestamp = 0;
    XEvent               event;
    XSetWindowAttributes attr;
    Window               currentCmSnOwner;
    char                 buf[128];

    sprintf (buf, "_NET_WM_CM_S%d", screen->screenNum ());
    cmSnAtom = XInternAtom (dpy, buf, 0);

    currentCmSnOwner = XGetSelectionOwner (dpy, cmSnAtom);

    if (currentCmSnOwner != None)
    {
	if (!replaceCurrentWm)
	{
	    compLogMessage ("composite", CompLogLevelError,
			    "Screen %d on display \"%s\" already "
			    "has a compositing manager; try using the "
			    "--replace option to replace the current "
			    "compositing manager.",
			    screen->screenNum (), DisplayString (dpy));

	    return false;
	}
    }

    attr.override_redirect = true;
    attr.event_mask        = PropertyChangeMask;

    newCmSnOwner =
	XCreateWindow (dpy, screen->root (),
		       -100, -100, 1, 1, 0,
		       CopyFromParent, CopyFromParent,
		       CopyFromParent,
		       CWOverrideRedirect | CWEventMask,
		       &attr);

    XChangeProperty (dpy, newCmSnOwner, Atoms::wmName, Atoms::utf8String, 8,
		     PropModeReplace, (unsigned char *) PACKAGE,
		     strlen (PACKAGE));

    XWindowEvent (dpy, newCmSnOwner, PropertyChangeMask, &event);

    cmSnTimestamp = event.xproperty.time;

    XSetSelectionOwner (dpy, cmSnAtom, newCmSnOwner, cmSnTimestamp);

    if (XGetSelectionOwner (dpy, cmSnAtom) != newCmSnOwner)
    {
	compLogMessage ("core", CompLogLevelError,
			"Could not acquire compositing manager "
			"selection on screen %d display \"%s\"",
			screen->screenNum (), DisplayString (dpy));

	return false;
    }

    /* Send client message indicating that we are now the compositing manager */
    event.xclient.type         = ClientMessage;
    event.xclient.window       = screen->root ();
    event.xclient.message_type = Atoms::manager;
    event.xclient.format       = 32;
    event.xclient.data.l[0]    = cmSnTimestamp;
    event.xclient.data.l[1]    = cmSnAtom;
    event.xclient.data.l[2]    = 0;
    event.xclient.data.l[3]    = 0;
    event.xclient.data.l[4]    = 0;

    XSendEvent (dpy, screen->root (), FALSE, StructureNotifyMask, &event);

    return true;
}


bool
CompositeScreen::registerPaintHandler (PaintHandler *pHnd)
{
    Display *dpy = screen->dpy ();

    if (priv->active)
	return false;

    CompScreen::checkForError (dpy);

    XCompositeRedirectSubwindows (dpy, screen->root (),
				  CompositeRedirectManual);

    priv->overlayWindowCount = 0;

    if (CompScreen::checkForError (dpy))
    {
	compLogMessage ("composite", CompLogLevelError,
			"Another composite manager is already "
			"running on screen: %d", screen->screenNum ());

	return false;
    }

    foreach (CompWindow *w, screen->windows ())
    {
	CompositeWindow *cw = CompositeWindow::get (w);
	cw->priv->overlayWindow = false;
	cw->priv->redirected = true;
    }

    priv->pHnd = pHnd;
    priv->active = true;

    showOutputWindow ();

    priv->paintTimer.start
	(boost::bind (&CompositeScreen::handlePaintTimeout, this),
	 priv->optimalRedrawTime);

    return true;
}

void
CompositeScreen::unregisterPaintHandler ()
{
    Display *dpy = screen->dpy ();

    foreach (CompWindow *w, screen->windows ())
    {
	CompositeWindow *cw = CompositeWindow::get (w);
	cw->priv->overlayWindow = false;
	cw->priv->redirected = false;
	cw->release ();
    }

    priv->overlayWindowCount = 0;

    XCompositeUnredirectSubwindows (dpy, screen->root (),
				    CompositeRedirectManual);

    priv->pHnd = NULL;
    priv->active = false;
    priv->paintTimer.stop ();

    hideOutputWindow ();
}

bool
CompositeScreen::compositingActive ()
{
    return priv->active;
}

void
CompositeScreen::damageScreen ()
{
    if (priv->damageMask == 0)
	priv->paintTimer.setTimes (priv->paintTimer.minLeft ());

    priv->damageMask |= COMPOSITE_SCREEN_DAMAGE_ALL_MASK;
    priv->damageMask &= ~COMPOSITE_SCREEN_DAMAGE_REGION_MASK;
}

void
CompositeScreen::damageRegion (const CompRegion &region)
{
    if (priv->damageMask & COMPOSITE_SCREEN_DAMAGE_ALL_MASK)
	return;

    if (priv->damageMask == 0)
	priv->paintTimer.setTimes (priv->paintTimer.minLeft ());

    priv->damage += region;
    priv->damageMask |= COMPOSITE_SCREEN_DAMAGE_REGION_MASK;

    /* if the number of damage rectangles grows two much between repaints,
       we have a lot of overhead just for doing the damage tracking -
       in order to make sure we're not having too much overhead, damage
       the whole screen if we have a lot of damage rects */

    if (priv->damage.numRects () > 100)
       damageScreen ();
}

void
CompositeScreen::damagePending ()
{
    if (priv->damageMask == 0)
	priv->paintTimer.setTimes (priv->paintTimer.minLeft ());

    priv->damageMask |= COMPOSITE_SCREEN_DAMAGE_PENDING_MASK;
}

unsigned int
CompositeScreen::damageMask ()
{
    return priv->damageMask;
}

void
CompositeScreen::showOutputWindow ()
{
#ifdef USE_COW
    if (useCow && priv->active)
    {
	Display       *dpy = screen->dpy ();
	XserverRegion region;

	region = XFixesCreateRegion (dpy, NULL, 0);

	XFixesSetWindowShapeRegion (dpy,
				    priv->output,
				    ShapeBounding,
				    0, 0, 0);
	XFixesSetWindowShapeRegion (dpy,
				    priv->output,
				    ShapeInput,
				    0, 0, region);

	XFixesDestroyRegion (dpy, region);

	damageScreen ();
    }
#endif

}

void
CompositeScreen::hideOutputWindow ()
{
#ifdef USE_COW
    if (useCow)
    {
	Display       *dpy = screen->dpy ();
	XserverRegion region;

	region = XFixesCreateRegion (dpy, NULL, 0);

	XFixesSetWindowShapeRegion (dpy,
				    priv->output,
				    ShapeBounding,
				    0, 0, region);

	XFixesDestroyRegion (dpy, region);
    }
#endif

}

void
CompositeScreen::updateOutputWindow ()
{
#ifdef USE_COW
    if (useCow && priv->active)
    {
	Display       *dpy = screen->dpy ();
	XserverRegion region;
	CompRegion    tmpRegion (screen->region ());

	for (CompWindowList::reverse_iterator rit =
	     screen->windows ().rbegin ();
	     rit != screen->windows ().rend (); rit++)
	    if (CompositeWindow::get (*rit)->overlayWindow ())
	    {
		tmpRegion -= (*rit)->region ();
	    }

	XShapeCombineRegion (dpy, priv->output, ShapeBounding,
			     0, 0, tmpRegion.handle (), ShapeSet);


	region = XFixesCreateRegion (dpy, NULL, 0);

	XFixesSetWindowShapeRegion (dpy,
				    priv->output,
				    ShapeInput,
				    0, 0, region);

	XFixesDestroyRegion (dpy, region);
    }
#endif

}

void
PrivateCompositeScreen::makeOutputWindow ()
{
#ifdef USE_COW
    if (useCow)
    {
	overlay = XCompositeGetOverlayWindow (screen->dpy (), screen->root ());
	output  = overlay;

	XSelectInput (screen->dpy (), output, ExposureMask);
    }
    else
#endif
	output = overlay = screen->root ();

    cScreen->hideOutputWindow ();
}

Window
CompositeScreen::output ()
{
    return priv->output;
}

Window
CompositeScreen::overlay ()
{
    return priv->overlay;
}

int &
CompositeScreen::overlayWindowCount ()
{
    return priv->overlayWindowCount;
}

void
CompositeScreen::setWindowPaintOffset (int x, int y)
{
    priv->windowPaintOffset = CompPoint (x, y);
}

CompPoint
CompositeScreen::windowPaintOffset ()
{
    return priv->windowPaintOffset;
}

void
PrivateCompositeScreen::detectRefreshRate ()
{
    if (!noDetection &&
	optionGetDetectRefreshRate ())
    {
	CompString        name;
	CompOption::Value value;

	value.set ((int) 0);

	if (screen->XRandr ())
	{
	    XRRScreenConfiguration *config;

	    config  = XRRGetScreenInfo (screen->dpy (),
					screen->root ());
	    value.set ((int) XRRConfigCurrentRate (config));

	    XRRFreeScreenConfigInfo (config);
	}

	if (value.i () == 0)
	    value.set ((int) 50);

	mOptions[CompositeOptions::DetectRefreshRate].value ().set (false);
	screen->setOptionForPlugin ("composite", "refresh_rate", value);
	mOptions[CompositeOptions::DetectRefreshRate].value ().set (true);
    }
    else
    {
	redrawTime = 1000 / optionGetRefreshRate ();
	optimalRedrawTime = redrawTime;
    }
}

CompositeFPSLimiterMode
CompositeScreen::FPSLimiterMode ()
{
    return priv->FPSLimiterMode;
}

void
CompositeScreen::setFPSLimiterMode (CompositeFPSLimiterMode newMode)
{
    priv->FPSLimiterMode = newMode;
}

int
PrivateCompositeScreen::getTimeToNextRedraw (struct timeval *tv)
{
    int diff;

    diff = TIMEVALDIFF (tv, &lastRedraw);

    /* handle clock rollback */
    if (diff < 0)
	diff = 0;
    
    bool hasVSyncBehavior =
	(FPSLimiterMode == CompositeFPSLimiterModeVSyncLike ||
	 (pHnd && pHnd->hasVSync ()));

    if (idle || hasVSyncBehavior)
    {
	if (timeMult > 1)
	{
	    frameStatus = -1;
	    redrawTime = optimalRedrawTime;
	    timeMult--;
	}
    }
    else
    {
	int next;
	if (diff > redrawTime)
	{
	    if (frameStatus > 0)
		frameStatus = 0;

	    next = optimalRedrawTime * (timeMult + 1);
	    if (diff > next)
	    {
		frameStatus--;
		if (frameStatus < -1)
		{
		    timeMult++;
		    redrawTime = diff = next;
		}
	    }
	}
	else if (diff < redrawTime)
	{
	    if (frameStatus < 0)
		frameStatus = 0;

	    if (timeMult > 1)
	    {
		next = optimalRedrawTime * (timeMult - 1);
		if (diff < next)
		{
		    frameStatus++;
		    if (frameStatus > 4)
		    {
			timeMult--;
			redrawTime = next;
		    }
		}
	    }
	}
    }
    
    if (diff >= redrawTime)
	return 1;

    if (hasVSyncBehavior)
	return (redrawTime - diff) * 0.7;

    return redrawTime - diff;
}

int
CompositeScreen::redrawTime ()
{
    return priv->redrawTime;
}

int
CompositeScreen::optimalRedrawTime ()
{
    return priv->optimalRedrawTime;
}

bool
CompositeScreen::handlePaintTimeout ()
{
    struct      timeval tv;
    int         timeToNextRedraw;

    gettimeofday (&tv, 0);

    if (priv->damageMask)
    {
	int         timeDiff;

	if (priv->pHnd)
	    priv->pHnd->prepareDrawing ();

	timeDiff = TIMEVALDIFF (&tv, &priv->lastRedraw);

	/* handle clock rollback */
	if (timeDiff < 0)
	    timeDiff = 0;

	if (priv->slowAnimations)
	{
	    int msSinceLastPaint;

	    if (priv->FPSLimiterMode == CompositeFPSLimiterModeDisabled)
		msSinceLastPaint = 1;
	    else
		msSinceLastPaint =
		    priv->idle ? 2 : (timeDiff * 2) / priv->redrawTime;

	    preparePaint (msSinceLastPaint);
	}
	else
	    preparePaint (priv->idle ? priv->redrawTime : timeDiff);

	/* substract top most overlay window region */
	if (priv->overlayWindowCount)
	{
	    for (CompWindowList::reverse_iterator rit =
		 screen->windows ().rbegin ();
	         rit != screen->windows ().rend (); rit++)
	    {
		CompWindow *w = (*rit);

		if (w->destroyed () || w->invisible ())
		    continue;

		if (!CompositeWindow::get (w)->redirected ())
		    priv->damage -= w->region ();

		break;
	    }

	    if (priv->damageMask & COMPOSITE_SCREEN_DAMAGE_ALL_MASK)
	    {
		priv->damageMask &= ~COMPOSITE_SCREEN_DAMAGE_ALL_MASK;
		priv->damageMask |= COMPOSITE_SCREEN_DAMAGE_REGION_MASK;
	    }
	}

	priv->tmpRegion = priv->damage & screen->region ();

	if (priv->damageMask & COMPOSITE_SCREEN_DAMAGE_REGION_MASK)
	{
	    if (priv->tmpRegion == screen->region ())
		damageScreen ();
	}

	priv->damage = CompRegion ();

	int mask = priv->damageMask;
	priv->damageMask = 0;

	CompOutput::ptrList outputs (0);

	if (priv->optionGetForceIndependentOutputPainting ()
	    || !screen->hasOverlappingOutputs ())
	{
	    foreach (CompOutput &o, screen->outputDevs ())
		outputs.push_back (&o);
	}
	else
	    outputs.push_back (&screen->fullscreenOutput ());

	paint (outputs, mask);


	donePaint ();

	foreach (CompWindow *w, screen->windows ())
	{
	    if (w->destroyed ())
	    {
		CompositeWindow::get (w)->addDamage ();
		break;
	    }
	}

	priv->idle = false;
    }
    else
    {
	priv->idle = true;
    }

    priv->lastRedraw = tv;
    gettimeofday (&tv, 0);

    if (priv->FPSLimiterMode == CompositeFPSLimiterModeDisabled)
    {
	const int msToReturn1After = 100;

	priv->frameTimeAccumulator += priv->redrawTime;
	if (priv->frameTimeAccumulator > msToReturn1After)
	{
	    priv->frameTimeAccumulator %= msToReturn1After;
	    timeToNextRedraw = 1;
	}
	else
	    timeToNextRedraw = 0;
    }
    else
	timeToNextRedraw = priv->getTimeToNextRedraw (&tv);

    if (priv->idle)
	priv->paintTimer.setTimes (timeToNextRedraw, MAXSHORT);
    else
	priv->paintTimer.setTimes (timeToNextRedraw);

    return true;
}

void
CompositeScreen::preparePaint (int msSinceLastPaint)
    WRAPABLE_HND_FUNC (0, preparePaint, msSinceLastPaint)

void
CompositeScreen::donePaint ()
    WRAPABLE_HND_FUNC (1, donePaint)

void
CompositeScreen::paint (CompOutput::ptrList &outputs,
		        unsigned int        mask)
{
    WRAPABLE_HND_FUNC (2, paint, outputs, mask)

    if (priv->pHnd)
	priv->pHnd->paintOutputs (outputs, mask, priv->tmpRegion);
}

const CompWindowList &
CompositeScreen::getWindowPaintList ()
{
    WRAPABLE_HND_FUNC_RETURN (3, const CompWindowList &, getWindowPaintList)

    return screen->windows ();
}

void
PrivateCompositeScreen::handleExposeEvent (XExposeEvent *event)
{
    if (output == event->window)
	return;

    exposeRects.push_back (CompRect (event->x,
				     event->y,
				     event->width,
				     event->height));

    if (event->count == 0)
    {
	CompRect rect;
	foreach (CompRect rect, exposeRects)
	{
	    cScreen->damageRegion (CompRegion (rect));
	}
	exposeRects.clear ();
    }
}

void
PrivateCompositeScreen::outputChangeNotify ()
{
    screen->outputChangeNotify ();
#ifdef USE_COW
    if (useCow)
	XMoveResizeWindow (screen->dpy (), overlay, 0, 0,
			   screen->width (), screen->height ());
#endif
    cScreen->damageScreen ();
}

bool
CompositeScreen::toggleSlowAnimations (CompAction         *action,
				       CompAction::State  state,
				       CompOption::Vector &options)
{
    CompositeScreen *cs = CompositeScreen::get (screen);
    if (cs)
	cs->priv->slowAnimations = !cs->priv->slowAnimations;

    return true;
}


void
CompositeScreenInterface::preparePaint (int msSinceLastPaint)
    WRAPABLE_DEF (preparePaint, msSinceLastPaint)

void
CompositeScreenInterface::donePaint ()
    WRAPABLE_DEF (donePaint)

void
CompositeScreenInterface::paint (CompOutput::ptrList &outputs,
				 unsigned int        mask)
    WRAPABLE_DEF (paint, outputs, mask)

const CompWindowList &
CompositeScreenInterface::getWindowPaintList ()
    WRAPABLE_DEF (getWindowPaintList)

const CompRegion &
CompositeScreen::currentDamage () const
{
    return priv->damage;
}
