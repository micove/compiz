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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <core/core.h>
#include <core/atoms.h>
#include "resize.h"

COMPIZ_PLUGIN_20090315 (resize, ResizePluginVTable)

void
ResizeScreen::getPaintRectangle (BoxPtr pBox)
{
    pBox->x1 = geometry.x - w->input ().left;
    pBox->y1 = geometry.y - w->input ().top;
    pBox->x2 = geometry.x + geometry.width +
	       w->serverGeometry ().border () * 2 + w->input ().right;

    if (w->shaded ())
	pBox->y2 = geometry.y + w->size ().height () + w->input ().bottom;
    else
	pBox->y2 = geometry.y + geometry.height +
	           w->serverGeometry ().border () * 2 + w->input ().bottom;
}

void
ResizeWindow::getStretchScale (BoxPtr pBox, float *xScale, float *yScale)
{
    CompRect rect (window->inputRect ());

    *xScale = (rect.width ())  ? (pBox->x2 - pBox->x1) /
				 (float) rect.width () : 1.0f;
    *yScale = (rect.height ()) ? (pBox->y2 - pBox->y1) /
				 (float) rect.height () : 1.0f;
}

void
ResizeScreen::getStretchRectangle (BoxPtr pBox)
{
    BoxRec box;
    float  xScale, yScale;

    getPaintRectangle (&box);
    ResizeWindow::get (w)->getStretchScale (&box, &xScale, &yScale);

    pBox->x1 = (int) (box.x1 - (w->output ().left - w->input ().left) * xScale);
    pBox->y1 = (int) (box.y1 - (w->output ().top - w->input ().top) * yScale);
    pBox->x2 = (int) (box.x2 + w->output ().right * xScale);
    pBox->y2 = (int) (box.y2 + w->output ().bottom * yScale);
}

void
ResizeScreen::damageRectangle (BoxPtr pBox)
{
    int x1, x2, y1, y2;

    x1 = pBox->x1 - 1;
    y1 = pBox->y1 - 1;
    x2 = pBox->x2 + 1;
    y2 = pBox->y2 + 1;

    if (cScreen)
	cScreen->damageRegion (CompRect (x1, y1, x2 - x1, y2 - y1));
}

Cursor
ResizeScreen::cursorFromResizeMask (unsigned int mask)
{
    Cursor cursor;

    if (mask & ResizeLeftMask)
    {
	if (mask & ResizeDownMask)
	    cursor = downLeftCursor;
	else if (mask & ResizeUpMask)
	    cursor = upLeftCursor;
	else
	    cursor = leftCursor;
    }
    else if (mask & ResizeRightMask)
    {
	if (mask & ResizeDownMask)
	    cursor = downRightCursor;
	else if (mask & ResizeUpMask)
	    cursor = upRightCursor;
	else
	    cursor = rightCursor;
    }
    else if (mask & ResizeUpMask)
    {
	cursor = upCursor;
    }
    else
    {
	cursor = downCursor;
    }

    return cursor;
}

void
ResizeScreen::sendResizeNotify ()
{
    XEvent xev;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = screen->dpy ();
    xev.xclient.format  = 32;

    xev.xclient.message_type = resizeNotifyAtom;
    xev.xclient.window	     = w->id ();

    xev.xclient.data.l[0] = geometry.x;
    xev.xclient.data.l[1] = geometry.y;
    xev.xclient.data.l[2] = geometry.width;
    xev.xclient.data.l[3] = geometry.height;
    xev.xclient.data.l[4] = 0;

    XSendEvent (screen->dpy (), screen->root (), false,
		SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

void
ResizeScreen::updateWindowProperty ()
{
    CompOption::Vector data = resizeInformationAtom.getReadTemplate ();;
    CompOption::Value v;

    if (data.size () != 4)
	return;

    v = geometry.x;
    data.at (0).set (v);

    v = geometry.y;
    data.at (1).set (v);

    v = geometry.width;
    data.at (2).set (v);

    v = geometry.height;
    data.at (3).set (v);

    resizeInformationAtom.updateProperty (w->id (), data, XA_CARDINAL);
}

void
ResizeScreen::finishResizing ()
{
    w->ungrabNotify ();

    resizeInformationAtom.deleteProperty (w->id ());

    w = NULL;
}

#define TOUCH_LEFT 1
#define TOUCH_RIGHT 2
#define TOUCH_TOP 3
#define TOUCH_BOTTOM 4

static unsigned int
findTouchingOutput (int touchPoint, unsigned int side)
{
    for (unsigned int i = 0; i < screen->outputDevs ().size (); i++)
    {
	CompOutput &o = screen->outputDevs ().at (i);
	if (side == TOUCH_LEFT)
	{
	    if (o.left () == touchPoint)
		return  i;
	}
	if (side == TOUCH_RIGHT)
	{
	    if (o.right () == touchPoint)
		return  i;
	}
	if (side == TOUCH_TOP)
	{
	    if (o.top () == touchPoint)
		return  i;
	}
	if (side == TOUCH_BOTTOM)
	{
	    if (o.bottom () == touchPoint)
		return  i;
	}
    }

    return -1;
}

static void
getPointForTp (unsigned int tp, unsigned int output, int &op, int &wap)
{
    CompRect og = CompRect (screen->outputDevs ().at (output));
    CompRect wag = screen->outputDevs ().at (output).workArea ();

    switch (tp)
    {
	case TOUCH_LEFT:
	    op = og.right ();
	    wap = wag.right ();
	    break;
	case TOUCH_RIGHT:
	    op = og.left ();
	    wap = wag.left ();
	    break;
	case TOUCH_TOP:
	    op = og.bottom ();
	    wap = wag.bottom ();
	    break;
	case TOUCH_BOTTOM:
	    op = og.top ();
	    wap = wag.top ();
	    break;
	default:
	    return;
    }
}

/* Be a little bit intelligent about how we calculate
 * the workarea. Basically we want to be enclosed in
 * any area that is obstructed by panels, but not
 * where two outputs meet
 *
 * Also, it does not make sense to resize over
 * non-touching outputs, so detect that case too
 * */

static int
getOutputForEdge (int windowOutput, unsigned int touch, bool skipFirst)
{
    int op, wap;
    int ret = windowOutput;

    getPointForTp (touch, windowOutput, op, wap);

    if ((op == wap) || skipFirst)
    {
	int co = windowOutput;

	do
	{
	    int oco = co;

	    co = findTouchingOutput (op, touch);

	    /* Could not find a leftmost output from here
	     * so we must have hit the edge of the universe */
	    if (co == -1)
	    {
		ret = oco;
		co = -1;
		break;
	    }

	    getPointForTp (touch, co, op, wap);

	    /* There is something in the way here.... */
	    if (op != wap)
	    {
		ret = co;
		co = -1;
	    }
	}
        while (co != -1);
    }

    return ret;
}

static bool
resizeInitiate (CompAction         *action,
	        CompAction::State  state,
	        CompOption::Vector &options,
	        unsigned int	   mode)
{
    CompWindow *w;
    Window     xid;

    xid = CompOption::getIntOptionNamed (options, "window");

    w = screen->findWindow (xid);
    if (w && (w->actions () & CompWindowActionResizeMask))
    {
	unsigned int mask;
	int          x, y;
	int	     button;

	RESIZE_SCREEN (screen);

	CompWindow::Geometry server = w->serverGeometry ();

	x = CompOption::getIntOptionNamed (options, "x", pointerX);
	y = CompOption::getIntOptionNamed (options, "y", pointerY);

	button = CompOption::getIntOptionNamed (options, "button", -1);

	mask = CompOption::getIntOptionNamed (options, "direction");

	/* Initiate the resize in the direction suggested by the
	 * sector of the window the mouse is in, eg drag in top left
	 * will resize up and to the left.  Keyboard resize starts out
	 * with the cursor in the middle of the window and then starts
	 * resizing the edge corresponding to the next key press. */
	if (state & CompAction::StateInitKey)
	{
	    mask = 0;
	}
	else if (!mask)
	{
	    int sectorSizeX = server.width () / 3;
	    int sectorSizeY = server.height () / 3;
	    int posX        = x - server.x ();
	    int posY        = y - server.y ();

	    if (posX < sectorSizeX)
		mask |= ResizeLeftMask;
	    else if (posX > (2 * sectorSizeX))
		mask |= ResizeRightMask;

	    if (posY < sectorSizeY)
		mask |= ResizeUpMask;
	    else if (posY > (2 * sectorSizeY))
		mask |= ResizeDownMask;

	    /* if the pointer was in the middle of the window,
	       just prevent input to the window */

	    if (!mask)
		return true;
	}

	if (screen->otherGrabExist ("resize", NULL))
	    return false;

	if (rs->w)
	    return false;

	if (w->type () & (CompWindowTypeDesktopMask |
		          CompWindowTypeDockMask	 |
		          CompWindowTypeFullscreenMask))
	    return false;

	if (w->overrideRedirect ())
	    return false;

	if (state & CompAction::StateInitButton)
	    action->setState (action->state () | CompAction::StateTermButton);

	if (w->shaded ())
	    mask &= ~(ResizeUpMask | ResizeDownMask);

	rs->w	 = w;
	rs->mask = mask;

	rs->savedGeometry.x	 = server.x ();
	rs->savedGeometry.y	 = server.y ();
	rs->savedGeometry.width  = server.width ();
	rs->savedGeometry.height = server.height ();

	rs->geometry = rs->savedGeometry;

	rs->pointerDx = x - pointerX;
	rs->pointerDy = y - pointerY;

	rs->centered =  rs->optionGetResizeFromCenterMatch ().evaluate (w);

	if ((w->state () & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	{
	    /* if the window is fully maximized, showing the outline or
	       rectangle would be visually distracting as the window can't
	       be resized anyway; so we better don't use them in this case */
	    rs->mode = ResizeOptions::ModeNormal;
	}
	else if (!rs->gScreen || !rs->cScreen ||
		 !rs->cScreen->compositingActive ())
	{
	    rs->mode = ResizeOptions::ModeNormal;
	}
	else
	{
	    rs->mode = mode;
	}

	if (rs->mode != ResizeOptions::ModeNormal)
	{
	    RESIZE_WINDOW (w);
	    if (rw->gWindow && rs->mode == ResizeOptions::ModeStretch)
		rw->gWindow->glPaintSetEnabled (rw, true);
	    if (rw->cWindow && rs->mode == ResizeOptions::ModeStretch)
		rw->cWindow->damageRectSetEnabled (rw, true);
	    rs->gScreen->glPaintOutputSetEnabled (rs, true);
	}

	if (!rs->grabIndex)
	{
	    Cursor cursor;

	    if (state & CompAction::StateInitKey)
		cursor = rs->middleCursor;
	    else
		cursor = rs->cursorFromResizeMask (mask);

	    rs->grabIndex = screen->pushGrab (cursor, "resize");
	}

	if (rs->grabIndex)
	{
	    BoxRec box;
	    unsigned int grabMask = CompWindowGrabResizeMask |
				    CompWindowGrabButtonMask;
	    bool sourceExternalApp =
		CompOption::getBoolOptionNamed (options, "external", false);

	    if (sourceExternalApp)
		grabMask |= CompWindowGrabExternalAppMask;

	    rs->releaseButton = button;

	    w->grabNotify (x, y, state, grabMask);

	    if (screen->getOption ("raise_on_click")->value ().b ())
		w->updateAttributes (CompStackingUpdateModeAboveFullscreen);

	    /* using the paint rectangle is enough here
	       as we don't have any stretch yet */
	    rs->getPaintRectangle (&box);
	    rs->damageRectangle (&box);

	    if (state & CompAction::StateInitKey)
	    {
		int xRoot, yRoot;

		xRoot = server.x () + (server.width () / 2);
		yRoot = server.y () + (server.height () / 2);

		screen->warpPointer (xRoot - pointerX, yRoot - pointerY);
	    }

	    rs->isConstrained = sourceExternalApp;

	    /* Update offWorkAreaConstrained and workArea at grab time */
	    rs->offWorkAreaConstrained = false;
	    if (sourceExternalApp)
	    {
		int output = w->outputDevice ();
		int lco, tco, bco, rco;
		bool sl = screen->outputDevs ().at (output).workArea ().left () >
			  w->serverGeometry ().left ();
		bool sr = screen->outputDevs ().at (output).workArea ().right () <
			  w->serverGeometry ().right ();
		bool st = screen->outputDevs ().at (output).workArea ().top () >
			  w->serverGeometry ().top ();
		bool sb = screen->outputDevs ().at (output).workArea ().bottom () <
			  w->serverGeometry ().bottom ();

		lco = tco = bco = rco = output;

		/* Prevent resizing beyond work area edges when resize is
		   initiated externally (e.g. with window frame or menu)
		   and not with a key (e.g. alt+button) */
		rs->offWorkAreaConstrained = true;

		lco = getOutputForEdge (output, TOUCH_RIGHT, sl);
		rco = getOutputForEdge (output, TOUCH_LEFT, sr);
		tco = getOutputForEdge (output, TOUCH_BOTTOM, st);
		bco = getOutputForEdge (output, TOUCH_TOP, sb);

		/* Now we need to form one big rect which describes
		 * the available workarea */

		int left = screen->outputDevs ().at (lco).workArea ().left ();
		int right = screen->outputDevs ().at (rco).workArea ().right ();
		int top = screen->outputDevs ().at (tco).workArea ().top ();
		int bottom = screen->outputDevs ().at (bco).workArea ().bottom ();

		if (rs->grabWindowWorkArea)
		    delete rs->grabWindowWorkArea;

		rs->grabWindowWorkArea = new CompRect (0, 0, 0, 0);
		rs->grabWindowWorkArea->setLeft (left);
		rs->grabWindowWorkArea->setRight (right);
		rs->grabWindowWorkArea->setTop (top);
		rs->grabWindowWorkArea->setBottom (bottom);


		rs->inRegionStatus   = false;
		rs->lastGoodHotSpotY = -1;
		rs->lastGoodSize     = w->serverSize ();

		/* Combine the work areas of all outputs */
		rs->constraintRegion = emptyRegion;
		foreach (CompOutput &output, ::screen->outputDevs ())
		    rs->constraintRegion += output.workArea ();
	    }
	}
    }

    return false;
}

static bool
resizeInitiateDefaultMode (CompAction	      *action,
			   CompAction::State  state,
			   CompOption::Vector &options)
{
    CompWindow   *w;
    unsigned int mode;

    RESIZE_SCREEN (screen);

    w = screen->findWindow (CompOption::getIntOptionNamed (options, "window"));
    if (!w)
	return false;

    mode = rs->optionGetMode ();

    if (rs->optionGetNormalMatch ().evaluate (w))
	mode = ResizeOptions::ModeNormal;
    if (rs->optionGetOutlineMatch ().evaluate (w))
	mode = ResizeOptions::ModeOutline;
    if (rs->optionGetRectangleMatch ().evaluate (w))
	mode = ResizeOptions::ModeRectangle;
    if (rs->optionGetStretchMatch ().evaluate (w))
	mode = ResizeOptions::ModeStretch;

    return resizeInitiate (action, state, options, mode);
}

static bool
resizeTerminate (CompAction         *action,
	         CompAction::State  state,
	         CompOption::Vector &options)
{
    RESIZE_SCREEN (screen);

    if (rs->w)
    {
	CompWindow     *w = rs->w;
	XWindowChanges xwc;
	unsigned int   mask = 0;

	if (rs->mode == ResizeOptions::ModeNormal)
	{
	    if (state & CompAction::StateCancel)
	    {
		xwc.x      = rs->savedGeometry.x;
		xwc.y      = rs->savedGeometry.y;
		xwc.width  = rs->savedGeometry.width;
		xwc.height = rs->savedGeometry.height;

		mask = CWX | CWY | CWWidth | CWHeight;
	    }
	}
	else
	{
	    XRectangle geometry;

	    if (state & CompAction::StateCancel)
		geometry = rs->savedGeometry;
	    else
		geometry = rs->geometry;

	    if (memcmp (&geometry, &rs->savedGeometry, sizeof (geometry)) == 0)
	    {
		BoxRec box;

		if (rs->mode == ResizeOptions::ModeStretch)
		    rs->getStretchRectangle (&box);
		else
		    rs->getPaintRectangle (&box);

		rs->damageRectangle (&box);
	    }
	    else
	    {
		xwc.x      = geometry.x;
		xwc.y      = geometry.y;
		xwc.width  = geometry.width;
		xwc.height = geometry.height;

		mask = CWX | CWY | CWWidth | CWHeight;
	    }

	    if (rs->mode != ResizeOptions::ModeNormal)
	    {
		RESIZE_WINDOW (rs->w);
		if (rw->gWindow && rs->mode == ResizeOptions::ModeStretch)
		    rw->gWindow->glPaintSetEnabled (rw, false);
		if (rw->cWindow && rs->mode == ResizeOptions::ModeStretch)
		    rw->cWindow->damageRectSetEnabled (rw, false);
		rs->gScreen->glPaintOutputSetEnabled (rs, false);
	    }
	}

	if ((mask & CWWidth) &&
	    xwc.width == (int) w->serverGeometry ().width ())
	    mask &= ~CWWidth;

	if ((mask & CWHeight) &&
	    xwc.height == (int) w->serverGeometry ().height ())
	    mask &= ~CWHeight;

	if (mask)
	{
	    if (mask & (CWWidth | CWHeight))
		w->sendSyncRequest ();

	    w->configureXWindow (mask, &xwc);
	}

	if (!(mask & (CWWidth | CWHeight)))
	    rs->finishResizing ();

	if (rs->grabIndex)
	{
	    screen->removeGrab (rs->grabIndex, NULL);
	    rs->grabIndex = 0;
	}

	rs->releaseButton = 0;
    }

    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

    return false;
}


void
ResizeScreen::updateWindowSize ()
{
    if (w->syncWait ())
	return;

    if (w->serverGeometry ().width ()  != geometry.width ||
	w->serverGeometry ().height () != geometry.height)
    {
	XWindowChanges xwc;

	xwc.x	   = geometry.x;
	xwc.y	   = geometry.y;
	xwc.width  = geometry.width;
	xwc.height = geometry.height;

	w->sendSyncRequest ();

	w->configureXWindow (CWX | CWY | CWWidth | CWHeight, &xwc);
    }
}

void
ResizeScreen::handleKeyEvent (KeyCode keycode)
{
    if (grabIndex && w)
    {
	int	   widthInc, heightInc;

	widthInc  = w->sizeHints ().width_inc;
	heightInc = w->sizeHints ().height_inc;

	if (widthInc < MIN_KEY_WIDTH_INC)
	    widthInc = MIN_KEY_WIDTH_INC;

	if (heightInc < MIN_KEY_HEIGHT_INC)
	    heightInc = MIN_KEY_HEIGHT_INC;

	for (unsigned int i = 0; i < NUM_KEYS; i++)
	{
	    if (keycode != key[i])
		continue;

	    if (mask & rKeys[i].warpMask)
	    {
		XWarpPointer (screen->dpy (), None, None, 0, 0, 0, 0,
			      rKeys[i].dx * widthInc, rKeys[i].dy * heightInc);
	    }
	    else
	    {
		int x, y, left, top, width, height;

		CompWindow::Geometry server = w->serverGeometry ();
		CompWindowExtents    input  = w->input ();

		left   = server.x () - input.left;
		top    = server.y () - input.top;
		width  = input.left + server.width () + input.right;
		height = input.top  + server.height () + input.bottom;

		x = left + width  * (rKeys[i].dx + 1) / 2;
		y = top  + height * (rKeys[i].dy + 1) / 2;

		screen->warpPointer (x - pointerX, y - pointerY);

		mask = rKeys[i].resizeMask;

		screen->updateGrab (grabIndex, cursor[i]);
	    }
	    break;
	}
    }
}

void
ResizeScreen::handleMotionEvent (int xRoot, int yRoot)
{
    if (grabIndex)
    {
	BoxRec box;
	int    wi, he, cwi, che;        /* size of window contents (c prefix for constrained)*/
	int    wX, wY, wWidth, wHeight; /* rect. for window contents+borders */
	int    workAreaSnapDistance = 15;

	wi = savedGeometry.width;
	he = savedGeometry.height;

	if (!mask)
	{
	    int        xDist, yDist;
	    int        minPointerOffsetX, minPointerOffsetY;

	    CompWindow::Geometry server = w->serverGeometry ();

	    xDist = xRoot - (server.x () + (server.width () / 2));
	    yDist = yRoot - (server.y () + (server.height () / 2));

	    /* decision threshold is 10% of window size */
	    minPointerOffsetX = MIN (20, server.width () / 10);
	    minPointerOffsetY = MIN (20, server.height () / 10);

	    /* if we reached the threshold in one direction,
	       make the threshold in the other direction smaller
	       so there is a chance that this threshold also can
	       be reached (by diagonal movement) */
	    if (abs (xDist) > minPointerOffsetX)
		minPointerOffsetY /= 2;
	    else if (abs (yDist) > minPointerOffsetY)
		minPointerOffsetX /= 2;

	    if (abs (xDist) > minPointerOffsetX)
	    {
		if (xDist > 0)
		    mask |= ResizeRightMask;
		else
		    mask |= ResizeLeftMask;
	    }

	    if (abs (yDist) > minPointerOffsetY)
	    {
		if (yDist > 0)
		    mask |= ResizeDownMask;
		else
		    mask |= ResizeUpMask;
	    }

	    /* if the pointer movement was enough to determine a
	       direction, warp the pointer to the appropriate edge
	       and set the right cursor */
	    if (mask)
	    {
		Cursor     cursor;
		CompAction *action;
		int        pointerAdjustX = 0;
		int        pointerAdjustY = 0;

		action = &optionGetInitiateKey ();
		action->setState (action->state () |
				  CompAction::StateTermButton);

		if (mask & ResizeRightMask)
			pointerAdjustX = server.x () + server.width () +
					 w->input ().right - xRoot;
		else if (mask & ResizeLeftMask)
			pointerAdjustX = server.x () - w->input ().left -
					 xRoot;

		if (mask & ResizeDownMask)
			pointerAdjustY = server.y () + server.height () +
					 w->input ().bottom - yRoot;
		else if (mask & ResizeUpMask)
			pointerAdjustY = server.y () - w->input ().top - yRoot;

		screen->warpPointer (pointerAdjustX, pointerAdjustY);

		cursor = cursorFromResizeMask (mask);
		screen->updateGrab (grabIndex, cursor);
	    }
	}
	else
	{
	    /* only accumulate pointer movement if a mask is
	       already set as we don't have a use for the
	       difference information otherwise */

	    if (centered)
	    {
		pointerDx += (xRoot - lastPointerX) * 2;
		pointerDy += (yRoot - lastPointerY) * 2;
	    }
	    else
	    {
		pointerDx += xRoot - lastPointerX;
		pointerDy += yRoot - lastPointerY;
	    }
	    
	    /* If we hit the edge of the screen while resizing
	     * the window and the adjacent window edge has not hit
	     * the edge of the screen, then accumulate pointer motion
	     * in the opposite direction. (So the apparant x / y
	     * mixup here is intentional)
	     */

	    if (isConstrained)
	    {
		if (mask == ResizeLeftMask)
		{
		    if (xRoot == 0 &&
			geometry.x - w->input ().left > grabWindowWorkArea->left ())
			pointerDx += abs (yRoot - lastPointerY) * -1;
		}
		else if (mask == ResizeRightMask)
		{
		    if (xRoot == screen->width () -1 &&
			geometry.x + geometry.width + w->input ().right < grabWindowWorkArea->right ())
			pointerDx += abs (yRoot - lastPointerY);
		}
		if (mask == ResizeUpMask)
		{
		    if (yRoot == 0 &&
			geometry.y - w->input ().top > grabWindowWorkArea->top ())
			pointerDy += abs (xRoot - lastPointerX) * -1;
		}
		else if (mask == ResizeDownMask)
		{
		    if (yRoot == screen->height () -1 &&
			geometry.y + geometry.height + w->input ().bottom < grabWindowWorkArea->bottom ())
			pointerDx += abs (yRoot - lastPointerY);
		}
	    }
	}

	if (mask & ResizeLeftMask)
	    wi -= pointerDx;
	else if (mask & ResizeRightMask)
	    wi += pointerDx;

	if (mask & ResizeUpMask)
	    he -= pointerDy;
	else if (mask & ResizeDownMask)
	    he += pointerDy;

	if (w->state () & CompWindowStateMaximizedVertMask)
	    he = w->serverGeometry ().height ();

	if (w->state () & CompWindowStateMaximizedHorzMask)
	    wi = w->serverGeometry ().width ();

	cwi = wi;
	che = he;

	if (w->constrainNewWindowSize (wi, he, &cwi, &che))
	{
	    Box box;

	    /* Also, damage relevant paint rectangles */
	    if (mode == ResizeOptions::ModeRectangle ||
	        mode == ResizeOptions::ModeOutline)
		getPaintRectangle (&box);
	    else if (mode == ResizeOptions::ModeStretch)
		getStretchRectangle (&box);

	    damageRectangle (&box);
	}

	/* constrain to work area */
	if (offWorkAreaConstrained)
	{
	    if (mask & ResizeUpMask)
	    {
		int decorTop = savedGeometry.y + savedGeometry.height -
		    (che + w->input ().top);

		if (grabWindowWorkArea->y () > decorTop)
		    che -= grabWindowWorkArea->y () - decorTop;
	    }
	    if (mask & ResizeDownMask)
	    {
		int decorBottom = savedGeometry.y + che + w->input ().bottom;

		if (decorBottom >
		    grabWindowWorkArea->y () + grabWindowWorkArea->height ())
		    che -= decorBottom - (grabWindowWorkArea->y () +
					  grabWindowWorkArea->height ());
	    }
	    if (mask & ResizeLeftMask)
	    {
		int decorLeft = savedGeometry.x + savedGeometry.width -
		    (cwi + w->input ().left);

		if (grabWindowWorkArea->x () > decorLeft)
		    cwi -= grabWindowWorkArea->x () - decorLeft;
	    }
	    if (mask & ResizeRightMask)
	    {
		int decorRight = savedGeometry.x + cwi + w->input ().right;

		if (decorRight >
		    grabWindowWorkArea->x () + grabWindowWorkArea->width ())
		    cwi -= decorRight - (grabWindowWorkArea->x () +
				         grabWindowWorkArea->width ());
	    }
	}

	wi = cwi;
	he = che;

	/* compute rect. for window + borders */
	wWidth  = wi + w->input ().left + w->input ().right;
	wHeight = he + w->input ().top + w->input ().bottom;

	if (centered)
	{
	    if (mask & ResizeLeftMask)
		wX = geometry.x + geometry.width -
		     (wi + w->input ().left);
	    else
		wX = geometry.x - w->input ().left;

	    if (mask & ResizeUpMask)
		wY = geometry.y + geometry.height -
		     (he + w->input ().top);
	    else
		wY = geometry.y - w->input ().top;
	}
	else
	{
	    if (mask & ResizeLeftMask)
		wX = savedGeometry.x + savedGeometry.width -
		     (wi + w->input ().left);
	    else
		wX = savedGeometry.x - w->input ().left;

	    if (mask & ResizeUpMask)
		wY = savedGeometry.y + savedGeometry.height -
		     (he + w->input ().top);
	    else
		wY = savedGeometry.y - w->input ().top;
	}

	/* Check if resized edge(s) are near output work-area boundaries */
	foreach (CompOutput &output, ::screen->outputDevs ())
	{
	    const CompRect &workArea = output.workArea ();

	    /* if window and work-area intersect in x axis */
	    if (wX + wWidth > workArea.x () &&
		wX < workArea.x2 ())
	    {
		if (mask & ResizeLeftMask)
		{
		    int dw = workArea.x () - wX;

		    if (0 < dw && dw < workAreaSnapDistance)
		    {
			wi     -= dw;
			wWidth -= dw;
			wX     += dw;
		    }
		}
		else if (mask & ResizeRightMask)
		{
		    int dw = wX + wWidth - workArea.x2 ();

		    if (0 < dw && dw < workAreaSnapDistance)
		    {
			wi     -= dw;
			wWidth -= dw;
		    }
		}
	    }

	    /* if window and work-area intersect in y axis */
	    if (wY + wHeight > workArea.y () &&
		wY < workArea.y2 ())
	    {
		if (mask & ResizeUpMask)
		{
		    int dh = workArea.y () - wY;

		    if (0 < dh && dh < workAreaSnapDistance)
		    {
			he      -= dh;
			wHeight -= dh;
			wY      += dh;
		    }
		}
		else if (mask & ResizeDownMask)
		{
		    int dh = wY + wHeight - workArea.y2 ();

		    if (0 < dh && dh < workAreaSnapDistance)
		    {
			he      -= dh;
			wHeight -= dh;
		    }
		}
	    }
	}

	if (isConstrained)
	{
	    int minWidth  = 50;
	    int minHeight = 50;

	    /* rect. for a minimal height window + borders
	       (used for the constraining in X axis) */
	    int minimalInputHeight = minHeight +
				     w->input ().top + w->input ().bottom;

	    /* small hot-spot square (on window's corner or edge) that is to be
	       constrained to the combined output work-area region */
	    int x, y;
	    int width = w->input ().top; /* square size = title bar height */
	    int height = width;
	    bool status; /* whether or not hot-spot is in the region */

	    /* compute x & y for constrained hot-spot rect */
	    if (mask & ResizeLeftMask)
		x = wX;
	    else if (mask & ResizeRightMask)
		x = wX + wWidth - width;
	    else
		x = MIN (MAX (xRoot, wX), wX + wWidth - width);

	    if (mask & ResizeUpMask)
		y = wY;
	    else if (mask & ResizeDownMask)
		y = wY + wHeight - height;
	    else
		y = MIN (MAX (yRoot, wY), wY + wHeight - height);

	    status = constraintRegion.contains (x, y, width, height);

	    /* only constrain movement if previous position was valid */
	    if (inRegionStatus)
	    {
		bool xStatus = false;
		int yForXResize = y;
		int nx = x;
		int nw = wi;
		int nh = he;

		if (mask & (ResizeLeftMask | ResizeRightMask))
		{
		    xStatus = status;

		    if (mask & ResizeUpMask)
			yForXResize = wY + wHeight - minimalInputHeight;
		    else if (mask & ResizeDownMask)
			yForXResize = wY + minimalInputHeight - height;
		    else
			yForXResize = y;

		    if (!constraintRegion.contains (x, yForXResize,
						    width, height))
		    {
			if (lastGoodHotSpotY >= 0)
			    yForXResize = lastGoodHotSpotY;
			else
			    yForXResize = y;
		    }
		}
		if (mask & ResizeLeftMask)
		{
		    while ((nw > minWidth) && !xStatus)
		    {
			xStatus = constraintRegion.contains (nx, yForXResize,
							     width, height);
			if (!xStatus)
			{
			    nw--;
			    nx++;
			}
		    }
		    if (nw > minWidth)
		    {
			x = nx;
			wi = nw;
		    }
		}
		else if (mask & ResizeRightMask)
		{
		    while ((nw > minWidth) && !xStatus)
		    {
			xStatus = constraintRegion.contains (nx, yForXResize,
							     width, height);
			if (!xStatus)
			{
			    nw--;
			    nx--;
			}
		    }
		    if (nw > minWidth)
		    {
			x = nx;
			wi = nw;
		    }
		}

		if (mask & ResizeUpMask)
		{
		    while ((nh > minHeight) && !status)
		    {
			status = constraintRegion.contains (x, y,
							    width, height);
			if (!status)
			{
			    nh--;
			    y++;
			}
		    }
		    if (nh > minHeight)
			he = nh;
		}
		else if (mask & ResizeDownMask)
		{
		    while ((nh > minHeight) && !status)
		    {
			status = constraintRegion.contains (x, y,
							    width, height);
			if (!status)
			{
			    nh--;
			    y--;
			}
		    }
		    if (nh > minHeight)
			he = nh;
		}

		if (((mask & (ResizeLeftMask | ResizeRightMask)) && xStatus) ||
		    ((mask & (ResizeUpMask | ResizeDownMask)) && status))
		{
		    /* hot-spot inside work-area region, store good values */
		    lastGoodHotSpotY = y;
		    lastGoodSize     = CompSize (wi, he);
		}
		else
		{
		    /* failed to find a good hot-spot position, restore size */
		    wi = lastGoodSize.width ();
		    he = lastGoodSize.height ();
		}
	    }
	    else
	    {
		inRegionStatus = status;
	    }
	}

	if (mode != ResizeOptions::ModeNormal)
	{
	    if (mode == ResizeOptions::ModeStretch)
		getStretchRectangle (&box);
	    else
		getPaintRectangle (&box);

	    damageRectangle (&box);
	}

	if (centered)
	{
	    if ((mask & ResizeLeftMask) || (mask & ResizeRightMask))
		geometry.x -= ((wi - geometry.width) / 2);
	    if ((mask & ResizeUpMask) || (mask & ResizeDownMask))
		geometry.y -= ((he - geometry.height) / 2);
	}
	else
	{
	    if (mask & ResizeLeftMask)
		geometry.x -= wi - geometry.width;
	    if (mask & ResizeUpMask)
		geometry.y -= he - geometry.height;
	}

	geometry.width  = wi;
	geometry.height = he;

	if (mode != ResizeOptions::ModeNormal)
	{
	    if (mode == ResizeOptions::ModeStretch)
		getStretchRectangle (&box);
	    else
		getPaintRectangle (&box);

	    damageRectangle (&box);
	}
	else
	{
	    updateWindowSize ();
	}

	updateWindowProperty ();
	sendResizeNotify ();
    }
}

void
ResizeScreen::handleEvent (XEvent *event)
{
    switch (event->type) {
	case KeyPress:
	    if (event->xkey.root == screen->root ())
		handleKeyEvent (event->xkey.keycode);
	    break;
	case ButtonRelease:
	    if (event->xbutton.root == screen->root ())
	    {
		if (grabIndex)
		{
		    if (releaseButton         == -1 ||
			(int) event->xbutton.button == releaseButton)
		    {
			CompAction *action = &optionGetInitiateButton ();

			resizeTerminate (action, CompAction::StateTermButton,
					 noOptions);
		    }
		}
	    }
	    break;
	case MotionNotify:
	    if (event->xmotion.root == screen->root ())
		handleMotionEvent (pointerX, pointerY);
	    break;
	case EnterNotify:
	case LeaveNotify:
	    if (event->xcrossing.root == screen->root ())
		handleMotionEvent (pointerX, pointerY);
	    break;
	case ClientMessage:
	    if (event->xclient.message_type == Atoms::wmMoveResize)
	    {
		CompWindow    *w;
		unsigned long type = event->xclient.data.l[2];

		RESIZE_SCREEN (screen);

		if (type <= WmMoveResizeSizeLeft ||
		    type == WmMoveResizeSizeKeyboard)
		{
		    w = screen->findWindow (event->xclient.window);
		    if (w)
		    {
			CompOption::Vector o (0);

			o.push_back (CompOption ("window",
				     CompOption::TypeInt));
			o[0].value ().set ((int) event->xclient.window);

			o.push_back (CompOption ("external",
				     CompOption::TypeBool));
			o[1].value ().set (true);

			if (event->xclient.data.l[2] == WmMoveResizeSizeKeyboard)
			{
			    resizeInitiateDefaultMode (&optionGetInitiateKey (),
						       CompAction::StateInitKey,
						       o);
			}
			else
			{
			    static unsigned int mask[] = {
				ResizeUpMask | ResizeLeftMask,
				ResizeUpMask,
				ResizeUpMask | ResizeRightMask,
				ResizeRightMask,
				ResizeDownMask | ResizeRightMask,
				ResizeDownMask,
				ResizeDownMask | ResizeLeftMask,
				ResizeLeftMask,
			    };

			    /* TODO: not only button 1 */
			    if (pointerMods & Button1Mask)
			    {
				o.push_back (CompOption ("modifiers",
					     CompOption::TypeInt));
				o.push_back (CompOption ("x",
					     CompOption::TypeInt));
				o.push_back (CompOption ("y",
					     CompOption::TypeInt));
				o.push_back (CompOption ("direction",
					     CompOption::TypeInt));
				o.push_back (CompOption ("button",
					     CompOption::TypeInt));

				o[2].value ().set ((int) pointerMods);
				o[3].value ().set
				    ((int) event->xclient.data.l[0]);
				o[4].value ().set
				    ((int) event->xclient.data.l[1]);
				o[5].value ().set
				    ((int) mask[event->xclient.data.l[2]]);
				o[6].value ().set
				    ((int) (event->xclient.data.l[3] ?
				     event->xclient.data.l[3] : -1));

				resizeInitiateDefaultMode (
				    &optionGetInitiateButton (),
				    CompAction::StateInitButton, o);

				ResizeScreen::get (screen)->
				    handleMotionEvent (pointerX, pointerY);
			    }
			}
		    }
		}
		else if (rs->w && type == WmMoveResizeCancel)
		{
		    if (rs->w->id () == event->xclient.window)
		    {
			resizeTerminate (&optionGetInitiateButton (),
					 CompAction::StateCancel, noOptions);
			resizeTerminate (&optionGetInitiateKey (),
					 CompAction::StateCancel, noOptions);
		    }
		}
	    }
	    break;
	case DestroyNotify:
	    if (w && w->id () == event->xdestroywindow.window)
	    {
		resizeTerminate (&optionGetInitiateButton (), 0, noOptions);
		resizeTerminate (&optionGetInitiateKey (), 0, noOptions);
	    }
	    break;
	case UnmapNotify:
	    if (w && w->id () == event->xunmap.window)
	    {
		resizeTerminate (&optionGetInitiateButton (), 0, noOptions);
		resizeTerminate (&optionGetInitiateKey (), 0, noOptions);
	    }
	default:
	    break;
    }

    if (event->type == screen->xkbEvent () && w)
    {
	XkbAnyEvent *xkbEvent = (XkbAnyEvent *) event;

	if (xkbEvent->xkb_type == XkbStateNotify)
	{
	    XkbStateNotifyEvent *stateEvent = (XkbStateNotifyEvent *) event;

	    /* Check if we need to change to outline mode */

	    unsigned int mods = 0xffffffff;
	    bool	 modifierMode = false;
	    int		 oldMode = mode;
	    if (outlineMask)
		mods = outlineMask;

	    if ((stateEvent->mods & mods) == mods)
	    {
		modifierMode = true;
		mode = ResizeOptions::ModeOutline;
	    }

	    mods = 0xffffffff;
	    if (rectangleMask)
		mods = rectangleMask;

	    if ((stateEvent->mods & mods) == mods)
	    {
		modifierMode = true;
		mode = ResizeOptions::ModeRectangle;
	    }

	    mods = 0xffffffff;
	    if (stretchMask)
		mods = stretchMask;

	    if ((stateEvent->mods & mods) == mods)
	    {
		modifierMode = true;
		mode = ResizeOptions::ModeStretch;
	    }

	    mods = 0xffffffff;
	    if (centeredMask)
		mods = centeredMask;

	    if (!modifierMode)
		mode = optionGetMode ();

	    if (oldMode != mode)
	    {
		Box box;

		getStretchRectangle (&box);
		damageRectangle (&box);
		getPaintRectangle (&box);
		damageRectangle (&box);

		box.x1 = w->outputRect ().x ();
		box.y1 = w->outputRect ().y ();
		box.x2 = box.x1 + w->outputRect ().width ();
		box.y2 = box.y1 + w->outputRect ().height ();

		damageRectangle (&box);
	    }

	    if ((stateEvent->mods & mods) == mods)
		centered = true;
	    else if ((w &&
		      !optionGetResizeFromCenterMatch ().evaluate (w)))
		centered = false;
	}
    }

    screen->handleEvent (event);

    if (event->type == screen->syncEvent () + XSyncAlarmNotify)
    {
	if (w)
	{
	    XSyncAlarmNotifyEvent *sa;

	    sa = (XSyncAlarmNotifyEvent *) event;

	    if (w->syncAlarm () == sa->alarm)
		updateWindowSize ();
	}
    }
}

void
ResizeWindow::resizeNotify (int dx, int dy, int dwidth, int dheight)
{
    window->resizeNotify (dx, dy, dwidth, dheight);

    if (rScreen->w == window && !rScreen->grabIndex)
	rScreen->finishResizing ();
}

void
ResizeScreen::glPaintRectangle (const GLScreenPaintAttrib &sAttrib,
				const GLMatrix            &transform,
				CompOutput                *output,
				unsigned short            *borderColor,
				unsigned short            *fillColor)
{
    BoxRec   box;
    GLMatrix sTransform (transform);

    getPaintRectangle (&box);

    glPushMatrix ();

    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

    glLoadMatrixf (sTransform.getMatrix ());

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnable (GL_BLEND);

    /* fill rectangle */
    if (fillColor)
    {
	glColor4usv (fillColor);
	glRecti (box.x1, box.y2, box.x2, box.y1);
    }

    /* draw outline */
    glColor4usv (borderColor);
    glLineWidth (2.0);
    glBegin (GL_LINE_LOOP);
    glVertex2i (box.x1, box.y1);
    glVertex2i (box.x2, box.y1);
    glVertex2i (box.x2, box.y2);
    glVertex2i (box.x1, box.y2);
    glEnd ();

    /* clean up */
    glColor4usv (defaultColor);
    glDisable (GL_BLEND);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glPopMatrix ();
}

bool
ResizeScreen::glPaintOutput (const GLScreenPaintAttrib &sAttrib,
			     const GLMatrix            &transform,
			     const CompRegion          &region,
			     CompOutput                *output,
			     unsigned int              mask)
{
    bool status;

    if (w)
    {
	if (mode == ResizeOptions::ModeStretch)
	    mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;
    }

    status = gScreen->glPaintOutput (sAttrib, transform, region, output, mask);

    if (status && w)
    {
	unsigned short *border, *fill;

	border = optionGetBorderColor ();
	fill   = optionGetFillColor ();

	switch (mode) {
	    case ResizeOptions::ModeOutline:
		glPaintRectangle (sAttrib, transform, output, border, NULL);
		break;
	    case ResizeOptions::ModeRectangle:
		glPaintRectangle (sAttrib, transform, output, border, fill);
	    default:
		break;
	}
    }

    return status;
}

bool
ResizeWindow::glPaint (const GLWindowPaintAttrib &attrib,
		       const GLMatrix            &transform,
		       const CompRegion          &region,
		       unsigned int              mask)
{
    bool       status;

    if (window == rScreen->w && rScreen->mode == ResizeOptions::ModeStretch)
    {
	GLMatrix       wTransform (transform);
	BoxRec	       box;
	float	       xOrigin, yOrigin;
	float	       xScale, yScale;
	int            x, y;

	if (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK)
	    return false;

	status = gWindow->glPaint (attrib, transform, region,
				   mask | PAINT_WINDOW_NO_CORE_INSTANCE_MASK);

	GLFragment::Attrib fragment (gWindow->lastPaintAttrib ());

	if (window->alpha () || fragment.getOpacity () != OPAQUE)
	    mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

	rScreen->getPaintRectangle (&box);
	getStretchScale (&box, &xScale, &yScale);

	x = window->geometry (). x ();
	y = window->geometry (). y ();

	xOrigin = x - window->input ().left;
	yOrigin = y - window->input ().top;

	wTransform.translate (xOrigin, yOrigin, 0.0f);
	wTransform.scale (xScale, yScale, 1.0f);
	wTransform.translate ((rScreen->geometry.x - x) / xScale - xOrigin,
			      (rScreen->geometry.y - y) / yScale - yOrigin,
			      0.0f);

	glPushMatrix ();
	glLoadMatrixf (wTransform.getMatrix ());

	gWindow->glDraw (wTransform, fragment, region,
			 mask | PAINT_WINDOW_TRANSFORMED_MASK);

	glPopMatrix ();
    }
    else
    {
	status = gWindow->glPaint (attrib, transform, region, mask);
    }

    return status;
}

bool
ResizeWindow::damageRect (bool initial, const CompRect &rect)
{
    bool status = false;

    if (window == rScreen->w && rScreen->mode == ResizeOptions::ModeStretch)
    {
	BoxRec box;

	rScreen->getStretchRectangle (&box);
	rScreen->damageRectangle (&box);

	status = true;
    }

    status |= cWindow->damageRect (initial, rect);

    return status;
}

/* We have to make some assumptions here in order to do this neatly,
 * see build/generated/resize_options.h for more info */

#define ResizeModeShiftMask (1 << 0)
#define ResizeModeAltMask (1 << 1)
#define ResizeModeControlMask (1 << 2)
#define ResizeModeMetaMask (1 << 3)

void
ResizeScreen::resizeMaskValueToKeyMask (int	   valueMask,
					int	   *mask)
{
    if (valueMask & ResizeModeShiftMask)
	*mask |= ShiftMask;
    if (valueMask & ResizeModeAltMask)
	*mask |= CompAltMask;
    if (valueMask & ResizeModeControlMask)
	*mask |= ControlMask;
    if (valueMask & ResizeModeMetaMask)
	*mask |= CompMetaMask;
}

void
ResizeScreen::optionChanged (CompOption		    *option,
			     ResizeOptions::Options num)
{
    int *mask = NULL;
    int valueMask = 0;

    switch (num)
    {
	case ResizeOptions::OutlineModifier:
	    mask = &outlineMask;
	    valueMask = optionGetOutlineModifierMask ();
	    break;
	case ResizeOptions::RectangleModifier:
	    mask = &rectangleMask;
	    valueMask = optionGetRectangleModifierMask ();
	    break;
	case ResizeOptions::StretchModifier:
	    mask = &stretchMask;
	    valueMask = optionGetStretchModifierMask ();
	    break;
	case ResizeOptions::CenteredModifier:
	    mask = &centeredMask;
	    valueMask = optionGetCenteredModifierMask ();
	    break;
	default:
	    break;
    }

    if (mask)
	resizeMaskValueToKeyMask (valueMask, mask);
}

ResizeScreen::ResizeScreen (CompScreen *s) :
    PluginClassHandler<ResizeScreen,CompScreen> (s),
    gScreen (GLScreen::get (s)),
    cScreen (CompositeScreen::get (s)),
    w (NULL),
    outlineMask (0),
    rectangleMask (0),
    stretchMask (0),
    centeredMask (0),
    releaseButton (0),
    isConstrained (false),
    offWorkAreaConstrained (true),
    grabWindowWorkArea (NULL)
{
    CompOption::Vector atomTemplate;
    Display *dpy = s->dpy ();
    ResizeOptions::ChangeNotify notify =
	       boost::bind (&ResizeScreen::optionChanged, this, _1, _2);

    atomTemplate.resize (4);

    for (int i = 0; i < 4; i++)
    {
	char buf[4];
	snprintf (buf, 4, "%i", i);
	CompString tmpName (buf);

	atomTemplate.at (i).setName (tmpName, CompOption::TypeInt);
    }

    resizeNotifyAtom = XInternAtom (s->dpy (), "_COMPIZ_RESIZE_NOTIFY", 0);
    resizeInformationAtom = PropertyWriter ("_COMPIZ_RESIZE_INFORMATION",
    					    atomTemplate);

    for (unsigned int i = 0; i < NUM_KEYS; i++)
	key[i] = XKeysymToKeycode (s->dpy (), XStringToKeysym (rKeys[i].name));

    grabIndex = 0;

    leftCursor      = XCreateFontCursor (dpy, XC_left_side);
    rightCursor     = XCreateFontCursor (dpy, XC_right_side);
    upCursor        = XCreateFontCursor (dpy, XC_top_side);
    upLeftCursor    = XCreateFontCursor (dpy, XC_top_left_corner);
    upRightCursor   = XCreateFontCursor (dpy, XC_top_right_corner);
    downCursor      = XCreateFontCursor (dpy, XC_bottom_side);
    downLeftCursor  = XCreateFontCursor (dpy, XC_bottom_left_corner);
    downRightCursor = XCreateFontCursor (dpy, XC_bottom_right_corner);
    middleCursor    = XCreateFontCursor (dpy, XC_fleur);

    cursor[0] = leftCursor;
    cursor[1] = rightCursor;
    cursor[2] = upCursor;
    cursor[3] = downCursor;

    optionSetInitiateKeyInitiate (resizeInitiateDefaultMode);
    optionSetInitiateKeyTerminate (resizeTerminate);
    optionSetInitiateButtonInitiate (resizeInitiateDefaultMode);
    optionSetInitiateButtonTerminate (resizeTerminate);

    optionSetOutlineModifierNotify (notify);
    optionSetRectangleModifierNotify (notify);
    optionSetStretchModifierNotify (notify);
    optionSetCenteredModifierNotify (notify);

    resizeMaskValueToKeyMask (optionGetOutlineModifierMask (), &outlineMask);
    resizeMaskValueToKeyMask (optionGetRectangleModifierMask (), &rectangleMask);
    resizeMaskValueToKeyMask (optionGetStretchModifierMask (), &stretchMask);
    resizeMaskValueToKeyMask (optionGetCenteredModifierMask (), &centeredMask);

    ScreenInterface::setHandler (s);

    if (gScreen)
	GLScreenInterface::setHandler (gScreen, false);
}

ResizeScreen::~ResizeScreen ()
{
    Display *dpy = screen->dpy ();

    if (leftCursor)
	XFreeCursor (dpy, leftCursor);
    if (rightCursor)
	XFreeCursor (dpy, rightCursor);
    if (upCursor)
	XFreeCursor (dpy, upCursor);
    if (downCursor)
	XFreeCursor (dpy, downCursor);
    if (middleCursor)
	XFreeCursor (dpy, middleCursor);
    if (upLeftCursor)
	XFreeCursor (dpy, upLeftCursor);
    if (upRightCursor)
	XFreeCursor (dpy, upRightCursor);
    if (downLeftCursor)
	XFreeCursor (dpy, downLeftCursor);
    if (downRightCursor)
	XFreeCursor (dpy, downRightCursor);
}

ResizeWindow::ResizeWindow (CompWindow *w) :
    PluginClassHandler<ResizeWindow,CompWindow> (w),
    window (w),
    gWindow (GLWindow::get (w)),
    cWindow (CompositeWindow::get (w)),
    rScreen (ResizeScreen::get (screen))
{
    WindowInterface::setHandler (window);

    if (cWindow)
	CompositeWindowInterface::setHandler (cWindow, false);

    if (gWindow)
	GLWindowInterface::setHandler (gWindow, false);
}


ResizeWindow::~ResizeWindow ()
{
}


bool
ResizePluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
	 return false;

    return true;
}

