/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2005 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "place.h"

COMPIZ_PLUGIN_20090315 (place, PlacePluginVTable)

PlaceScreen::PlaceScreen (CompScreen *screen) :
    PluginClassHandler<PlaceScreen, CompScreen> (screen),
    mPrevSize (screen->width (), screen->height ()),
    mStrutWindowCount (0),
    fullPlacementAtom (XInternAtom (screen->dpy (),
    				    "_NET_WM_FULL_PLACEMENT", 0))
{
    ScreenInterface::setHandler (screen);
    mResChangeFallbackHandle.setTimes (4000, 4500); /* 4 Seconds */

    screen->updateSupportedWmHints ();
}

PlaceScreen::~PlaceScreen ()
{
    screen->addSupportedAtomsSetEnabled (this, false);

    mResChangeFallbackHandle.stop ();
    screen->updateSupportedWmHints ();
}

void
PlaceScreen::doHandleScreenSizeChange (bool firstPass,
				       int  newWidth,
				       int  newHeight)
{
    int            vpX, vpY, shiftX, shiftY;
    CompRect       extents;
    XWindowChanges xwc;
    CompRect       vpRelRect, winRect, workArea;
    int		   pivotX, pivotY;
    unsigned int   mask;
    int		   curVpOffsetX = screen->vp ().x () * screen->width ();
    int		   curVpOffsetY = screen->vp ().y () * screen->height ();

    if (firstPass)
	mStrutWindowCount = 0;
    else
	if (mResChangeFallbackHandle.active ())
	{
	    mResChangeFallbackHandle.stop ();
	}

    foreach (CompWindow *w, screen->windows ())
    {
	if (!w->managed ())
	    continue;

	PLACE_WINDOW (w);

	if (firstPass)
	{
	    /* count the windows that have struts */
	    if (w->struts ())
		mStrutWindowCount++;

	    /* for maximized/fullscreen windows, keep window coords before
	     * screen resize, as they are sometimes automaticall changed
	     * before the 2nd pass */

	    if (w->type () & CompWindowTypeFullscreenMask ||
		(w->state () & (CompWindowStateMaximizedVertMask |
			        CompWindowStateMaximizedHorzMask)))
	    {
		pw->mPrevServer.set (w->serverX (), w->serverY ());
	    }
	}

	if (w->wmType () & (CompWindowTypeDockMask |
			    CompWindowTypeDesktopMask))
	{
	    continue;
	}

	/* Also in the first pass, we save the rectangle of those windows that
	 * don't already have a saved one. So, skip those tat do. */

	if (firstPass && pw->mSavedOriginal)
	    continue;

	winRect = ((CompRect) w->serverGeometry ());


	pivotX = winRect.x ();
	pivotY = winRect.y ();

	if (w->type () & CompWindowTypeFullscreenMask ||
	    (w->state () & (CompWindowStateMaximizedVertMask |
	    		    CompWindowStateMaximizedHorzMask)))
	{
	    if (w->saveMask () & CWX)
		winRect.setX (w->saveWc ().x);

	    if (w->saveMask () & CWY)
		winRect.setY (w->saveWc ().y);

	    if (w->saveMask () & CWWidth)
		winRect.setWidth (w->saveWc ().width);

	    if (w->saveMask () & CWHeight)
		winRect.setHeight (w->saveWc ().height);

	    pivotX = pw->mPrevServer.x ();
	    pivotY = pw->mPrevServer.y ();
	}

	/* calculate target vp x, y index for window's pivot point */
	vpX = pivotX / newWidth;
	if (pivotX < 0)
	    vpX -= 1;
	vpY = pivotY / newHeight;
	if (pivotY < 0)
	    vpY -= 1;

	/* if window's target vp is to the left of the leftmost viewport on that
	   row, assign its target vp column as 0 (-s->x rel. to current vp) */
	if (screen->vp ().x () + vpX < 0)
	    vpX = -screen->vp ().x ();

	/* if window's target vp is above the topmost viewport on that column,
	   assign its target vp row as 0 (-s->y rel. to current vp) */
	if (screen->vp ().y () + vpY < 0)
	    vpY = -screen->vp ().y ();

	if (pw->mSavedOriginal)
	{
	    /* set position/size to saved original rectangle */
	    vpRelRect = pw->mOrigVpRelRect;

	    xwc.x = pw->mOrigVpRelRect.x () + vpX * screen->width ();
	    xwc.y = pw->mOrigVpRelRect.y () + vpY * screen->height ();
	}
	else
	{
	    /* set position/size to window's current rectangle
	       (with position relative to target viewport) */
	    vpRelRect.setX (winRect.x () - vpX * mPrevSize.width ());
	    vpRelRect.setY (winRect.y () - vpY * mPrevSize.height ());
	    vpRelRect.setWidth (winRect.width ());
	    vpRelRect.setHeight (winRect.height ());

	    xwc.x = winRect.x ();
	    xwc.y = winRect.y ();

	    shiftX = vpX * (newWidth - screen->width ());
	    shiftY = vpY * (newWidth - screen->height ());

	    /* if coords. relative to viewport are outside new viewport area,
	       shift window left/up so that it falls inside */
	    if (vpRelRect.x () >= screen->width ())
		shiftX -= vpRelRect.x () - (screen->width () - 1);
	    if (vpRelRect.y () >= screen->height ())
		shiftY -= vpRelRect.y () - (screen->height () - 1);

	    if (shiftX)
		xwc.x += shiftX;

	    if (shiftY)
		xwc.y += shiftY;
	}

	mask       = CWX | CWY | CWWidth | CWHeight;
	xwc.width  = vpRelRect.width ();
	xwc.height = vpRelRect.height ();

	/* Handle non-(0,0) current viewport by shifting by curVpOffsetX,Y,
	   and bring window to (0,0) by shifting by minus its vp offset */

	xwc.x += curVpOffsetX - (screen->vp ().x () + vpX) * screen->width ();
	xwc.y += curVpOffsetY - (screen->vp ().y () + vpY) * screen->height ();

	workArea =
	    pw->doValidateResizeRequest (mask, &xwc, FALSE, FALSE);

	xwc.x -= curVpOffsetX - (screen->vp ().x () + vpX) * screen->width ();
	xwc.y -= curVpOffsetY - (screen->vp ().y () + vpY) * screen->height ();

	/* Check if the new coordinates are different than current position and
	   size. If not, we can clear the corresponding mask bits. */
	if (xwc.x == winRect.x ())
	    mask &= ~CWX;

	if (xwc.y == winRect.y ())
	    mask &= ~CWY;

	if (xwc.width == winRect.width ())
	    mask &= ~CWWidth;

	if (xwc.height == winRect.height ())
	    mask &= ~CWHeight;

	if (!pw->mSavedOriginal)
	{
	    if (mask)
	    {
		/* save window geometry (relative to viewport) so that it
		can be restored later */
		pw->mSavedOriginal = TRUE;
		pw->mOrigVpRelRect = vpRelRect;
	    }
	}
	else if (pw->mOrigVpRelRect.x () + vpX * newWidth == xwc.x &&
		 pw->mOrigVpRelRect.y () + vpY * newHeight == xwc.y &&
		 pw->mOrigVpRelRect.width ()  == xwc.width &&
		 pw->mOrigVpRelRect.height () == xwc.height)
	{
	    /* if size and position is back to original, clear saved rect */
	    pw->mSavedOriginal = FALSE;
	}

	if (firstPass) /* if first pass, don't actually move the window */
	    continue;

	/* for maximized/fullscreen windows, update saved pos/size */
	if (w->type () & CompWindowTypeFullscreenMask ||
	    (w->state () & (CompWindowStateMaximizedVertMask |
			 CompWindowStateMaximizedHorzMask)))
	{
	    if (mask & CWX)
	    {
		w->saveWc ().x = xwc.x;
		w->saveMask () |= CWX;
	    }
	    if (mask & CWY)
	    {
		w->saveWc ().y = xwc.y;
		w->saveMask () |= CWY;
	    }
	    if (mask & CWWidth)
	    {
		w->saveWc ().width = xwc.width;
		w->saveMask () |= CWWidth;
	    }
	    if (mask & CWHeight)
	    {
		w->saveWc ().height = xwc.height;
		w->saveMask () |= CWHeight;
	    }

	    if (w->type () & CompWindowTypeFullscreenMask)
	    {
		mask |= CWX | CWY | CWWidth | CWHeight;
		xwc.x = vpX * screen->width ();
		xwc.y = vpY * screen->height ();
		xwc.width  = screen->width ();
		xwc.height = screen->height ();
	    }
	    else
	    {
		if (w->state () & CompWindowStateMaximizedHorzMask)
		{
		    mask |= CWX | CWWidth;
		    xwc.x = vpX * screen->width () + workArea.x () + w->input ().left;
		    xwc.width = workArea.width () -
			(2 * w->serverGeometry ().border () +
			 w->input ().left + w->input ().right);
		}
		if (w->state () & CompWindowStateMaximizedVertMask)
		{
		    mask |= CWY | CWHeight;
		    xwc.y = vpY * screen->height () + workArea.y () + w->input ().top;
		    xwc.height = workArea.height () -
			(2 * w->serverGeometry ().border () +
			 w->input ().top + w->input ().bottom);
		}
	    }
	}
	if (mask)
	{
	    /* actually move/resize window in directions given by mask */
	    w->configureXWindow (mask, &xwc);
	}
    }
}

bool
PlaceScreen::handleScreenSizeChangeFallback (int width,
					     int height)
{
    /* If countdown is not finished yet (i.e. at least one struct window didn't
     * update its struts), reset the count down and do the 2nd pass here */

    if (mStrutWindowCount > 0) /* no windows with struts found */
    {
	mStrutWindowCount = 0;
	doHandleScreenSizeChange (false, width, height);
    }

    return false;
}

void
PlaceScreen::handleScreenSizeChange (int width,
				     int height)
{
    CompRect       extents;

    if (screen->width () == width && screen->height () == height)
	return;

    mPrevSize.setWidth (screen->width ());
    mPrevSize.setHeight (screen->height ());

    if (mResChangeFallbackHandle.active ())
	mResChangeFallbackHandle.stop ();

    doHandleScreenSizeChange (true, width, height);

    if (mStrutWindowCount == 0) /* no windows with struts found */
    {
	mResChangeFallbackHandle.stop ();
	/* do the 2nd pass right here instead of handleEvent */

	doHandleScreenSizeChange (false, width, height);
    }
    else
    {
        mResChangeFallbackHandle.setCallback (
        	      boost::bind (&PlaceScreen::handleScreenSizeChangeFallback,
        		           this, width, height));
	mResChangeFallbackHandle.start ();
    }
}

void
PlaceScreen::handleEvent (XEvent *event)
{
    switch (event->type)
    {
	case ConfigureNotify:
	    {

		if (event->type == ConfigureNotify &&
		event->xconfigure.window == screen->root ())
		{
		    handleScreenSizeChange (event->xconfigure.width,
					    event->xconfigure.height);
		}
	    }
	    break;
	case PropertyNotify:
	    if (event->xproperty.atom == Atoms::wmStrut ||
	        event->xproperty.atom == Atoms::wmStrutPartial)
	    {
	        CompWindow *w;

	        w = screen->findWindow (event->xproperty.window);
	        if (w)
	        {
		    /* Only do when handling screen size change.
		       ps->strutWindowCount is 0 at any other time */
		    if (mStrutWindowCount > 0 &&
		        w->updateStruts ())
		    {
		        mStrutWindowCount--;
		        screen->updateWorkarea ();

		        /* if this was the last window with struts */
		        if (!mStrutWindowCount)
			    doHandleScreenSizeChange (false, screen->width (),
			    				     screen->height ()); /* 2nd pass */
		    }
	        }
	    }
    }
    screen->handleEvent (event);
}

/* sort functions */

static bool
compareLeftmost (CompWindow *a,
		 CompWindow *b)
{
    int ax, bx;

    ax = a->serverX () - a->input ().left;
    bx = b->serverX () - b->input ().left;

    return (ax <= bx);
}

static bool
compareTopmost (CompWindow *a,
		CompWindow *b)
{
    int ay, by;

    ay = a->serverY () - a->input ().top;
    by = b->serverY () - b->input ().top;

    return (ay <= by);
}

static bool
compareNorthWestCorner (CompWindow *a,
			CompWindow *b)
{
    int fromOriginA;
    int fromOriginB;
    int ax, ay, bx, by;

    ax = a->serverX () - a->input ().left;
    ay = a->serverY () - a->input ().top;

    bx = b->serverX () - b->input ().left;
    by = b->serverY () - b->input ().top;

    /* probably there's a fast good-enough-guess we could use here. */
    fromOriginA = sqrt (ax * ax + ay * ay);
    fromOriginB = sqrt (bx * bx + by * by);

    return (fromOriginA <= fromOriginB);
}

PlaceWindow::PlaceWindow (CompWindow *w) :
    PluginClassHandler<PlaceWindow, CompWindow> (w),
    mSavedOriginal (false),
    window (w),
    ps (PlaceScreen::get (screen))
{
    WindowInterface::setHandler (w);
}

PlaceWindow::~PlaceWindow ()
{
}

bool
PlaceWindow::place (CompPoint &pos)
{
    bool      status = window->place (pos);
    CompPoint viewport;

    if (status)
	return status;

    doPlacement (pos);
    if (matchViewport (viewport))
    {
	int x, y;

	viewport.setX (MAX (MIN (viewport.x (),
				 screen->vpSize ().width () - 1), 0));
	viewport.setY (MAX (MIN (viewport.y (),
				 screen->vpSize ().height () - 1), 0));

	x = pos.x () % screen->width ();
	if (x < 0)
	    x += screen->width ();
	y = pos.y () % screen->height ();
	if (y < 0)
	    y += screen->height ();

	pos.setX (x + (viewport.x () - screen->vp ().x ()) * screen->width ());
	pos.setY (y + (viewport.y () - screen->vp ().y ()) * screen->height ());
    }

    return true;
}

CompRect
PlaceWindow::doValidateResizeRequest (unsigned int &mask,
				      XWindowChanges *xwc,
				      unsigned int source,
				      bool	   clampToViewport)
{
    CompRect workArea;
    int	     x, y, left, right, bottom, top;
    CompWindow::Geometry geom;
    int      output;
    bool     sizeOnly = true;

    if (clampToViewport)
    {
	/* left, right, top, bottom target coordinates, clamed to viewport
	 * sizes as we don't need to validate movements to other viewports;
	 * we are only interested in inner-viewport movements */

	x = xwc->x % screen->width ();
	if ((x + xwc->width) < 0)
	    x += screen->width ();

	y = xwc->y % screen->height ();
	if ((y + xwc->height) < 0)
	    y += screen->height ();
    }
    else
    {
	x = xwc->x;
	y = xwc->y;
    }

    left   = x - window->input ().left;
    right  = left + xwc->width +  (window->input ().left +
    				   window->input ().right +
				   2 * window->serverGeometry ().border ());;
    top    = y - window->input ().top;
    bottom = top + xwc->height + (window->input ().top +
    				  window->input ().bottom +
				  2 * window->serverGeometry ().border ());;

    geom.set (xwc->x, xwc->y, xwc->width, xwc->height,
	      window->serverGeometry ().border ());
    output   = screen->outputDeviceForGeometry (geom);
    workArea = screen->getWorkareaForOutput (output);

    if (clampToViewport &&
    	xwc->width >= workArea.width () &&
	xwc->height >= workArea.height ())
    {
	if ((window->actions () & MAXIMIZE_STATE) == MAXIMIZE_STATE &&
	    (window->mwmDecor () & (MwmDecorAll | MwmDecorTitle))   &&
	    !(window->state () & CompWindowStateFullscreenMask))
	{
	    sendMaximizationRequest ();
	}
    }

    if ((right - left) > workArea.width ())
    {
	left  = workArea.left ();
	right = workArea.right ();
    }
    else
    {
	if (left < workArea.left ())
	{
	    right += workArea.left () - left;
	    left  = workArea.left ();
	}

	if (right > workArea.right ())
	{
	    left -= right - workArea.right ();
	    right = workArea.right ();
	}
    }

    if ((bottom - top) > workArea.height ())
    {
	top    = workArea.top ();
	bottom = workArea.bottom ();
    }
    else
    {
	if (top < workArea.top ())
	{
	    bottom += workArea.top () - top;
	    top    = workArea.top ();
	}

	if (bottom > workArea.bottom ())
	{
	    top   -= bottom - workArea.bottom ();
	    bottom = workArea.bottom ();
	}
    }

    /* bring left/right/top/bottom to actual window coordinates */
    left   += window->input ().left;
    right  -= window->input ().right + 2 * window->serverGeometry ().border ();
    top    += window->input ().top;
    bottom -= window->input ().bottom + 2 * window->serverGeometry ().border ();

    /* always validate position if the applicaiton changed only its size,
     * as it might become partially offscreen because of that */
    if (!(mask) & (CWX | CWY) && (mask & (CWWidth | CWHeight)))
	sizeOnly = false;

    if ((right - left) != xwc->width)
    {
	xwc->width = right - left;
	mask       |= CWWidth;
	sizeOnly   = false;
    }

    if ((bottom - top) != xwc->height)
    {
	xwc->height = bottom - top;
	mask        |= CWHeight;
	sizeOnly    = false;
    }

    if (!sizeOnly)
    {
	if (left != x)
	{
	    xwc->x += left - x;
	    mask   |= CWX;
	}

	if (top != y)
	{
	    xwc->y += top - y;
	    mask   |= CWY;
	}
    }

    return workArea;
}

void
PlaceWindow::validateResizeRequest (unsigned int   &mask,
				    XWindowChanges *xwc,
				    unsigned int   source)
{
    CompRect             workArea;
    CompWindow::Geometry geom;
    bool                 sizeOnly = false;

    window->validateResizeRequest (mask, xwc, source);

    if (!mask)
	return;

    if (source == ClientTypePager)
	return;

    if (window->state () & CompWindowStateFullscreenMask)
	return;

    if (window->wmType () & (CompWindowTypeDockMask |
			     CompWindowTypeDesktopMask))
	return;

    /* do nothing if the window was already (at least partially) offscreen */
    if (window->serverX () < 0                         ||
	window->serverX () + window->serverWidth () > screen->width () ||
	window->serverY () < 0                         ||
	window->serverY () + window->serverHeight () > screen->height ())
    {
	return;
    }

    if (hasUserDefinedPosition (false))
	/* try to keep the window position intact for USPosition -
	   obviously we can't do that if we need to change the size */
	sizeOnly = true;

    doValidateResizeRequest (mask, xwc, sizeOnly, true);

}

void
PlaceScreen::addSupportedAtoms (std::vector<Atom> &atoms)
{

    atoms.push_back (fullPlacementAtom);

    screen->addSupportedAtoms (atoms);
}

void
PlaceWindow::doPlacement (CompPoint &pos)
{
    CompRect          workArea;
    CompPoint         targetVp;
    PlacementStrategy strategy;
    bool              keepInWorkarea;
    int		      mode;

    if (matchPosition (pos, keepInWorkarea))
    {
	strategy = keepInWorkarea ? ConstrainOnly : NoPlacement;
    }
    else
    {
	strategy = getStrategy ();
	if (strategy == NoPlacement)
	    return;
    }

    mode = getPlacementMode ();
    const CompOutput &output = getPlacementOutput (mode, strategy, pos);
    workArea = output.workArea ();

    targetVp = window->initialViewport ();

    if (strategy == PlaceOverParent)
    {
	CompWindow *parent;

	parent = screen->findWindow (window->transientFor ());
	if (parent)
	{
	    /* center over parent horizontally */
	    pos.setX (parent->serverX () +
		      (parent->serverGeometry ().width () / 2) -
		      (window->serverGeometry ().width () / 2));

	    /* "visually" center vertically, leaving twice as much space below
	       as on top */
	    pos.setY (parent->serverY () +
		      (parent->serverGeometry ().height () -
		       window->serverGeometry ().height ()) / 3);

	    /* if parent is visible on current viewport, clip to work area;
	       don't constrain further otherwise */
	    if (parent->serverX () < screen->width ()           &&
		parent->serverX () + parent->serverWidth () > 0 &&
		parent->serverY () < screen->height ()          &&
		parent->serverY () + parent->serverHeight () > 0)
	    {
		targetVp = parent->defaultViewport ();
		strategy = ConstrainOnly;
	    }
	    else
	    {
		strategy = NoPlacement;
	    }
	}
    }

    if (strategy == PlaceCenteredOnScreen)
    {
	/* center window on current output device */

	pos.setX (output.x () +
		  (output.width () - window->serverGeometry ().width ()) /2);
	pos.setY (output.y () +
		  (output.height () - window->serverGeometry ().height ()) / 2);

	strategy = ConstrainOnly;
    }

    workArea.setX (workArea.x () +
                   (targetVp.x () - screen->vp ().x ()) * screen->width ());
    workArea.setY (workArea.y () +
                   (targetVp.y () - screen->vp ().y ()) * screen->height ());

    if (strategy == PlaceOnly || strategy == PlaceAndConstrain)
    {
	switch (mode) {
	    case PlaceOptions::ModeCascade:
	    placeCascade (workArea, pos);
	    break;
	case PlaceOptions::ModeCentered:
	    placeCentered (workArea, pos);
	    break;
	case PlaceOptions::ModeRandom:
	    placeRandom (workArea, pos);
	    break;
	case PlaceOptions::ModePointer:
	    placePointer (workArea, pos);
	    break;
	case PlaceOptions::ModeMaximize:
	    sendMaximizationRequest ();
	    break;
	case PlaceOptions::ModeSmart:
	    placeSmart (workArea, pos);
	    break;
	}

	/* When placing to the fullscreen output, constrain to one
	   output nevertheless */
	if ((unsigned int) output.id () == (unsigned int) ~0)
	{
	    int                  id;
	    CompWindow::Geometry geom (window->serverGeometry ());

	    geom.setPos (pos);

	    id       = screen->outputDeviceForGeometry (geom);
	    workArea = screen->getWorkareaForOutput (id);

	    workArea.setX (workArea.x () +
	                   (targetVp.x () - screen->vp ().x ()) *
			   screen->width ());
	    workArea.setY (workArea.y () +
	                   (targetVp.y () - screen->vp ().y ()) *
			   screen->height ());
	}

	/* Maximize windows if they are too big for their work area (bit of
	 * a hack here). Assume undecorated windows probably don't intend to
	 * be maximized.
	 */
	if ((window->actions () & MAXIMIZE_STATE) == MAXIMIZE_STATE &&
	    (window->mwmDecor () & (MwmDecorAll | MwmDecorTitle))   &&
	    !(window->state () & CompWindowStateFullscreenMask))
	{
	    if (window->serverWidth () >= workArea.width () &&
		window->serverHeight () >= workArea.height ())
	    {
		sendMaximizationRequest ();
	    }
	}
    }

    if (strategy == ConstrainOnly || strategy == PlaceAndConstrain)
	constrainToWorkarea (workArea, pos);
}

void
PlaceWindow::placeCascade (const CompRect &workArea,
			   CompPoint      &pos)
{
    CompWindowList windows;

    /* Find windows that matter (not minimized, on same workspace
     * as placed window, may be shaded - if shaded we pretend it isn't
     * for placement purposes)
     */
    foreach (CompWindow *w, screen->windows ())
    {
	if (!windowIsPlaceRelevant (w))
	    continue;

	if (w->type () & (CompWindowTypeFullscreenMask |
			  CompWindowTypeUnknownMask))
	    continue;

	if (w->serverX () >= workArea.right ()                              ||
	    w->serverX () + w->serverGeometry ().width () <= workArea.x  () ||
	    w->serverY () >= workArea.bottom ()                             ||
	    w->serverY () + w->serverGeometry ().height () <= workArea.y ())
	    continue;

	windows.push_back (w);
    }

    if (!cascadeFindFirstFit (windows, workArea, pos))
    {
	/* if the window wasn't placed at the origin of screen,
	 * cascade it onto the current screen
	 */
	cascadeFindNext (windows, workArea, pos);
    }
}

void
PlaceWindow::placeCentered (const CompRect &workArea,
			    CompPoint      &pos)
{
    pos.setX (workArea.x () +
	      (workArea.width () - window->serverGeometry ().width ()) / 2);
    pos.setY (workArea.y () +
	      (workArea.height () - window->serverGeometry ().height ()) / 2);
}

void
PlaceWindow::placeRandom (const CompRect &workArea,
			  CompPoint      &pos)
{
    int remainX, remainY;

    pos.setX (workArea.x ());
    pos.setY (workArea.y ());

    remainX = workArea.width () - window->serverGeometry ().width ();
    if (remainX > 0)
	pos.setX (pos.x () + (rand () % remainX));

    remainY = workArea.height () - window->serverGeometry ().height ();
    if (remainY > 0)
	pos.setY (pos.y () + (rand () % remainY));
}

void
PlaceWindow::placePointer (const CompRect &workArea,
			   CompPoint	  &pos)
{
    if (PlaceScreen::get (screen)->getPointerPosition (pos))
    {
	unsigned int dx = (window->serverGeometry ().width () / 2) -
			   window->serverGeometry ().border ();
	unsigned int dy = (window->serverGeometry ().height () / 2) -
			   window->serverGeometry ().border ();
	pos -= CompPoint (dx, dy);
    }
    else
	placeCentered (workArea, pos);
}


/* overlap types */
#define NONE    0
#define H_WRONG -1
#define W_WRONG -2

void
PlaceWindow::placeSmart (const CompRect &workArea,
			 CompPoint      &pos)
{
    /*
     * SmartPlacement by Cristian Tibirna (tibirna@kde.org)
     * adapted for kwm (16-19jan98) and for kwin (16Nov1999) using (with
     * permission) ideas from fvwm, authored by
     * Anthony Martin (amartin@engr.csulb.edu).
     * Xinerama supported added by Balaji Ramani (balaji@yablibli.com)
     * with ideas from xfce.
     * adapted for Compiz by Bellegarde Cedric (gnumdk(at)gmail.com)
     */
    int overlap, minOverlap = 0;
    int xOptimal, yOptimal;
    int possible;

    /* temp coords */
    int cxl, cxr, cyt, cyb;
    /* temp coords */
    int xl,  xr,  yt,  yb;
    /* temp holder */
    int basket;
    /* CT lame flag. Don't like it. What else would do? */
    bool firstPass = true;

    /* get the maximum allowed windows space */
    int xTmp = workArea.x ();
    int yTmp = workArea.y ();

    /* client gabarit */
    int cw = window->serverWidth () - 1;
    int ch = window->serverHeight () - 1;

    xOptimal = xTmp;
    yOptimal = yTmp;

    /* loop over possible positions */
    do
    {
	/* test if enough room in x and y directions */
	if (yTmp + ch > workArea.bottom () && ch < workArea.height ())
	    overlap = H_WRONG; /* this throws the algorithm to an exit */
	else if (xTmp + cw > workArea.right ())
	    overlap = W_WRONG;
	else
	{
	    overlap = NONE; /* initialize */

	    cxl = xTmp;
	    cxr = xTmp + cw;
	    cyt = yTmp;
	    cyb = yTmp + ch;

	    foreach (CompWindow *w, screen->windows ())
	    {
		if (!windowIsPlaceRelevant (w))
		    continue;

		xl = w->serverX () - w->input ().left;
		yt = w->serverY () - w->input ().top;
		xr = w->serverX () + w->serverWidth () +
		     w->input ().right +
		     w->serverGeometry ().border () * 2;
		yb = w->serverY () + w->serverHeight () +
		     w->input ().bottom +
		     w->serverGeometry ().border () * 2;

		/* if windows overlap, calc the overall overlapping */
		if (cxl < xr && cxr > xl && cyt < yb && cyb > yt)
		{
		    xl = MAX (cxl, xl);
		    xr = MIN (cxr, xr);
		    yt = MAX (cyt, yt);
		    yb = MIN (cyb, yb);

		    if (w->state () & CompWindowStateAboveMask)
			overlap += 16 * (xr - xl) * (yb - yt);
		    else if (w->state () & CompWindowStateBelowMask)
			overlap += 0;
		    else
			overlap += (xr - xl) * (yb - yt);
		}
	    }
	}

	/* CT first time we get no overlap we stop */
	if (overlap == NONE)
	{
	    xOptimal = xTmp;
	    yOptimal = yTmp;
	    break;
	}

	if (firstPass)
	{
	    firstPass  = false;
	    minOverlap = overlap;
	}
	/* CT save the best position and the minimum overlap up to now */
	else if (overlap >= NONE && overlap < minOverlap)
	{
	    minOverlap = overlap;
	    xOptimal = xTmp;
	    yOptimal = yTmp;
	}

	/* really need to loop? test if there's any overlap */
	if (overlap > NONE)
	{
	    possible = workArea.right ();

	    if (possible - cw > xTmp)
		possible -= cw;

	    /* compare to the position of each client on the same desk */
	    foreach (CompWindow *w, screen->windows ())
	    {
		if (!windowIsPlaceRelevant (w))
		    continue;

		xl = w->serverX () - w->input ().left;
		yt = w->serverY () - w->input ().top;
		xr = w->serverX () + w->serverWidth () +
		     w->input ().right +
		     w->serverGeometry ().border () * 2;
		yb = w->serverY () + w->serverHeight () +
		     w->input ().bottom +
		     w->serverGeometry ().border () * 2;

		/* if not enough room above or under the current
		 * client determine the first non-overlapped x position
		 */
		if (yTmp < yb && yt < ch + yTmp)
		{
		    if (xr > xTmp && possible > xr)
			possible = xr;

		    basket = xl - cw;
		    if (basket > xTmp && possible > basket)
			possible = basket;
		}
	    }
	    xTmp = possible;
	}
	/* else ==> not enough x dimension (overlap was wrong on horizontal) */
	else if (overlap == W_WRONG)
	{
	    xTmp     = workArea.x ();
	    possible = workArea.bottom ();

	    if (possible - ch > yTmp)
		possible -= ch;

	    /* test the position of each window on the desk */
	    foreach (CompWindow *w, screen->windows ())
	    {
		if (!windowIsPlaceRelevant (w))
		    continue;

		xl = w->serverX () - w->input ().left;
		yt = w->serverY () - w->input ().top;
		xr = w->serverX () + w->serverWidth () +
		     w->input ().right +
		     w->serverGeometry ().border () * 2;
		yb = w->serverY () + w->serverHeight () +
		     w->input ().bottom +
		     w->serverGeometry ().border () * 2;

		/* if not enough room to the left or right of the current
		 * client determine the first non-overlapped y position
		 */
		if (yb > yTmp && possible > yb)
		    possible = yb;

		basket = yt - ch;
		if (basket > yTmp && possible > basket)
		    possible = basket;
	    }
	    yTmp = possible;
	}
    }
    while (overlap != NONE && overlap != H_WRONG && yTmp < workArea.bottom ());

    if (ch >= workArea.height ())
	yOptimal = workArea.y ();

    pos.setX (xOptimal + window->input ().left);
    pos.setY (yOptimal + window->input ().top);
}

static void
centerTileRectInArea (CompRect       &rect,
		      const CompRect &workArea)
{
    int fluff;

    /* The point here is to tile a window such that "extra"
     * space is equal on either side (i.e. so a full screen
     * of windows tiled this way would center the windows
     * as a group)
     */

    fluff  = (workArea.width () % (rect.width () + 1)) / 2;
    rect.setX (workArea.x () + fluff);

    fluff  = (workArea.height () % (rect.height () + 1)) / 3;
    rect.setY (workArea.y () + fluff);
}

static bool
rectOverlapsWindow (const CompRect       &rect,
		    const CompWindowList &windows)
{
    CompRect dest;

    foreach (CompWindow *other, windows)
    {
	CompRect intersect;

	switch (other->type ()) {
	case CompWindowTypeDockMask:
	case CompWindowTypeSplashMask:
	case CompWindowTypeDesktopMask:
	case CompWindowTypeDialogMask:
	case CompWindowTypeModalDialogMask:
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeUnknownMask:
	    break;
	case CompWindowTypeNormalMask:
	case CompWindowTypeUtilMask:
	case CompWindowTypeToolbarMask:
	case CompWindowTypeMenuMask:
	    intersect = rect & other->serverInputRect ();
	    if (!intersect.isEmpty ())
		return true;
	    break;
	}
    }

    return false;
}

/* Find the leftmost, then topmost, empty area on the workspace
 * that can contain the new window.
 *
 * Cool feature to have: if we can't fit the current window size,
 * try shrinking the window (within geometry constraints). But
 * beware windows such as Emacs with no sane minimum size, we
 * don't want to create a 1x1 Emacs.
 */
bool
PlaceWindow::cascadeFindFirstFit (const CompWindowList &windows,
				  const CompRect       &workArea,
				  CompPoint            &pos)
{
    /* This algorithm is limited - it just brute-force tries
     * to fit the window in a small number of locations that are aligned
     * with existing windows. It tries to place the window on
     * the bottom of each existing window, and then to the right
     * of each existing window, aligned with the left/top of the
     * existing window in each of those cases.
     */
    bool           retval = false;
    CompWindowList belowSorted, rightSorted;
    CompRect       rect;

    /* Below each window */
    belowSorted = windows;
    belowSorted.sort (compareLeftmost);
    belowSorted.sort (compareTopmost);

    /* To the right of each window */
    rightSorted = windows;
    rightSorted.sort (compareTopmost);
    rightSorted.sort (compareLeftmost);

    rect = window->serverInputRect ();
    centerTileRectInArea (rect, workArea);

    if (workArea.contains (rect) && !rectOverlapsWindow (rect, windows))
    {
	pos.setX (rect.x () + window->input ().left);
	pos.setY (rect.y () + window->input ().top);
	retval = true;
    }

    if (!retval)
    {
	/* try below each window */
	foreach (CompWindow *w, belowSorted)
	{
	    CompRect outerRect;

	    if (retval)
		break;

	    outerRect = w->serverInputRect ();

	    rect.setX (outerRect.x ());
	    rect.setY (outerRect.bottom ());

	    if (workArea.contains (rect) &&
		!rectOverlapsWindow (rect, belowSorted))
	    {
		pos.setX (rect.x () + window->input ().left);
		pos.setY (rect.y () + window->input ().top);
		retval = true;
	    }
	}
    }

    if (!retval)
    {
	/* try to the right of each window */
	foreach (CompWindow *w, rightSorted)
	{
	    CompRect outerRect;

	    if (retval)
		break;

	    outerRect = w->serverInputRect ();

	    rect.setX (outerRect.right ());
	    rect.setY (outerRect.y ());

	    if (workArea.contains (rect) &&
		!rectOverlapsWindow (rect, rightSorted))
	    {
		pos.setX (rect.x () + w->input ().left);
		pos.setY (rect.y () + w->input ().top);
		retval = true;
	    }
	}
    }

    return retval;
}

void
PlaceWindow::cascadeFindNext (const CompWindowList &windows,
			      const CompRect       &workArea,
			      CompPoint            &pos)
{
    CompWindowList           sorted;
    CompWindowList::iterator iter;
    int                      cascadeX, cascadeY;
    int                      xThreshold, yThreshold;
    int                      winWidth, winHeight;
    int                      cascadeStage;

    sorted = windows;
    sorted.sort (compareNorthWestCorner);

    /* This is a "fuzzy" cascade algorithm.
     * For each window in the list, we find where we'd cascade a
     * new window after it. If a window is already nearly at that
     * position, we move on.
     */

    /* arbitrary-ish threshold, honors user attempts to
     * manually cascade.
     */
#define CASCADE_FUZZ 15

    xThreshold = MAX (window->input ().left, CASCADE_FUZZ);
    yThreshold = MAX (window->input ().top, CASCADE_FUZZ);

    /* Find furthest-SE origin of all workspaces.
     * cascade_x, cascade_y are the target position
     * of NW corner of window frame.
     */

    cascadeX = MAX (0, workArea.x ());
    cascadeY = MAX (0, workArea.y ());

    /* Find first cascade position that's not used. */

    winWidth  = window->serverWidth ();
    winHeight = window->serverHeight ();

    cascadeStage = 0;
    for (iter = sorted.begin (); iter != sorted.end (); iter++)
    {
	CompWindow *w = *iter;
	int        wx, wy;

	/* we want frame position, not window position */
	wx = w->serverX () - w->input ().left;
	wy = w->serverY () - w->input ().top;

	if (abs (wx - cascadeX) < xThreshold &&
	    abs (wy - cascadeY) < yThreshold)
	{
	    /* This window is "in the way", move to next cascade
	     * point. The new window frame should go at the origin
	     * of the client window we're stacking above.
	     */
	    wx = cascadeX = w->serverX ();
	    wy = cascadeY = w->serverY ();

	    /* If we go off the screen, start over with a new cascade */
	    if ((cascadeX + winWidth > workArea.right ()) ||
		(cascadeY + winHeight > workArea.bottom ()))
	    {
		cascadeX = MAX (0, workArea.x ());
		cascadeY = MAX (0, workArea.y ());

#define CASCADE_INTERVAL 50 /* space between top-left corners of cascades */

		cascadeStage += 1;
		cascadeX += CASCADE_INTERVAL * cascadeStage;

		/* start over with a new cascade translated to the right,
		 * unless we are out of space
		 */
		if (cascadeX + winWidth < workArea.right ())
		{
		    iter = sorted.begin ();
		    continue;
		}
		else
		{
		    /* All out of space, this cascade_x won't work */
		    cascadeX = MAX (0, workArea.x ());
		    break;
		}
	    }
	}
	else
	{
	    /* Keep searching for a further-down-the-diagonal window. */
	}
    }

    /* cascade_x and cascade_y will match the last window in the list
     * that was "in the way" (in the approximate cascade diagonal)
     */

    /* Convert coords to position of window, not position of frame. */
    pos.setX (cascadeX + window->input ().left);
    pos.setY (cascadeY + window->input ().top);
}

bool
PlaceWindow::hasUserDefinedPosition (bool acceptPPosition)
{
    PLACE_SCREEN (screen);

    CompMatch &match = ps->optionGetForcePlacementMatch ();

    if (match.evaluate (window))
	return false;

    if (acceptPPosition && (window->sizeHints ().flags & PPosition))
	return true;

    if ((window->type () & CompWindowTypeNormalMask) ||
	ps->optionGetWorkarounds ())
    {
	/* Only accept USPosition on non-normal windows if workarounds are
	 * enabled because apps claiming the user set -geometry for a
	 * dialog or dock are most likely wrong
	 */
	if (window->sizeHints ().flags & USPosition)
	    return true;
    }

    return false;
}

PlaceWindow::PlacementStrategy
PlaceWindow::getStrategy ()
{
    if (window->type () & (CompWindowTypeDockMask       |
			   CompWindowTypeDesktopMask    |
			   CompWindowTypeUtilMask       |
			   CompWindowTypeToolbarMask    |
			   CompWindowTypeMenuMask       |
			   CompWindowTypeFullscreenMask |
			   CompWindowTypeUnknownMask))
    {
	/* assume the app knows best how to place these */
	return NoPlacement;
    }

    if (window->wmType () & (CompWindowTypeDockMask |
			     CompWindowTypeDesktopMask))
    {
	/* see above */
	return NoPlacement;
    }

    if (hasUserDefinedPosition (true))
	return ConstrainOnly;

   if (window->transientFor () &&
       (window->type () & (CompWindowTypeDialogMask |
			   CompWindowTypeModalDialogMask)))
    {
	CompWindow *parent = screen->findWindow (window->transientFor ());

	if (parent && parent->managed ())
	    return PlaceOverParent;
    }

    if (window->type () & (CompWindowTypeDialogMask      |
			   CompWindowTypeModalDialogMask |
			   CompWindowTypeSplashMask))
    {
	return PlaceCenteredOnScreen;
    }

    return PlaceAndConstrain;
}

const CompOutput &
PlaceWindow::getPlacementOutput (int		   mode,
				 PlacementStrategy strategy,
				 CompPoint         pos)
{
    int output = -1;
    int multiMode;

    /* short cut: it makes no sense to determine a placement
       output if there is only one output */
    if (screen->outputDevs ().size () == 1)
	return screen->outputDevs ().at (0);

    switch (strategy) {
    case PlaceOverParent:
	{
	    CompWindow *parent;

	    parent = screen->findWindow (window->transientFor ());
	    if (parent)
		output = parent->outputDevice ();
	}
	break;
    case ConstrainOnly:
	{
	    CompWindow::Geometry geom = window->serverGeometry ();

	    geom.setPos (pos);
	    output = screen->outputDeviceForGeometry (geom);
	}
	break;
    default:
	break;
    }

    if (output >= 0)
	return screen->outputDevs ()[output];

    multiMode = ps->optionGetMultioutputMode ();
    /* force 'output with pointer' for placement under pointer */
    if (mode == PlaceOptions::ModePointer)
	multiMode = PlaceOptions::MultioutputModeUseOutputDeviceWithPointer;

    switch (multiMode) {
	case PlaceOptions::MultioutputModeUseActiveOutputDevice:
	    return screen->currentOutputDev ();
	    break;
	case PlaceOptions::MultioutputModeUseOutputDeviceWithPointer:
	    {
		CompPoint p;
		if (PlaceScreen::get (screen)->getPointerPosition (p))
		{
		    output = screen->outputDeviceForPoint (p.x (), p.y ());
		}
	    }
	    break;
	case PlaceOptions::MultioutputModeUseOutputDeviceOfFocussedWindow:
	    {
		CompWindow *active;

		active = screen->findWindow (screen->activeWindow ());
		if (active)
		    output = active->outputDevice ();
	    }
	    break;
	case PlaceOptions::MultioutputModePlaceAcrossAllOutputs:
	    /* only place on fullscreen output if not placing centered, as the
	    constraining will move the window away from the center otherwise */
	    if (strategy != PlaceCenteredOnScreen)
		return screen->fullscreenOutput ();
	    break;
    }

    if (output < 0)
	return screen->currentOutputDev ();

    return screen->outputDevs ()[output];
}

int
PlaceWindow::getPlacementMode ()
{
    CompOption::Value::Vector& matches = ps->optionGetModeMatches ();
    CompOption::Value::Vector& modes   = ps->optionGetModeModes ();
    int                        i, min;

    min = MIN (matches.size (), modes.size ());

    for (i = 0; i < min; i++)
	if (matches[i].match ().evaluate (window))
	    return modes[i].i ();

    return ps->optionGetMode ();
}

void
PlaceWindow::constrainToWorkarea (const CompRect &workArea,
				  CompPoint      &pos)
{
    CompWindowExtents extents;
    int               delta;

    extents.left   = pos.x () - window->input ().left;
    extents.top    = pos.y () - window->input ().top;
    extents.right  = extents.left + window->serverWidth () +
		     (window->input ().left +
		      window->input ().right +
		      2 * window->serverGeometry ().border ());
    extents.bottom = extents.top + window->serverHeight () +
		     (window->input ().top +
		      window->input ().bottom +
		      2 * window->serverGeometry ().border ());

    delta = workArea.right () - extents.right;
    if (delta < 0)
	extents.left += delta;

    delta = workArea.left () - extents.left;
    if (delta > 0)
	extents.left += delta;

    delta = workArea.bottom () - extents.bottom;
    if (delta < 0)
	extents.top += delta;

    delta = workArea.top () - extents.top;
    if (delta > 0)
	extents.top += delta;

    pos.setX (extents.left + window->input ().left);
    pos.setY (extents.top  + window->input ().top);

}

bool
PlaceWindow::windowIsPlaceRelevant (CompWindow *w)
{
    if (w->id () == window->id ())
	return false;
    if (!w->isViewable () && !w->shaded ())
	return false;
    if (w->overrideRedirect ())
	return false;
    if (w->wmType () & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))
	return false;

    return true;
}

void
PlaceWindow::sendMaximizationRequest ()
{
    XEvent  xev;
    Display *dpy = screen->dpy ();

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = dpy;
    xev.xclient.format  = 32;

    xev.xclient.message_type = Atoms::winState;
    xev.xclient.window	     = window->id ();

    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = Atoms::winStateMaximizedHorz;
    xev.xclient.data.l[2] = Atoms::winStateMaximizedVert;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent (dpy, screen->root (), false,
		SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

bool
PlaceScreen::getPointerPosition (CompPoint &p)
{
    Window wDummy;
    int	   iDummy;
    unsigned int uiDummy;
    int x, y;
    bool ret;

    /* this means a server roundtrip, which kind of sucks; this
     * this code should be removed as soon as we have software cursor
     * rendering and thus a cache pointer co-ordinate */

    ret = XQueryPointer (screen->dpy (), screen->root (), &wDummy, &wDummy,
    			  &x, &y, &iDummy, &iDummy, &uiDummy);

    p.set (x, y);

    return ret;
}

bool
PlaceWindow::matchXYValue (CompOption::Value::Vector &matches,
			   CompOption::Value::Vector &xValues,
			   CompOption::Value::Vector &yValues,
			   CompPoint                 &pos,
			   CompOption::Value::Vector *constrainValues,
			   bool                      *keepInWorkarea)
{
    unsigned int i, min;

    if (window->type () & CompWindowTypeDesktopMask)
	return false;

    min = MIN (matches.size (), xValues.size ());
    min = MIN (min, yValues.size ());

    for (i = 0; i < min; i++)
    {
	if (matches[i].match ().evaluate (window))
	{
	    pos.setX (xValues[i].i ());
	    pos.setY (yValues[i].i ());

	    if (keepInWorkarea)
	    {
		if (constrainValues && constrainValues->size () > i)
		    *keepInWorkarea = (*constrainValues)[i].b ();
		else
		    *keepInWorkarea = true;
	    }

	    return true;
	}
    }

    return false;
}

bool
PlaceWindow::matchPosition (CompPoint &pos,
			    bool      &keepInWorkarea)
{
    return matchXYValue (
	ps->optionGetPositionMatches (),
	ps->optionGetPositionXValues (),
	ps->optionGetPositionYValues (),
	pos,
	&ps->optionGetPositionConstrainWorkarea (),
	&keepInWorkarea);
}

bool
PlaceWindow::matchViewport (CompPoint &pos)
{
    if (matchXYValue (ps->optionGetViewportMatches (),
		      ps->optionGetViewportXValues (),
		      ps->optionGetViewportYValues (),
		      pos))
    {
	/* Viewport matches are given 1-based, so we need to adjust that */
	pos.setX (pos.x () - 1);
	pos.setY (pos.y () - 1);

	return true;
    }

    return false;
}

void
PlaceWindow::grabNotify (int x,
			 int y,
			 unsigned int state,
			 unsigned int mask)
{
    if (mSavedOriginal)
    {
	if (screen->grabExist ("move") ||
	    screen->grabExist ("resize"))
	    mSavedOriginal = false;
    }

    window->grabNotify (x, y, state, mask);
}

bool
PlacePluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
	return false;

    return true;
}



