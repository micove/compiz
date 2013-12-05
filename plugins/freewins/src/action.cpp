/**
 * Compiz Fusion Freewins plugin
 *
 * action.cpp
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
 *  - Code could be cleaner
 *  - Add timestep and speed options to animation
 *  - Add window hover-over info via paintOutput : i.e
 *    - Resize borders
 *    - 'Reset' Button
 *    - 'Scale' Button
 *    - 'Rotate' Button
 */

/* TODO: Finish porting stuff to actions */

#include "freewins.h"


/* ------ Actions -------------------------------------------------------*/

/* Initiate Mouse Rotation */
bool
FWScreen::initiateFWRotate (CompAction         *action,
			    CompAction::State  state,
			    CompOption::Vector options)
{
    CompWindow* w;
    CompWindow *useW;
    Window xid;

    xid = CompOption::getIntOptionNamed (options, "window", 0);

    w   = screen->findWindow (xid);
    useW = screen->findWindow (xid);

    if (w)
    {

	foreach (FWWindowInputInfo *info, mTransformedWindows)
	{
	    if (info->ipw)
		if (w->id () == info->ipw)
		/* The window we just grabbed was actually
		 * an IPW, get the real window instead
		 */
		    useW = getRealWindow (w);
	}

	mRotateCursor = XCreateFontCursor (screen->dpy (), XC_fleur);

	if (!screen->otherGrabExist ("freewins", 0))
	    if (!mGrabIndex)
	    {
		mGrabIndex = screen->pushGrab (0, "freewins");
	    }
    }

    if (useW)
    {
	if (true || optionGetShapeWindowTypes ().evaluate (useW))
	{
	    FREEWINS_WINDOW (useW);

	    int x = CompOption::getIntOptionNamed (options, "x",
					       useW->x () + (useW->width () / 2));
	    int y = CompOption::getIntOptionNamed (options, "y",
					       useW->y () + (useW->height () / 2));

	    int mods = CompOption::getIntOptionNamed (options, "modifiers", 0);

	    mGrabWindow = useW;

	    fww->mGrab = grabRotate;

	    /* Save current scales and angles */

	    fww->mAnimate.oldAngX = fww->mTransform.angX;
	    fww->mAnimate.oldAngY = fww->mTransform.angY;
	    fww->mAnimate.oldAngZ = fww->mTransform.angZ;
	    fww->mAnimate.oldScaleX = fww->mTransform.scaleX;
	    fww->mAnimate.oldScaleY = fww->mTransform.scaleY;

	    if (pointerY > fww->mIMidY)
	    {
		if (pointerX > fww->mIMidX)
		    fww->mCorner = CornerBottomRight;
		else if (pointerX < fww->mIMidX)
		    fww->mCorner = CornerBottomLeft;
	    }
	    else if (pointerY < fww->mIMidY)
	    {
		if (pointerX > fww->mIMidX)
		    fww->mCorner = CornerTopRight;
		else if (pointerX < fww->mIMidX)
		    fww->mCorner = CornerTopLeft;
	    }

	    switch (optionGetZAxisRotation ())
	    {
		case ZAxisRotationAlways3d:
		    fww->mCan3D = TRUE;
		    fww->mCan2D = FALSE;
		    break;

		case ZAxisRotationAlways2d:
		    fww->mCan3D = FALSE;
		    fww->mCan2D = TRUE;
		    break;

		case ZAxisRotationDetermineOnClick:
		case ZAxisRotationSwitch:
		    fww->determineZAxisClick (pointerX, pointerY, FALSE);
		    break;

		case ZAxisRotationInterchangeable:
		    fww->mCan3D = TRUE;
		    fww->mCan2D = TRUE;
		    break;

		default:
		    break;
	    }

	    /* Set the rotation axis */

	    switch (optionGetRotationAxis ())
	    {
		case RotationAxisAlwaysCentre:
		default:
		    fww->calculateInputOrigin (WIN_REAL_X (mGrabWindow) +
					       WIN_REAL_W (mGrabWindow) / 2.0f,
					       WIN_REAL_Y (mGrabWindow) +
					       WIN_REAL_H (mGrabWindow) / 2.0f);
		    fww->calculateOutputOrigin (WIN_OUTPUT_X (mGrabWindow) +
						WIN_OUTPUT_W (mGrabWindow) / 2.0f,
						WIN_OUTPUT_Y (mGrabWindow) +
						WIN_OUTPUT_H (mGrabWindow) / 2.0f);
		    break;

		case RotationAxisClickPoint:
		    fww->calculateInputOrigin (mClick_root_x, mClick_root_y);
		    fww->calculateOutputOrigin (mClick_root_x, mClick_root_y);
		    break;

		case RotationAxisOppositeToClick:
		    fww->calculateInputOrigin (useW->x () + useW->width () - mClick_root_x,
					       useW->y () + useW->height () - mClick_root_y);
		    fww->calculateOutputOrigin (useW->x () + useW->width () - mClick_root_x,
						useW->y () + useW->height () - mClick_root_y);
		    break;
	    }

	    /* Announce that we grabbed the window */
	    useW->grabNotify (x, y, mods,  CompWindowGrabMoveMask |
			      CompWindowGrabButtonMask);

	    /* Shape the window beforehand and avoid a stale grab */
	    if (fww->canShape ())
		if (fww->handleWindowInputInfo ())
		    fww->adjustIPW ();

	    cScreen->damageScreen ();

	    if (state & CompAction::StateInitButton)
		action->setState (action->state () | CompAction::StateTermButton);

	}
    }
    return true;
}

bool
FWScreen::terminateFWRotate (CompAction          *action,
			     CompAction::State   state,
			     CompOption::Vector  options)
{
    if (mGrabWindow && mGrabIndex)
    {
	FREEWINS_WINDOW (mGrabWindow);
	if (fww->mGrab == grabRotate)
	{
	    int distX, distY;

	    fww->window->ungrabNotify ();

	    switch (optionGetRotationAxis ())
	    {
		case RotationAxisClickPoint:
		case RotationAxisOppositeToClick:

		    distX =  (fww->mOutputRect.x1 () +
			      (fww->mOutputRect.width ()) / 2.0f) -
			     (WIN_REAL_X (mGrabWindow) +
			      WIN_REAL_W (mGrabWindow) / 2.0f);
		    distY = (fww->mOutputRect.y1 () +
			     (fww->mOutputRect.height ()) / 2.0f) -
			    (WIN_REAL_Y (mGrabWindow) +
			     WIN_REAL_H (mGrabWindow) / 2.0f);

		    mGrabWindow->move (distX, distY, true);

		    fww->calculateInputOrigin (WIN_REAL_X (mGrabWindow) +
					       WIN_REAL_W (mGrabWindow) / 2.0f,
					       WIN_REAL_Y (mGrabWindow) +
					       WIN_REAL_H (mGrabWindow) / 2.0f);
		    fww->calculateOutputOrigin (WIN_OUTPUT_X (mGrabWindow) +
						WIN_OUTPUT_W (mGrabWindow) / 2.0f,
						WIN_OUTPUT_Y (mGrabWindow) +
						WIN_OUTPUT_H (mGrabWindow) / 2.0f);

		    break;
		default:
		    break;
	    }

	    if (fww->canShape ())
		if (fww->handleWindowInputInfo ())
		    fww->adjustIPW ();

	    screen->removeGrab (mGrabIndex, 0);
	    mGrabIndex = 0;
	    mGrabWindow = NULL;
	    fww->mGrab = grabNone;
	}
    }

    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

    return false;
}

/*static void FWMoveWindowToCorrectPosition (CompWindow *w, float distX, float distY)
{

	FREEWINS_WINDOW (w);    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

	fprintf(stderr, "distX is %f distY is %f midX and midY are %f %f\n", distX, distY, fww->mIMidX, fww->mIMidY);
	
	moveWindow (w, distX * (1 + (1 - fww->mTransform.scaleX)), distY * (1 + (1 - fww->mTransform.scaleY)), TRUE, FALSE);
	
	syncWindowPosition (w);
}*/

/* Initiate Scaling */
bool
FWScreen::initiateFWScale (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector options)
{
    CompWindow* w;
    CompWindow *useW;
    Window xid;

    xid = CompOption::getIntOptionNamed (options, "window", 0);
    w = screen->findWindow (xid);
    useW = screen->findWindow (xid);

    if (w)
    {
	foreach (FWWindowInputInfo *info, mTransformedWindows)
	{
	    if (info->ipw)
		if (w->id () == info->ipw)
		/* The window we just grabbed was actually
		 * an IPW, get the real window instead
		 */
		    useW = getRealWindow (w);
	}

	mRotateCursor = XCreateFontCursor (screen->dpy (), XC_fleur);

	if (!screen->otherGrabExist ("freewins", 0))
	    if (!mGrabIndex)
		mGrabIndex = screen->pushGrab (mRotateCursor, "freewins");
    }

    if (useW)
    {
	if (optionGetShapeWindowTypes ().evaluate (useW))
	{
	    FREEWINS_WINDOW (useW);

	    int x = CompOption::getIntOptionNamed (options, "x",
					       useW->x () + (useW->width () / 2));
	    int y = CompOption::getIntOptionNamed (options, "y",
					       useW->y () + (useW->height () / 2));

	    int mods = CompOption::getIntOptionNamed (options, "modifiers", 0);

	    mGrabWindow = useW;

	    /* Find out the corner we clicked in */

	    float MidX = fww->mInputRect.centerX ();
	    float MidY = fww->mInputRect.centerY ();

	    /* Check for Y axis clicking (Top / Bottom) */
	    if (pointerY > MidY)
	    {
		/* Check for X axis clicking (Left / Right) */
		if (pointerX > MidX)
		    fww->mCorner = CornerBottomRight;
		else if (pointerX < MidX)
		    fww->mCorner = CornerBottomLeft;
	    }
	    else if (pointerY < MidY)
	    {
		/* Check for X axis clicking (Left / Right) */
		if (pointerX > MidX)
		    fww->mCorner = CornerTopRight;
		else if (pointerX < MidX)
		    fww->mCorner = CornerTopLeft;
	    }

	    switch (optionGetScaleMode ())
	    {
		case ScaleModeToCentre:
		    fww->calculateInputOrigin(WIN_REAL_X (useW) + WIN_REAL_W (useW) / 2.0f,
					      WIN_REAL_Y (useW) + WIN_REAL_H (useW) / 2.0f);
		    fww->calculateOutputOrigin(WIN_OUTPUT_X (useW) + WIN_OUTPUT_W (useW) / 2.0f,
					       WIN_OUTPUT_Y (useW) + WIN_OUTPUT_H (useW) / 2.0f);
		    break;
		/*
		 *Experimental scale to corners mode
		 */
		case ScaleModeToOppositeCorner:
		    switch (fww->mCorner)
		    {
			case CornerBottomRight:
			    /* Translate origin to the top left of the window */
			    //FWMoveWindowToCorrectPosition (w, fww->inputRect.x1 - WIN_REAL_X (useW), fww->inputRect.y1 - WIN_REAL_Y (useW));
			    fww->calculateInputOrigin (WIN_REAL_X (useW), WIN_REAL_Y (useW));
			    break;

			case CornerBottomLeft:
			    /* Translate origin to the top right of the window */
			    //FWMoveWindowToCorrectPosition (w, fww->inputRect.x2 - (WIN_REAL_X (useW) + WIN_REAL_W (useW)), fww->inputRect.y1 - WIN_REAL_Y (useW));
			    fww->calculateInputOrigin (WIN_REAL_X (useW) + (WIN_REAL_W (useW)), WIN_REAL_Y (useW));
			    break;

			case CornerTopRight:
			    /* Translate origin to the bottom left of the window */
			    //FWMoveWindowToCorrectPosition (w, fww->inputRect.x1 - WIN_REAL_X (useW) , fww->inputRect.y1 - (WIN_REAL_Y (useW) + WIN_REAL_H (useW)));
			    fww->calculateInputOrigin (WIN_REAL_X (useW), WIN_REAL_Y (useW) + (WIN_REAL_H (useW)));
			    break;

			case CornerTopLeft:
			    /* Translate origin to the bottom right of the window */
			    //FWMoveWindowToCorrectPosition (w, fww->inputRect.x1 -(WIN_REAL_X (useW) + WIN_REAL_W (useW)) , fww->inputRect.y1 - (WIN_REAL_Y (useW) + WIN_REAL_H (useW)));
			    fww->calculateInputOrigin (WIN_REAL_X (useW) + (WIN_REAL_W (useW)), WIN_REAL_Y (useW) + (WIN_REAL_H (useW)));
			    break;
		    }
		    break;
	    }

	    fww->mGrab = grabScale;

	    /* Announce that we grabbed the window */
	    useW->grabNotify (x, y, mods,  CompWindowGrabMoveMask |
			      CompWindowGrabButtonMask);

	    cScreen->damageScreen ();

	    /* Shape the window beforehand and avoid a stale grab */
	    if (fww->canShape ())
		if (fww->handleWindowInputInfo ())
		    fww->adjustIPW ();


	    if (state & CompAction::StateInitButton)
		action->setState (action->state () | CompAction::StateTermButton);
	}
    }

    return TRUE;
}

bool
FWScreen::terminateFWScale (CompAction         *action,
			    CompAction::State  state,
			    CompOption::Vector options)
{
    if (mGrabWindow && mGrabIndex)
    {
	FREEWINS_WINDOW (mGrabWindow);
	if (fww->mGrab == grabScale)
	{
	    fww->window->ungrabNotify ();

	    switch (optionGetScaleMode ())
	    {
		int distX, distY;

		case ScaleModeToOppositeCorner:
		    distX =  (fww->mOutputRect.x1 () + (fww->mOutputRect.width () / 2.0f) - (WIN_REAL_X (mGrabWindow) + WIN_REAL_W (mGrabWindow) / 2.0f));
		    distY = (fww->mOutputRect.y1 () + (fww->mOutputRect.width () / 2.0f) - (WIN_REAL_Y (mGrabWindow) + WIN_REAL_H (mGrabWindow) / 2.0f));

		    mGrabWindow->move (distX, distY, true);

		    fww->calculateInputOrigin (WIN_REAL_X (mGrabWindow) +
					       WIN_REAL_W (mGrabWindow) / 2.0f,
					       WIN_REAL_Y (mGrabWindow) +
					       WIN_REAL_H (mGrabWindow) / 2.0f);
		    fww->calculateOutputOrigin (WIN_OUTPUT_X (mGrabWindow) +
						WIN_OUTPUT_W (mGrabWindow) / 2.0f,
						WIN_OUTPUT_Y (mGrabWindow) +
						WIN_OUTPUT_H (mGrabWindow) / 2.0f);

		    break;

		default:
		    break;

	    }

	    screen->removeGrab (mGrabIndex, 0);
	    mGrabIndex = 0;
	    mGrabWindow = NULL;
	    fww->mGrab = grabNone;
	}
    }

    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

    return FALSE;
}

/* Repetitive Stuff */

void
FWWindow::setPrepareRotation (float dx,
			      float dy,
			      float dz,
			      float dsu,
			      float dsd)
{    
    if (FWScreen::get (screen)->optionGetShapeWindowTypes ().evaluate (window))
    {
	calculateInputOrigin (WIN_REAL_X (window) +
			      WIN_REAL_W (window) / 2.0f,
			      WIN_REAL_Y (window) +
			      WIN_REAL_H (window) / 2.0f);
	calculateOutputOrigin (WIN_OUTPUT_X (window) +
			       WIN_OUTPUT_W (window) / 2.0f,
			       WIN_OUTPUT_Y (window) +
			       WIN_OUTPUT_H (window) / 2.0f);

	mTransform.unsnapAngX += dy;
	mTransform.unsnapAngY -= dx;
	mTransform.unsnapAngZ += dz;

	mTransform.unsnapScaleX += dsu;
	mTransform.unsnapScaleY += dsd;

	mAnimate.oldAngX = mTransform.angX;
	mAnimate.oldAngY = mTransform.angY;
	mAnimate.oldAngZ = mTransform.angZ;

	mAnimate.oldScaleX = mTransform.scaleX;
	mAnimate.oldScaleY = mTransform.scaleY;

	mAnimate.destAngX = mTransform.angX + dy;
	mAnimate.destAngY = mTransform.angY - dx;
	mAnimate.destAngZ = mTransform.angZ + dz;

	mAnimate.destScaleX = mTransform.scaleX + dsu;
	mAnimate.destScaleY = mTransform.scaleY + dsd;

    }
}

#define ROTATE_INC freewinsGetRotateIncrementAmount (w->screen)
#define NEG_ROTATE_INC freewinsGetRotateIncrementAmount (w->screen) *-1

#define SCALE_INC freewinsGetScaleIncrementAmount (w->screen)
#define NEG_SCALE_INC freewinsGetScaleIncrementAmount (w->screen) *-1

bool
FWScreen::rotate (CompAction         *action,
		  CompAction::State  state,
		  CompOption::Vector options, int dx, int dy, int dz)
{
    CompWindow *w = screen->findWindow (CompOption::getIntOptionNamed (options,
								       "window",
								       0));
    foreach (FWWindowInputInfo *info, mTransformedWindows)
    {
	if (info->ipw == w->id ())
	{
	    w = getRealWindow (w);
	}
    }

    FREEWINS_WINDOW (w);

    fww->setPrepareRotation (dx, dy, dz, 0, 0);

    if (fww->canShape ())
	if (fww->handleWindowInputInfo ())
	    fww->adjustIPW ();

    return true;
}

bool
FWScreen::scale (CompAction          *action,
		 CompAction::State   state,
		 CompOption::Vector  options,
		 int		     scale)
{
    CompWindow *w = screen->findWindow (CompOption::getIntOptionNamed (options,
								       "window",
								       0));
    foreach (FWWindowInputInfo *info, mTransformedWindows)
    {
	if (info->ipw == w->id ())
	{
	    w = getRealWindow (w);
	}
    }

    FREEWINS_WINDOW (w);

    fww->setPrepareRotation (0, 0, 0, scale, scale);
    fww->cWindow->addDamage ();

    if (fww->canShape ())
	if (fww->handleWindowInputInfo ())
	    fww->adjustIPW ();

    if (!optionGetAllowNegative ())
    {
	float minScale = optionGetMinScale ();

	if (fww->mAnimate.destScaleX < minScale)
	    fww->mAnimate.destScaleX = minScale;

	if (fww->mAnimate.destScaleY < minScale)
	    fww->mAnimate.destScaleY = minScale;
    }

    return true;
}

/* Reset the Rotation and Scale to 0 and 1 */
bool
FWScreen::resetFWTransform (CompAction         *action,
			    CompAction::State  state,
			    CompOption::Vector options)
{
    CompWindow *w = screen->findWindow (CompOption::getIntOptionNamed (options,
								       "window",
								       0));
    foreach (FWWindowInputInfo *info, mTransformedWindows)
    {
	if (info->ipw == w->id ())
	{
	    w = getRealWindow (w);
	}
    }

    if (w)
    {
	FREEWINS_WINDOW (w);
	fww->setPrepareRotation (fww->mTransform.angY,
				 -fww->mTransform.angX,
				 -fww->mTransform.angZ,
				 (1 - fww->mTransform.scaleX),
				 (1 - fww->mTransform.scaleY));
	fww->cWindow->addDamage ();

	fww->mTransformed = FALSE;

	if (fww->canShape ())
	    if (fww->handleWindowInputInfo ())
		fww->adjustIPW ();

	fww->mResetting = TRUE;
    }

    return TRUE;
}

/* Callable action to rotate a window to the angle provided
 * x: Set angle to x degrees
 * y: Set angle to y degrees
 * z: Set angle to z degrees
 * window: The window to apply the transformation to
 */
bool
FWScreen::rotateAction (CompAction         *action,
			CompAction::State  state,
			CompOption::Vector options)
{
    CompWindow *w;

    w = screen->findWindow (CompOption::getIntOptionNamed (options, "window", 0));

    if (w)
    {
	FREEWINS_WINDOW (w);

	float x = CompOption::getFloatOptionNamed(options, "x", 0.0f);
	float y = CompOption::getFloatOptionNamed(options, "y", 0.0f);
	float z = CompOption::getFloatOptionNamed(options, "z", 0.0f);

	fww->setPrepareRotation (x - fww->mAnimate.destAngX,
				 y - fww->mAnimate.destAngY,
				 z - fww->mAnimate.destAngZ, 0, 0);
	fww->cWindow->addDamage ();

    }
    else
    {
	return false;
    }

    return true;
}

/* Callable action to increment window rotation by the angles provided
 * x: Increment angle by x degrees
 * y: Increment angle by y degrees
 * z: Increment angle by z degrees
 * window: The window to apply the transformation to
 */
bool
FWScreen::incrementRotateAction (CompAction         *action,
				 CompAction::State  state,
				 CompOption::Vector options)
{
    CompWindow *w;

    w = screen->findWindow (CompOption::getIntOptionNamed (options, "window", 0));

    if (w)
    {
	FREEWINS_WINDOW (w);

	float x = CompOption::getFloatOptionNamed(options, "x", 0.0f);
	float y = CompOption::getFloatOptionNamed(options, "y", 0.0f);
	float z = CompOption::getFloatOptionNamed(options, "z", 0.0f);

	fww->setPrepareRotation (x,
				 y,
				 z, 0, 0);
	fww->cWindow->addDamage ();

    }
    else
    {
	return false;
    }

    return true;
}

/* Callable action to scale a window to the scale provided
 * x: Set scale to x factor
 * y: Set scale to y factor
 * window: The window to apply the transformation to
 */
bool
FWScreen::scaleAction (CompAction         *action,
		       CompAction::State  state,
		       CompOption::Vector options)
{
    CompWindow *w;

    w = screen->findWindow (CompOption::getIntOptionNamed (options, "window", 0));

    if (w)
    {
	FREEWINS_WINDOW (w);

	float x = CompOption::getFloatOptionNamed (options, "x", 0.0f);
	float y = CompOption::getFloatOptionNamed (options, "y", 0.0f);

	fww->setPrepareRotation (0, 0, 0,
				 x - fww->mAnimate.destScaleX,
				 y - fww->mAnimate.destScaleY);
	if (fww->canShape ())
	    if (fww->handleWindowInputInfo ())
		fww->adjustIPW ();

	/* Stop scale at threshold specified */
	if (!optionGetAllowNegative ())
	{
	    float minScale = optionGetMinScale ();
	    if (fww->mAnimate.destScaleX < minScale)
		fww->mAnimate.destScaleX = minScale;

	    if (fww->mAnimate.destScaleY < minScale)
		fww->mAnimate.destScaleY = minScale;
	}

	fww->cWindow->addDamage ();

	if (fww->canShape ())
	    fww->handleWindowInputInfo ();

    }
    else
    {
	return false;
    }

    return true;
}

/* Toggle Axis-Help Display */
bool
FWScreen::toggleFWAxis (CompAction         *action,
			CompAction::State  state,
			CompOption::Vector options)
{
    mAxisHelp = !mAxisHelp;

    cScreen->damageScreen ();

    return TRUE;
}
