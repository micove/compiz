/**
 * Compiz Fusion Freewins plugin
 *
 * events.cpp
 *
 * Copyright (C) 2007  Rodolfo Granata <warlock.cc@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author(s):
 * Rodolfo Granata <warlock.cc@gmail.com>
 * Sam Spilsbury <smspillaz@gmail.com>
 *
 * Button binding support and Reset added by:
 * enigma_0Z <enigma.0ZA@gmail.com>
 *
 * Most of the input handling here is based on
 * the shelf plugin by
 *        : Kristian Lyngstøl <kristian@bohemians.org>
 *        : Danny Baumann <maniac@opencompositing.org>
 *
 * Description:
 *
 * This plugin allows you to freely transform the texture of a window,
 * whether that be rotation or scaling to make better use of screen space
 * or just as a toy.
 *
 * Todo:
 *  - Fully implement an input redirection system by
 *    finding an inverse matrix, multiplying by it,
 *    translating to the actual window co-ords and
 *    XSendEvent() the co-ords to the actual window.
 * -  X Input Redirection
 *  - Code could be cleaner
 *  - Add timestep and speed options to animation
 *  - Add window hover-over info via paintOutput : i.e
 *    - Resize borders
 *    - 'Reset' Button
 *    - 'Scale' Button
 *    - 'Rotate' Button
 */

#include "freewins.h"


/* ------ Event Handlers ------------------------------------------------*/

void
FWWindow::handleIPWResizeInitiate ()
{
    FREEWINS_SCREEN (screen);

    window->activate ();
    mGrab = grabResize;
    fws->mRotateCursor = XCreateFontCursor (screen->dpy (), XC_plus);
    if(!screen->otherGrabExist ("freewins", "resize", 0))
	if(!fws->mGrabIndex)
	{
	    unsigned int mods = 0;
	    mods &= CompNoMask;
	    fws->mGrabIndex = screen->pushGrab (fws->mRotateCursor, "resize");
	    window->grabNotify (window->x () + (window->width () / 2),
				window->y () + (window->height () / 2), mods,
				CompWindowGrabMoveMask | CompWindowGrabButtonMask);
	    fws->mGrabWindow = window;
	}
}

void
FWWindow::handleIPWMoveInitiate ()
{
    FREEWINS_SCREEN (screen);

    window->activate ();
    mGrab = grabMove;
    fws->mRotateCursor = XCreateFontCursor (screen->dpy (), XC_fleur);
    if(!screen->otherGrabExist ("freewins", "resize", 0))
	if(!fws->mGrabIndex)
	{
	    unsigned int mods = 0;
	    mods &= CompNoMask;
	    fws->mGrabIndex = screen->pushGrab (fws->mRotateCursor, "resize");
	    window->grabNotify (window->x () + (window->width () / 2),
				window->y () + (window->height () / 2), mods,
				CompWindowGrabResizeMask | CompWindowGrabButtonMask);
	    fws->mGrabWindow = window;
	}
}

void
FWWindow::handleIPWMoveMotionEvent (unsigned int x,
				    unsigned int y)
{
    FREEWINS_SCREEN (screen);

    int dx = x - lastPointerX;
    int dy = y - lastPointerY;

    if (!fws->mGrabIndex)
	return;

    window->move (dx, dy, fws->optionGetImmediateMoves ());

}

void
FWWindow::handleIPWResizeMotionEvent (unsigned int x,
				      unsigned int y)
{
    int dx = (x - lastPointerX) * 10;
    int dy = (y - lastPointerY) * 10;

    mWinH += dy;
    mWinW += dx;

    /* In order to prevent a window redraw on resize
     * on every motion event we have a threshold
     */

    /* FIXME: cf-love: Instead of actually resizing the window, scale it up, then resize it */

    if (mWinH - 10 > window->height () || mWinW - 10 > window->width ())
    {
	XWindowChanges xwc;
	unsigned int   mask = CWX | CWY | CWWidth | CWHeight;

	xwc.x = window->serverX ();
	xwc.y = window->serverY ();
	xwc.width = mWinW;
	xwc.height = mWinH;

	if (xwc.width == window->serverWidth ())
	    mask &= ~CWWidth;

	if (xwc.height == window->serverHeight ())
	    mask &= ~CWHeight;

	if (window->mapNum () && (mask & (CWWidth | CWHeight)))
	    window->sendSyncRequest ();

	window->configureXWindow (mask, &xwc);
    }
}

/* Handle Rotation */
void
FWWindow::handleRotateMotionEvent (float dx,
				   float dy,
				   int x,
				   int y)
{
    FREEWINS_SCREEN (screen);

    x -= 100;
    y -= 100;

    int oldX = lastPointerX - 100;
    int oldY = lastPointerY - 100;

    float midX = WIN_REAL_X (window) + WIN_REAL_W (window)/2.0;
    float midY = WIN_REAL_Y (window) + WIN_REAL_H (window)/2.0;

    float angX;
    float angY;
    float angZ;

    /* Save the current angles so we can work with them */
    if (fws->optionGetSnap () || fws->mSnap)
    {
	angX = mTransform.unsnapAngX;
	angY = mTransform.unsnapAngY;
	angZ = mTransform.unsnapAngZ;
    }
    else
    {
	angX = mAnimate.destAngX;
	angY = mAnimate.destAngY;
	angZ = mAnimate.destAngZ;
    }

    /* Check for Y axis clicking (Top / Bottom) */
    if (pointerY > midY)
    {
	/* Check for X axis clicking (Left / Right) */
	if (pointerX > midX)
	    mCorner = CornerBottomRight;
	else if (pointerX < midX)
	    mCorner = CornerBottomLeft;
    }
    else if (pointerY < midY)
    {
	/* Check for X axis clicking (Left / Right) */
	if (pointerX > midX)
	    mCorner = CornerTopRight;
	else if (pointerX < midX)
	    mCorner = CornerTopLeft;
    }

    float percentFromXAxis = 0.0, percentFromYAxis = 0.0;

    if (fws->optionGetZAxisRotation () == FreewinsOptions::ZAxisRotationInterchangeable)
    {

	/* Trackball rotation was too hard to implement. If anyone can implement it,
	 * please come forward so I can replace this hacky solution to the problem.
	 * Anyways, what happens here, is that we determine how far away we are from
	 * each axis (y and x). The further we are away from the y axis, the more
	 * up / down movements become Z axis movements and the further we are away from
	 * the x-axis, the more left / right movements become z rotations. */

	/* We determine this by taking a percentage of how far away the cursor is from
	 * each axis. We divide the 3D rotation by this percentage ( and divide by the
	 * percentage squared in order to ensure that rotation is not too violent when we
	 * are quite close to the origin. We multiply the 2D rotation by this percentage also
	 * so we are essentially rotating in 3D and 2D all the time, but this is only really
	 * noticeable when you move the cursor over to the extremes of a window. In every case
	 * percentage can be defined as decimal-percentage (i.e 0.036 == 3.6%). Like I mentioned
	 * earlier, if you can replace this with trackball rotation, please come forward! */

	float halfWidth = WIN_REAL_W (window) / 2.0f;
	float halfHeight = WIN_REAL_H (window) / 2.0f;

	float distFromXAxis = fabs (mIMidX - pointerX);
	float distFromYAxis = fabs (mIMidY - pointerY);

	percentFromXAxis = distFromXAxis / halfWidth;
	percentFromYAxis = distFromYAxis / halfHeight;

    }
    else if (fws->optionGetZAxisRotation () == FreewinsOptions::ZAxisRotationSwitch)
	determineZAxisClick (pointerX, pointerY, TRUE);

    dx *= 360;
    dy *= 360;

    /* Handle inversion */

    bool can2D = mCan2D, can3D = mCan3D;

    if (fws->mInvert && fws->optionGetZAxisRotation () != FreewinsOptions::ZAxisRotationInterchangeable)
    {
	can2D = !mCan2D;
	can3D = !mCan3D;
    }

    if(can2D)
    {

	float zX = 1.0f;
	float zY = 1.0f;

	if (fws->optionGetZAxisRotation () == FreewinsOptions::ZAxisRotationInterchangeable)
	{
	    zX = percentFromXAxis;
	    zY = percentFromYAxis;
	}

	zX = zX > 1.0f ? 1.0f : zX;
	zY = zY > 1.0f ? 1.0f : zY;

	switch (mCorner)
	{
	    case CornerTopRight:

		if (x < oldX)
		    angZ -= dx * zX;
		else if (x > oldX)
		    angZ += dx * zX;

		if (y < oldY)
		    angZ -= dy * zY;
		else if (y > oldY)
		    angZ += dy * zY;

		break;

	    case CornerTopLeft:

		if (x < oldX)
		    angZ -= dx * zX;
		else if (x > oldX)
		    angZ += dx * zX;

		if (y < oldY)
		    angZ += dy * zY;
		else if (y > oldY)
		    angZ -= dy * zY;

		break;

	    case CornerBottomLeft:

		if (x < oldX)
		    angZ += dx * zX;
		else if (x > oldX)
		    angZ -= dx * zX;

		if (y < oldY)
		    angZ += dy * zY;
		else if (y > oldY)
		    angZ -= dy * zY;

		break;

	    case CornerBottomRight:

		if (x < oldX)
		    angZ += dx * zX;
		else if (x > oldX)
		    angZ -= dx * zX;

		if (y < oldY)
		    angZ -= dy * zY;
		else if (y > oldY)
		    angZ += dy * zY;

		break;
	}
    }

    if (can3D)
    {
	if (fws->optionGetZAxisRotation () != FreewinsOptions::ZAxisRotationInterchangeable)
	{
	    percentFromXAxis = 0.0f;
	    percentFromYAxis = 0.0f;
	}

	angX -= dy * (1 - percentFromXAxis);
	angY += dx * (1 - percentFromYAxis);
    }

    /* Restore angles */

    if (fws->optionGetSnap () || fws->mSnap)
    {
	mTransform.unsnapAngX = angX;
	mTransform.unsnapAngY = angY;
	mTransform.unsnapAngZ = angZ;
    }
    else
    {
	mAnimate.destAngX = angX;
	mAnimate.destAngY = angY;
	mAnimate.destAngZ = angZ;
    }

    handleSnap ();
}

/* Handle Scaling */
void
FWWindow::handleScaleMotionEvent (float dx,
				  float dy,
				  int x,
				  int y)
{
    FREEWINS_SCREEN (screen);

    x -= 100.0;
    y -= 100.0;
    
    int oldX = lastPointerX - 100;
    int oldY = lastPointerY - 100;
    
    float scaleX, scaleY;
    
    if (fws->optionGetSnap () || fws->mSnap)
    {
	scaleX = mTransform.unsnapScaleX;
	scaleY = mTransform.unsnapScaleY;
    }
    else
    {
	scaleX = mAnimate.destScaleX;
	scaleY = mAnimate.destScaleY;
    }

    calculateInputRect ();

    switch (mCorner)
    {
	case CornerTopLeft:

	    // Check X Direction
	    if (x < oldX)
		scaleX -= dx;
	    else if (x > oldX)
		scaleX -= dx;

	    // Check Y Direction
	    if (y < oldY)
		scaleY -= dy;
	    else if (y > oldY)
		scaleY -= dy;

	    break;

	case CornerTopRight:

	    // Check X Direction
	    if (x < oldX)
		scaleX += dx;
	    else if (x > oldX)
		scaleX += dx;

	    // Check Y Direction
	    if (y < oldY)
		scaleY -= dy;
	    else if (y > oldY)
		scaleY -= dy;

	    break;

	case CornerBottomLeft:

	    // Check X Direction
	    if (x < oldX)
		scaleX -= dx;
	    else if (y > oldX)
		scaleX -= dx;

	    // Check Y Direction
	    if (y < oldY)
		scaleY += dy;
	    else if (y > oldY)
		scaleY += dy;

	    break;

	case CornerBottomRight:

	    // Check X Direction
	    if (x < oldX)
		scaleX += dx;
	    else if (x > oldX)
		scaleX += dx;

	    // Check Y Direction
	    if (y < oldY)
		scaleY += dy;
	    else if (y > oldY)
		scaleY += dy;

	    break;
    }
    
    if (fws->optionGetSnap () || fws->mSnap)
    {
	mTransform.unsnapScaleX = scaleX;
	mTransform.unsnapScaleY = scaleY;
    }
    else
    {
	mAnimate.destScaleX = scaleX;
	mAnimate.destScaleY = scaleY;
    }

    /* Stop scale at threshold specified */
    if (!fws->optionGetAllowNegative ())
    {
	float minScale = fws->optionGetMinScale ();
	if (mAnimate.destScaleX < minScale)
	    mAnimate.destScaleX = minScale;

	if (mAnimate.destScaleY < minScale)
	    mAnimate.destScaleY = minScale;
    }

    /* Change scales for maintaining aspect ratio */
    if (fws->optionGetScaleUniform ())
    {
	float tempscaleX = mAnimate.destScaleX;
	float tempscaleY = mAnimate.destScaleY;
	mAnimate.destScaleX = (tempscaleX + tempscaleY) / 2;
	mAnimate.destScaleY = (tempscaleX + tempscaleY) / 2;
	mTransform.unsnapScaleX = (tempscaleX + tempscaleY) / 2;
	mTransform.unsnapScaleY = (tempscaleX + tempscaleY) / 2;
    }

    handleSnap ();
}

void
FWWindow::handleButtonReleaseEvent ()
{
    FREEWINS_SCREEN (screen);

    if (mGrab == grabMove || mGrab == grabResize)
    {
	screen->removeGrab (fws->mGrabIndex, NULL);
	window->ungrabNotify ();
	window->moveInputFocusTo ();
	adjustIPW ();
	fws->mGrabIndex = 0;
	fws->mGrabWindow = NULL;
	mGrab = grabNone;
    }
}

void
FWWindow::handleEnterNotify (XEvent *xev)
{
    XEvent EnterNotifyEvent;

    memcpy (&EnterNotifyEvent.xcrossing, &xev->xcrossing,
	    sizeof (XCrossingEvent));
/*
    if (window)
    {
	EnterNotifyEvent.xcrossing.window = window->id ();
	XSendEvent (screen->dpy (), window->id (),
		    FALSE, EnterWindowMask, &EnterNotifyEvent);
    }
*/
}

void
FWWindow::handleLeaveNotify (XEvent *xev)
{
    XEvent LeaveNotifyEvent;

    memcpy (&LeaveNotifyEvent.xcrossing, &xev->xcrossing,
	    sizeof (XCrossingEvent));
    LeaveNotifyEvent.xcrossing.window = window->id ();

    //XSendEvent (screen->dpy (), window->id (), FALSE,
    //            LeaveWindowMask, &LeaveNotifyEvent);
}

/* X Event Handler */
void
FWScreen::handleEvent (XEvent *ev)
{
    float dx, dy;
    CompWindow *oldPrev, *oldNext, *w;

    /* Check our modifiers first */

    if (ev->type == screen->xkbEvent ())
    {
	XkbAnyEvent *xkbEvent = (XkbAnyEvent *) ev;

	if (xkbEvent->xkb_type == XkbStateNotify)
	{
	    XkbStateNotifyEvent *stateEvent = (XkbStateNotifyEvent *) ev;
	    unsigned int snapMods = 0xffffffff;
	    unsigned int invertMods = 0xffffffff;

	    if (mSnapMask)
		snapMods = mSnapMask;

	    if ((stateEvent->mods & snapMods) == snapMods)
		mSnap = TRUE;
	    else
		mSnap = FALSE;

	    if (mInvertMask)
		invertMods = mInvertMask;

	    if ((stateEvent->mods & invertMods) == invertMods)
		mInvert = TRUE;
	    else
		mInvert = FALSE;

	}
    }

    switch(ev->type)
    {
	case EnterNotify:
	{
	    CompWindow *btnW;
	    btnW = screen->findWindow (ev->xbutton.subwindow);

	    /* It wasn't the subwindow, try the window */
	    if (!btnW)
		btnW = screen->findWindow (ev->xbutton.window);

	    /* We have established the CompWindow we clicked
	     * on. Get the real window */
	    if (btnW)
	    {
		FREEWINS_WINDOW (btnW);
		if (fww->canShape () && !mGrabWindow && !screen->otherGrabExist (0))
		    mHoverWindow = btnW;
		btnW = getRealWindow (btnW);
	    }
	    
	    if (btnW)
	    {
		FREEWINS_WINDOW (btnW);
		if (fww->canShape () && !mGrabWindow && !screen->otherGrabExist (0))
		    mHoverWindow = btnW;
		fww->handleEnterNotify (ev);
	    }
	}
	    break;

	case LeaveNotify:
	{
	    CompWindow *btnW;
	    btnW = screen->findWindow (ev->xbutton.subwindow);

	    /* It wasn't the subwindow, try the window */
	    if (!btnW)
		btnW = screen->findWindow (ev->xbutton.window);

	    /* We have established the CompWindow we clicked
	     * on. Get the real window */
	    if (btnW)
		btnW = getRealWindow (btnW);

	    if (btnW)
		FWWindow::get (btnW)->handleLeaveNotify (ev);
	}
	    break;

	case MotionNotify:
	{
	    if (mGrabWindow)
	    {
		FREEWINS_WINDOW (mGrabWindow);

		dx = ((float)(pointerX - lastPointerX) / screen->width ()) * \
		     optionGetMouseSensitivity ();
		dy = ((float)(pointerY - lastPointerY) / screen->height ()) * \
		     optionGetMouseSensitivity ();

		if (optionGetShapeWindowTypes ().evaluate (mGrabWindow))
		{
		    if (fww->mGrab == grabMove || fww->mGrab == grabResize)
		    {
			FWWindowInputInfo *info;
//			CompWindow *w = mGrabWindow;
			foreach (info, mTransformedWindows)
			{
			    if (mGrabWindow->id () == info->ipw)
				/* The window we just grabbed was actually
				 * an IPW, get the real window instead
				 */
				w = getRealWindow (mGrabWindow);
			}
		    }
		    switch (fww->mGrab)
		    {
			case grabMove:
			    fww->handleIPWMoveMotionEvent (pointerX, pointerY); break;
			case grabResize:
			    fww->handleIPWResizeMotionEvent (pointerX, pointerY); break;
			default:
			    break;
		    }
		}

		if (fww->mGrab == grabRotate)
		{
		    fww->handleRotateMotionEvent(dx, dy, ev->xmotion.x, ev->xmotion.y);
		}

		if (fww->mGrab == grabScale)
		{
		    fww->handleScaleMotionEvent(dx * 3, dy * 3, ev->xmotion.x, ev->xmotion.y);
		}

		//if(dx != 0.0 || dy != 0.0)
		//    fww->damageArea ();
	    }
	}
	    break;

	/* Button Press and Release */
	case ButtonPress:
	{
	    CompWindow *btnW;
	    btnW = screen->findWindow (ev->xbutton.subwindow);

	    /* It wasn't the subwindow, try the window */
	    if (!btnW)
		btnW = screen->findWindow (ev->xbutton.window);

	    /* We have established the CompWindow we clicked
	     * on. Get the real window
	     * FIXME: Free btnW and use another CompWindow * such as realW
	     */
	    if (btnW)
		btnW = getRealWindow (btnW);

	    if (btnW)
	    {
		FREEWINS_WINDOW (btnW);

		if (optionGetShapeWindowTypes ().evaluate (btnW))
		    switch (ev->xbutton.button)
		    {
			case Button1:
			    fww->handleIPWMoveInitiate ();
			    break;

			case Button3:
			    fww->handleIPWResizeInitiate ();
			    break;

			default:
			    break;
		    }
	    }

	    mClick_root_x = ev->xbutton.x_root;
	    mClick_root_y = ev->xbutton.y_root;
	}
	    break;

	case ButtonRelease:
	{
	    if (mGrabWindow)
	    {
		FREEWINS_WINDOW (mGrabWindow);

		if (optionGetShapeWindowTypes ().evaluate (mGrabWindow))
		    if (fww->mGrab == grabMove || fww->mGrab == grabResize)
		    {
			fww->handleButtonReleaseEvent ();
			mGrabWindow = 0;
		    }
	    }
	}
	    break;

	case ConfigureNotify:
	{
	    w = screen->findWindow (ev->xconfigure.window);
	    if (w)
	    {
		oldPrev = w->prev;
		oldNext = w->next;

		FREEWINS_WINDOW (w);

		fww->mWinH = WIN_REAL_H (w);
		fww->mWinW = WIN_REAL_W (w);
	    }
	}
	    break;

#if 0
	case ClientMessage:
	{
	    if (ev->xclient.message_type == Atoms::desktopViewport)
	    {
		/* Viewport change occurred, or something like that - adjust the IPW's */
		CompWindow *adjW, *actualW;

		foreach (adjW, screen->windows ())
		{
		    int vX = 0, vY = 0, dX, dY;

		    actualW = getRealWindow (adjW);

		    if (!actualW)
			actualW = adjW;

		    if (actualW)
		    {
			CompWindow *ipw;

			FREEWINS_WINDOW (actualW);

			if (!fww->mInput || fww->mInput->ipw)
			    break;

			ipw = screen->findWindow (fww->mInput->ipw);

			if (ipw)
			{
			    dX = screen->vp ().x () - vX;
			    dY = screen->vp ().y () - vY;

			    CompPoint p = actualW->defaultViewport ();

			    vX = p.x ();
			    vY = p.y ();

			    ipw->moveToViewportPosition (ipw->x () - dX * screen->width (),
							 ipw->y () - dY * screen->height (),
							 true); // ???
			}
		    }
		}
	    }
	}
	    break;
#endif
	default:
	    break;
#if 0
	    if (ev->type == screen->shapeEvent () + ShapeNotify)
	    {
		XShapeEvent *se = (XShapeEvent *) ev;
		if (se->kind == ShapeInput)
		{
		    CompWindow *w;
		    w = screen->findWindow (se->window);
		    if (w)
		    {
			FREEWINS_WINDOW (w);

			if (fww->canShape () && (fww->mTransform.scaleX != 1.0f || fww->mTransform.scaleY != 1.0f))
			{
			    // Reset the window back to normal
			    fww->mTransform.scaleX = 1.0f;
			    fww->mTransform.scaleY = 1.0f;
			    fww->mTransform.angX = 0.0f;
			    fww->mTransform.angY = 0.0f;
			    fww->mTransform.angZ = 0.0f;
			    /*FWShapeInput (w); - Disabled due to problems it causes*/
			}
		    }
		}
	    }
#endif
    }
    
    screen->handleEvent (ev);

    /* Now we can find out if a restacking occurred while we were handing events */
    switch (ev->type)
    {
	case ConfigureNotify:
	{
	    w = screen->findWindow (ev->xconfigure.window);
	    if (w)
	    {
		oldPrev = w->prev;
		oldNext = w->next;
		if (w->prev != oldPrev || w->next != oldNext)
		{
		    /* restacking occured, ensure ipw stacking */
		    adjustIPWStacking ();
		}
	    }
	}
	    break;
    }
}
