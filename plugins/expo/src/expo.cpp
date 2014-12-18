/**
 *
 * Compiz expo plugin
 *
 * expo.cpp
 *
 * Copyright (c) 2011 Linaro Limited
 * Copyright (c) 2008 Dennis Kasprzyk <racarr@opencompositing.org>
 * Copyright (c) 2006 Robert Carr <racarr@beryl-project.org>
 *
 * Authors:
 * Robert Carr <racarr@beryl-project.org>
 * Dennis Kasprzyk <onestone@opencompositing.org>
 * Travis Watkins <travis.watkins@linaro.org>
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
 **/

#include "expo.h"
#include "click-threshold.h"
#include "wall-offset.h"
#include <core/logmessage.h>
#include <math.h>
#ifndef USE_GLES
#include <GL/glu.h>
#endif
#include <X11/cursorfont.h>

COMPIZ_PLUGIN_20090315 (expo, ExpoPluginVTable);

#define sigmoid(x) (1.0f / (1.0f + exp (-11.0f * ((x) - 0.5f))))
#define sigmoidProgress(x) ((sigmoid (x) - sigmoid (0)) / \
			    (sigmoid (1) - sigmoid (0)))

#define interpolate(a, b, val) (((val) * (a)) + ((1 - (val)) * (b)))

bool
ExpoScreen::dndInit (CompAction         *action,
		     CompAction::State  state,
		     CompOption::Vector &options)
{
    if (expoMode)
    {
	dndState = DnDStart;
	action->setState (action->state () | CompAction::StateTermButton);
	cScreen->damageScreen ();

	return true;
    }

    return false;
}

bool
ExpoScreen::dndFini (CompAction         *action,
		     CompAction::State  state,
		     CompOption::Vector &options)
{
    if (dndState == DnDDuring || dndState == DnDStart)
    {
	if (dndWindow)
	    finishWindowMovement ();

	dndState  = DnDNone;
	dndWindow = NULL;

	/* The action could be an action of key, edge or button binding if
	 * expo was terminated during dnd. Thus we must fetch the action of
	 * dndButton ourselves or we mess their state up. */
	CompAction &dndAction = optionGetDndButton ();
	dndAction.setState (dndAction.state () & CompAction::StateInitButton);

	cScreen->damageScreen ();

	return true;
    }

    return false;
}

bool
ExpoScreen::doExpo (CompAction         *action,
		    CompAction::State  state,
		    CompOption::Vector &options)
{
    if (screen->otherGrabExist ("expo", NULL))
	return false;

    if (!expoMode)
    {
	if (!grabIndex)
	    grabIndex = screen->pushGrab (None, "expo");

	updateWraps (true);

	expoMode    = true;
	anyClick    = false;
	doubleClick = false;
	clickTime   = 0;

	dndState  = DnDNone;
	dndWindow = NULL;

	selectedVp     = screen->vp ();
	lastSelectedVp = selectedVp;
	origVp         = selectedVp;

	screen->addAction (&optionGetDndButton ());
	screen->addAction (&optionGetExitButton ());
	screen->addAction (&optionGetNextVpButton ());
	screen->addAction (&optionGetPrevVpButton ());

	cScreen->damageScreen ();
    }
    else
	termExpo (action, state, options);

    return true;
}

bool
ExpoScreen::termExpo (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector &options)
{
    /* Warning: *action is NULL if we came here from handleEvent. */
    if (!expoMode)
	return true;

    expoMode = false;

    if (dndState != DnDNone)
	dndFini (action, state, options);

    if (state & CompAction::StateCancel)
	vpUpdateMode = VPUpdatePrevious;
    else
	vpUpdateMode = VPUpdateMouseOver;

    dndState  = DnDNone;
    dndWindow = NULL;

    screen->removeAction (&optionGetDndButton ());
    screen->removeAction (&optionGetExitButton ());
    screen->removeAction (&optionGetNextVpButton ());
    screen->removeAction (&optionGetPrevVpButton ());

    cScreen->damageScreen ();
    screen->focusDefaultWindow ();

    return true;
}

bool
ExpoScreen::exitExpo (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector &options)
{
    if (!expoMode)
	return false;

    termExpo (action, 0, noOptions ());
    anyClick = true;
    cScreen->damageScreen ();

    return true;
}

bool
ExpoScreen::nextVp (CompAction         *action,
		    CompAction::State  state,
		    CompOption::Vector &options)
{
    if (!expoMode)
	return false;

    unsigned int newX = selectedVp.x () + 1;
    unsigned int newY = selectedVp.y ();

    if (newX >= (unsigned int) screen->vpSize ().width ())
    {
	newX = 0;
	newY = newY + 1;

	if (newY >= (unsigned int) screen->vpSize ().height ())
	    newY = 0;
    }

    moveFocusViewport (newX - selectedVp.x (),
		       newY - selectedVp.y ());
    cScreen->damageScreen ();

    return true;
}

bool
ExpoScreen::prevVp (CompAction         *action,
		    CompAction::State  state,
		    CompOption::Vector &options)
{
    if (!expoMode)
	return false;

    int newX = selectedVp.x () - 1;
    int newY = selectedVp.y ();

    if (newX < 0)
    {
	newX = screen->vpSize ().width () - 1;
	newY = newY - 1;

	if (newY < 0)
	    newY = screen->vpSize ().height () - 1;
    }

    moveFocusViewport (newX - selectedVp.x (),
		       newY - selectedVp.y ());
    cScreen->damageScreen ();

    return true;
}

void
ExpoScreen::moveFocusViewport (int dx,
			       int dy)
{
    lastSelectedVp = selectedVp;

    int newX = selectedVp.x () + dx;
    int newY = selectedVp.y () + dy;

    newX = MAX (0, MIN (static_cast <int> (screen->vpSize ().width ())  - 1, newX));
    newY = MAX (0, MIN (static_cast <int> (screen->vpSize ().height ()) - 1, newY));

    selectedVp.set (newX, newY);
    cScreen->damageScreen ();
}

void
ExpoScreen::finishWindowMovement ()
{
    CompOption::Vector o(0);
    dndWindow->ungrabNotify ();

    screen->handleCompizEvent ("expo", "start_viewport_switch", o);
    screen->moveViewport (screen->vp ().x () - selectedVp.x (),
			  screen->vp ().y () - selectedVp.y (), true);
    screen->handleCompizEvent ("expo", "end_viewport_switch", o);

    /* update saved window attributes in case we moved the
       window to a new viewport */
    if (dndWindow->saveMask () & CWX)
    {
	dndWindow->saveWc ().x = dndWindow->saveWc ().x % screen->width ();

	if (dndWindow->saveWc ().x < 0)
	    dndWindow->saveWc ().x += screen->width ();
    }

    if (dndWindow->saveMask () & CWY)
    {
	dndWindow->saveWc ().y = dndWindow->saveWc ().y % screen->height ();

	if (dndWindow->saveWc ().y < 0)
	    dndWindow->saveWc ().y += screen->height ();
    }

    /* update window attibutes to make sure a moved maximized window
       is properly snapped to the work area */
    if (dndWindow->state () & MAXIMIZE_STATE)
	dndWindow->updateAttributes (CompStackingUpdateModeNone);

#if 0 /* FIXME: obsolete in the meantime? */
    {
	/* make sure we snap to the correct output */
	int lastOutput = s->currentOutputDev;
	int centerX    = (WIN_X (w) + WIN_W (w) / 2) % s->width;

	if (centerX < 0)
	    centerX += s->width;

	int centerY = (WIN_Y (w) + WIN_H (w) / 2) % s->height;

	if (centerY < 0)
	    centerY += s->height;

	s->currentOutputDev = outputDeviceForPoint (s, centerX, centerY);

	updateWindowAttributes (w, CompStackingUpdateModeNone);

	s->currentOutputDev = lastOutput;
    }
#endif
}

void
ExpoScreen::handleEvent (XEvent *event)
{
    switch (event->type)
    {
	case KeyPress:
	    if (expoMode && event->xkey.root == screen->root ())
	    {
		if (event->xkey.keycode == leftKey)
		    moveFocusViewport (-1, 0);
		else if (event->xkey.keycode == rightKey)
		    moveFocusViewport (1, 0);
		else if (event->xkey.keycode == upKey)
		    moveFocusViewport (0, -1);
		else if (event->xkey.keycode == downKey)
		    moveFocusViewport (0, 1);
	    }

	    break;

	case ButtonPress:
	    if (expoMode			    &&
		event->xbutton.button == Button1    &&
		event->xbutton.root   == screen->root ())
	    {
		CompPoint pointer (event->xbutton.x_root, event->xbutton.y_root);

		if (!screen->workArea ().contains (pointer))
		    break;

		anyClick = true;

		if (clickTime == 0)
		    clickTime = event->xbutton.time;
		else if (event->xbutton.time - clickTime <=
			 static_cast <unsigned int> (optionGetDoubleClickTime ()) &&
			 lastSelectedVp == selectedVp)
		    doubleClick = true;
		else
		{
		    clickTime   = event->xbutton.time;
		    doubleClick = false;
		}

		cScreen->damageScreen ();
		prevClickPoint = CompPoint (event->xbutton.x, event->xbutton.y);
	    }

	    break;

	case ButtonRelease:
	    if (expoMode			    &&
		event->xbutton.button == Button1    &&
		event->xbutton.root   == screen->root ())
	    {
		CompPoint pointer (event->xbutton.x_root, event->xbutton.y_root);

		if (!screen->workArea ().contains (pointer))
		    break;

		if (event->xbutton.time - clickTime >
		    (unsigned int)optionGetDoubleClickTime ())
		{
		    clickTime   = 0;
		    doubleClick = false;
		}
		else if (doubleClick ||
			 compiz::expo::clickMovementInThreshold(prevClickPoint.x (),
								prevClickPoint.y (),
								event->xbutton.x,
								event->xbutton.y))
		{
		    clickTime   = 0;
		    doubleClick = false;

		    termExpo (NULL, 0, noOptions ());
		    anyClick = true;
		}
	    }

	    break;

	default:
	    break;
    }

    screen->handleEvent (event);
}

void
ExpoScreen::preparePaint (int msSinceLastPaint)
{
    float val = (static_cast <float> (msSinceLastPaint) / 1000.0f) /
		optionGetZoomTime ();

    if (expoMode)
	expoCam = MIN (1.0, expoCam + val);
    else
	expoCam = MAX (0.0, expoCam - val);

    if (expoCam)
    {
	unsigned int i, j, vp;
	unsigned int vpCountHorz = screen->vpSize ().width ();
	unsigned int vpCountVert = screen->vpSize ().height ();
	unsigned int vpCount     = vpCountHorz * vpCountVert;

	if (vpActivity.size () < vpCount)
	{
	    vpActivity.resize (vpCount);

	    foreach (float &activity, vpActivity)
		activity = 1.0f;
	}

	for (i = 0; i < vpCountHorz; ++i)
	{
	    for (j = 0; j < vpCountVert; ++j)
	    {
		vp = j * vpCountHorz + i;

		if (CompPoint (i, j) == selectedVp)
		    vpActivity[vp] = MIN (1.0, vpActivity[vp] + val);
		else
		    vpActivity[vp] = MAX (0.0, vpActivity[vp] - val);
	    }
	}

	const float degToRad    = M_PI / 180.0f;
	const int   screenWidth = screen->width ();

	for (i = 0; i < 360; ++i)
	{
	    vpNormals[i * 3]     = (-sin (i * degToRad) / screenWidth) * expoCam;
	    vpNormals[i * 3 + 1] = 0.0;
	    vpNormals[i * 3 + 2] = (-cos (i * degToRad) * expoCam) - (1 - expoCam);
	}
    }

    cScreen->preparePaint (msSinceLastPaint);
}

void
ExpoScreen::updateWraps (bool enable)
{
    screen->handleEventSetEnabled (this, enable);
    cScreen->preparePaintSetEnabled (this, enable);
    cScreen->paintSetEnabled (this, enable);
    cScreen->donePaintSetEnabled (this, enable);
    gScreen->glPaintOutputSetEnabled (this, enable);
    gScreen->glPaintTransformedOutputSetEnabled (this, enable);

    ExpoWindow *ew;

    foreach (CompWindow *w, screen->windows ())
    {
	ew = ExpoWindow::get (w);

	ew->cWindow->damageRectSetEnabled    (ew, enable);
	ew->gWindow->glPaintSetEnabled       (ew, enable);
	ew->gWindow->glDrawSetEnabled        (ew, enable);
	ew->gWindow->glAddGeometrySetEnabled (ew, enable);
	ew->gWindow->glDrawTextureSetEnabled (ew, enable);
    }
}

void
ExpoScreen::paint (CompOutput::ptrList &outputs,
		   unsigned int        mask)
{
    if (expoCam         > 0.0	&&
	outputs.size () > 1	&&
	optionGetMultioutputMode () == MultioutputModeOneBigWall)
    {
	outputs.clear ();
	outputs.push_back (&screen->fullscreenOutput ());
    }

    cScreen->paint (outputs, mask);
}

void
ExpoScreen::donePaint ()
{
    CompOption::Vector o(0);
    screen->handleCompizEvent ("expo", "start_viewport_switch", o);

    switch (vpUpdateMode)
    {
	case VPUpdateMouseOver:
	    screen->moveViewport (screen->vp ().x () - selectedVp.x (),
				  screen->vp ().y () - selectedVp.y (), true);
	    screen->focusDefaultWindow ();
	    vpUpdateMode = VPUpdateNone;
	    break;

	case VPUpdatePrevious:
	    screen->moveViewport (screen->vp ().x () - origVp.x (),
				  screen->vp ().y () - origVp.y (), true);
	    lastSelectedVp = selectedVp;
	    selectedVp     = origVp;
	    screen->focusDefaultWindow ();
	    vpUpdateMode = VPUpdateNone;
	    break;

	default:
	    break;
    }

    screen->handleCompizEvent ("expo", "end_viewport_switch", o);

    if ((expoCam > 0.0f && expoCam < 1.0f) || dndState != DnDNone)
	cScreen->damageScreen ();

    if (expoCam == 1.0f)
    {
	foreach (float &vp, vpActivity)
	    if (vp != 0.0 && vp != 1.0)
		cScreen->damageScreen ();
    }

    if (grabIndex && expoCam <= 0.0f && !expoMode)
    {
	screen->removeGrab (grabIndex, NULL);
	grabIndex = 0;
	updateWraps (false);
    }

    cScreen->donePaint ();

    switch (dndState)
    {
	case DnDDuring:
	{
	    if (dndWindow)
		dndWindow->move (newCursor.x () - prevCursor.x (),
				 newCursor.y () - prevCursor.y (),
				 optionGetExpoImmediateMove ());

	    prevCursor = newCursor;
	    cScreen->damageScreen ();
	}
	    break;

	case DnDStart:
	{
	    int xOffset = screen->vpSize ().width ()  * screen->width ();
	    int yOffset = screen->vpSize ().height () * screen->height ();

	    dndState = DnDNone;

	    bool       inWindow;
	    int        nx, ny;
	    CompWindow *w;

	    for (CompWindowList::reverse_iterator iter = screen->windows ().rbegin ();
		 iter != screen->windows ().rend (); ++iter)
	    {
		w = *iter;
		CompRect input (w->inputRect ());

		if (w->destroyed () ||
		    (!w->shaded () && !w->isViewable ()))
		    continue;

		if (w->onAllViewports ())
		{
		    nx = (newCursor.x () + xOffset) % screen->width ();
		    ny = (newCursor.y () + yOffset) % screen->height ();
		}
		else
		{
		    nx = newCursor.x () - (screen->vp ().x () * screen->width ());
		    ny = newCursor.y () - (screen->vp ().y () * screen->height ());
		}

		inWindow = (nx >= input.left () && nx <= input.right ()) ||
			   (nx >= (input.left ()  + xOffset) &&
			    nx <= (input.right () + xOffset));

		inWindow &= (ny >= input.top () && ny <= input.bottom ()) ||
			    (ny >= (input.top ()    + yOffset) &&
			     ny <= (input.bottom () + yOffset));

		if (!inWindow)
		    continue;

		/* make sure we never move windows we're not allowed to move */
		if (!w->managed ())
		    break;
		else if (!(w->actions () & CompWindowActionMoveMask))
		    break;
		else if (w->type () & (CompWindowTypeDockMask |
				       CompWindowTypeDesktopMask))
		    break;

		dndState  = DnDDuring;
		dndWindow = w;

		w->grabNotify (nx, ny, 0,
			       CompWindowGrabMoveMask |
			       CompWindowGrabButtonMask);

		screen->updateGrab (grabIndex, dragCursor);

		w->raise ();
		w->moveInputFocusTo ();
		break;
	    }

	    prevCursor = newCursor;
	}

	    break;

	case DnDNone:
	    screen->updateGrab (grabIndex, screen->normalCursor ());
	    break;

	default:
	    break;
    }
}

static bool
unproject (float          winx,
	   float          winy,
	   float          winz,
	   const GLMatrix &modelview,
	   const GLMatrix &projection,
	   const GLint    viewport[4],
	   float          *objx,
	   float          *objy,
	   float          *objz)
{
    GLMatrix finalMatrix = projection * modelview;
    float in[4], out[4];

    if (!finalMatrix.invert ())
	return false;

    in[0] = winx;
    in[1] = winy;
    in[2] = winz;
    in[3] = 1.0;

    /* Map x and y from window coordinates */
    in[0] = (in[0] - viewport[0]) / viewport[2];
    in[1] = (in[1] - viewport[1]) / viewport[3];

    /* Map to range -1 to 1 */
    in[0] = in[0] * 2 - 1;
    in[1] = in[1] * 2 - 1;
    in[2] = in[2] * 2 - 1;

    for (int i = 0; i < 4; ++i)
    {
	out[i] = in[0] * finalMatrix[i] +
	         in[1] * finalMatrix[4  + i] +
	         in[2] * finalMatrix[8  + i] +
	         in[3] * finalMatrix[12 + i];
    }

    if (out[3] == 0.0)
	return false;

    out[0] /= out[3];
    out[1] /= out[3];
    out[2] /= out[3];

    *objx = out[0];
    *objy = out[1];
    *objz = out[2];

    return true;
}

void
ExpoScreen::invertTransformedVertex (const GLScreenPaintAttrib &attrib,
				     const GLMatrix            &transform,
				     CompOutput                *output,
				     int                       vertex[2])
{
    GLMatrix sTransform (transform);
    float    p1[3], p2[3], v[3];
    GLint    viewport[4];

    gScreen->glApplyTransform (attrib, output, &sTransform);
    sTransform.toScreenSpace (output, -attrib.zTranslate);

    glGetIntegerv (GL_VIEWPORT, viewport);

    unproject (vertex[0], screen->height () - vertex[1], 0,
	    sTransform, *gScreen->projectionMatrix (), viewport,
	    &p1[0], &p1[1], &p1[2]);
    unproject (vertex[0], screen->height () - vertex[1], -1.0,
	    sTransform, *gScreen->projectionMatrix (), viewport,
	    &p2[0], &p2[1], &p2[2]);

    for (int i = 0; i < 3; ++i)
	v[i] = p1[i] - p2[i];

    float alpha = -p1[2] / v[2];

    if (optionGetDeform () == DeformCurve && screen->desktopWindowCount ())
    {
	const float screenWidth                  = static_cast <float> (screen->width ());
	const float screenWidthSquared           = screenWidth * screenWidth;
	const float curveDistSquaredPlusQuarter  = curveDistance * curveDistance + 0.25;
	const float pOne2MinusCurveDist          = p1[2] - curveDistance;
	const float v0Squared                    = v[0] * v[0];
	const float v2Squared                    = v[2] * v[2];
	const float vsv                          = v2Squared * screenWidthSquared +
						   v0Squared;

	const float p   = (2.0 * screenWidthSquared * pOne2MinusCurveDist * v[2] +
			   2.0 * p1[0] * v[0] - v[0] * screenWidth) / vsv;
	const float q   = (-screenWidthSquared * curveDistSquaredPlusQuarter +
			   screenWidthSquared * pOne2MinusCurveDist * pOne2MinusCurveDist +
			   0.25 * screenWidthSquared +
			   p1[0] * p1[0] - p1[0] * screenWidth) / vsv;

	const float rq  = 0.25 * p * p - q;
	const float ph  = -p * 0.5;

	if (rq < 0.0)
	{
	    vertex[0] = -1000;
	    vertex[1] = -1000;
	    return;
	}
	else
	{
	    alpha = ph + sqrt(rq);

	    if (p1[2] + (alpha * v[2]) > 0.0)
	    {
		vertex[0] = -1000;
		vertex[1] = -1000;
		return;
	    }
	}
    }

    vertex[0] = ceil (p1[0] + (alpha * v[0]));
    vertex[1] = ceil (p1[1] + (alpha * v[1]));
}

void
ExpoScreen::paintWall (const GLScreenPaintAttrib &attrib,
		       const GLMatrix&           transform,
		       const CompRegion&         region,
		       CompOutput                *output,
		       unsigned int              mask,
		       bool                      reflection)
{
    GLfloat  vertexData[12];
    GLushort colorData[16];
    GLMatrix sTransformW, sTransform (transform);

    CompPoint vpSize (screen->vpSize ().width (), screen->vpSize ().height ());

    /* amount of gap between viewports */
    const float gapY = optionGetVpDistance () * 0.1f * expoCam;
    const float gapX = optionGetVpDistance () * 0.1f * screen->height () /
		       screen->width () * expoCam;

    int glPaintTransformedOutputIndex = gScreen->glPaintTransformedOutputGetCurrentIndex ();

    GLVertexBuffer *streamingBuffer = GLVertexBuffer::streamingBuffer ();

    // Make sure that the base glPaintTransformedOutput function is called
    gScreen->glPaintTransformedOutputSetCurrentIndex (MAXSHORT);

    /* Zoom animation stuff */
    /* camera position for the selected viewport */
    GLVector vpCamPos (0, 0, 0, 0);

    /* camera position during expo mode */
    GLVector expoCamPos (0, 0, 0, 0);

    float sx = screen->width ()  / static_cast <float> (output->width ());
    float sy = screen->height () / static_cast <float> (output->height ());

    if (optionGetDeform () == DeformCurve)
	vpCamPos[GLVector::x] = -sx * (0.5 - ((static_cast <float> (output->x ()) +
					       output->width () / 2.0) /
					      static_cast <float> (screen->width ())));
    else
	vpCamPos[GLVector::x] = screen->vp ().x () * sx + 0.5 +
				output->x () / output->width () -
				vpSize.x () * 0.5 * sx +
				gapX * screen->vp ().x ();

    vpCamPos[GLVector::y] = -(screen->vp ().y () * sy + 0.5 +
			      output->y () / output->height ()) +
			    vpSize.y () * 0.5 * sy -
			    gapY * screen->vp ().y ();

    float biasZ = MAX (vpSize.x () * sx, vpSize.y () * sy);

    if (optionGetDeform () == DeformTilt || optionGetReflection ())
	biasZ *= (0.15 + optionGetDistance ());
    else
	biasZ *= optionGetDistance ();

    float progress = sigmoidProgress (expoCam);

    if (optionGetDeform () != DeformCurve)
	expoCamPos[GLVector::x] = gapX * (vpSize.x () - 1) * 0.5;

    expoCamPos[GLVector::y] = -gapY * (vpSize.y () - 1) * 0.5;
    expoCamPos[GLVector::z] = -DEFAULT_Z_CAMERA + DEFAULT_Z_CAMERA *
			      (MAX (vpSize.x () + (vpSize.x () - 1) * gapX,
				    vpSize.y () + (vpSize.y () - 1) * gapY) +
			       biasZ);

    /* interpolate between vpCamPos and expoCamPos */
    GLVector cam;

    cam[GLVector::x] = vpCamPos[GLVector::x] * (1 - progress) +
		       expoCamPos[GLVector::x] * progress;
    cam[GLVector::y] = vpCamPos[GLVector::y] * (1 - progress) +
		       expoCamPos[GLVector::y] * progress;
    cam[GLVector::z] = vpCamPos[GLVector::z] * (1 - progress) +
		       expoCamPos[GLVector::z] * progress;

    float aspectX = 1.0f, aspectY = 1.0f;

    if (vpSize.x () > vpSize.y ())
    {
	aspectY  = vpSize.x () / static_cast <float> (vpSize.y ());
	aspectY -= 1.0;
	aspectY *= -optionGetAspectRatio () + 1.0;
	aspectY *= progress;
	aspectY += 1.0;
    }
    else
    {
	aspectX  = vpSize.y () / static_cast <float> (vpSize.x ());
	aspectX -= 1.0;
	aspectX *= -optionGetAspectRatio () + 1.0;
	aspectX *= progress;
	aspectX += 1.0;
    }

    /* End of Zoom animation stuff */

    float rotation = 0.0f;

    if (optionGetDeform () == DeformTilt)
    {
	if (optionGetExpoAnimation () == ExpoAnimationZoom)
	    rotation = 10.0 * sigmoidProgress (expoCam);
	else
	    rotation = 10.0 * expoCam;
    }

    GLenum oldFilter = gScreen->textureFilter ();

    if (optionGetMipmaps ())
	gScreen->setTextureFilter (GL_LINEAR_MIPMAP_LINEAR);

    /* ALL TRANSFORMATION ARE EXECUTED FROM BOTTOM TO TOP */

    float oScale = 1 / (1 + ((MAX (sx, sy) - 1) * progress));

    sTransform.scale (oScale, oScale, 1.0);

    /* zoom out */
    oScale = DEFAULT_Z_CAMERA / (cam[GLVector::z] + DEFAULT_Z_CAMERA);
    sTransform.scale (oScale, oScale, oScale);
//    glNormal3f (0.0, 0.0, -oScale);
    sTransform.translate (-cam[GLVector::x], -cam[GLVector::y],
			  -cam[GLVector::z] - DEFAULT_Z_CAMERA);

    if (reflection)
    {
	float scaleFactor = optionGetScaleFactor ();

	sTransform.translate (0.0,
			      (vpSize.y () + ((vpSize.y () - 1) * gapY * 2)) *
			      -sy * aspectY,
			      0.0);
	sTransform.scale (1.0, -1.0, 1.0);
	sTransform.translate (0.0,
			      - (1 - scaleFactor) / 2 * sy * aspectY *
			      (vpSize.y () + ((vpSize.y () - 1) * gapY * 2)),
			      0.0);
	sTransform.scale (1.0, scaleFactor, 1.0);
	glCullFace (GL_FRONT);
    }

    /* rotate */
    sTransform.rotate (rotation, 0.0f, 1.0f, 0.0f);
    sTransform.scale (aspectX, aspectY, 1.0);

    CompPoint offsetInScreenCoords (optionGetXOffset (),
				    optionGetYOffset ());
    float offsetInWorldCoordX, offsetInWorldCoordY, worldScaleFactorX, worldScaleFactorY;

    compiz::expo::calculateWallOffset (*output,
				       offsetInScreenCoords,
				       vpSize,
				       *screen,
				       offsetInWorldCoordX,
				       offsetInWorldCoordY,
				       worldScaleFactorX,
				       worldScaleFactorY,
				       sigmoidProgress (expoCam));

    /* translate expo to center */
    sTransform.translate (vpSize.x () * sx * -0.5 + offsetInWorldCoordX,
			  vpSize.y () * sy *  0.5 - offsetInWorldCoordY, 0.0f);
    sTransform.scale (worldScaleFactorX, worldScaleFactorY, 1.0f);


    if (optionGetDeform () == DeformCurve)
	sTransform.translate ((vpSize.x () - 1) * sx * 0.5, 0.0, 0.0);

    sTransformW = sTransform;

    /* revert prepareXCoords region shift. Now all screens display the same */
    sTransform.translate (0.5f, -0.5f, DEFAULT_Z_CAMERA);

    if (vpSize.x () > 2)
	/* we can't have 90 degree for the left/right most viewport */
	curveAngle = interpolate (359 / ((vpSize.x () - 1) * 2), 1,
				  optionGetCurve ());
    else
	curveAngle = interpolate (180 / vpSize.x (), 1, optionGetCurve ());

    const float halfGapX = gapX / 2.0;

    curveDistance = ((0.5f * sx) + halfGapX) /
		    tanf ((M_PI / 360.0f) * curveAngle);
    curveRadius   = ((0.5f * sx) + halfGapX) /
		    sinf ((M_PI / 360.0f) * curveAngle);

    expoActive = true;

    float rotateX, vpp;
    int   vp;

    for (int j = 0; j < vpSize.y (); ++j)
    {
	GLMatrix sTransform2 (sTransform), sTransform3;

	for (int i = 0; i < vpSize.x (); ++i)
	{
	    if (optionGetExpoAnimation () == ExpoAnimationVortex)
		sTransform2.rotate (360 * expoCam,
				    0.0f, 1.0f, 2.0f * expoCam);

	    sTransform3 = sTransform2;

	    sTransform3.translate ( output->x () / static_cast <float> (output->width ()),
				   -output->y () / static_cast <float> (output->height ()), 0.0);

	    cScreen->setWindowPaintOffset ((screen->vp ().x () - i) *
					   screen->width (),
					   (screen->vp ().y () - j) *
					   screen->height ());

	    vp = (j * vpSize.x ()) + i;

	    vpp = (expoCam * vpActivity[vp]) + (1 - expoCam);
	    vpp = sigmoidProgress (vpp);

	    vpBrightness = vpp + ((1.0 - vpp) *
				  optionGetVpBrightness () / 100.0);
	    vpSaturation = vpp + ((1.0 - vpp) *
				  optionGetVpSaturation () / 100.0);

	    paintingVp.set (i, j);

	    if (optionGetDeform () == DeformCurve)
	    {
		sTransform3.translate (-vpCamPos[GLVector::x], 0.0f,
				       curveDistance - DEFAULT_Z_CAMERA);

		rotateX = -i + interpolate ((static_cast <float> (vpSize.x ()) / 2.0) -
					    0.5,
					    screen->vp ().x (), progress);

		sTransform3.rotate (curveAngle * rotateX, 0.0, 1.0, 0.0);

		sTransform3.translate (vpCamPos[GLVector::x], 0.0f,
				       DEFAULT_Z_CAMERA - curveDistance);
	    }

	    gScreen->glPaintTransformedOutput (attrib, sTransform3,
					       screen->region (), output,
					       mask);

	    if (!reflection)
	    {
		int cursor[2] = { pointerX, pointerY };

		invertTransformedVertex (attrib, sTransform3,
					 output, cursor);

		if (cursor[0] > 0					&&
		    cursor[0] < static_cast <int> (screen->width ())	&&
		    cursor[1] > 0					&&
		    cursor[1] < static_cast <int> (screen->height ()))
		{
		    newCursor.setX (i * screen->width ()  + cursor[0]);
		    newCursor.setY (j * screen->height () + cursor[1]);

		    if (anyClick || dndState != DnDNone)
		    {
			/* Used to save last viewport interaction was in */
			lastSelectedVp = selectedVp;
			selectedVp.set (i, j);
			anyClick       = false;
		    }
		}
	    }

	    /* not sure this will work with different resolutions */
	    if (optionGetDeform () != DeformCurve)
		sTransform2.translate (sx + gapX, 0.0f, 0.0);
	}

	/* not sure this will work with different resolutions */
	sTransform.translate (0.0, -(sy + gapY), 0.0f);
    }

//    glNormal3f (0.0, 0.0, -1.0);

    if (reflection)
    {
	GLboolean glBlendEnabled = glIsEnabled (GL_BLEND);

	/* just enable blending if it is disabled */
	if (!glBlendEnabled)
	    glEnable (GL_BLEND);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (optionGetDeform () != DeformCurve)
	{
	    streamingBuffer->begin (GL_TRIANGLE_STRIP);

	    colorData[0]  = 0;
	    colorData[1]  = 0;
	    colorData[2]  = 0;
	    colorData[3]  = 65535;
	    colorData[4]  = 0;
	    colorData[5]  = 0;
	    colorData[6]  = 0;
	    colorData[7]  = 32768;
	    colorData[8]  = 0;
	    colorData[9]  = 0;
	    colorData[10] = 0;
	    colorData[11] = 65535;
	    colorData[12] = 0;
	    colorData[13] = 0;
	    colorData[14] = 0;
	    colorData[15] = 32768;

	    vertexData[0]  = 0;
	    vertexData[1]  = 0;
	    vertexData[2]  = 0;
	    vertexData[3]  = 0;
	    vertexData[4]  = -vpSize.y () * (sy + gapY);
	    vertexData[5]  = 0;
	    vertexData[6]  = vpSize.x () * sx * (1.0 + gapX);
	    vertexData[7]  = 0;
	    vertexData[8]  = 0;
	    vertexData[9]  = vpSize.x () * sx * (1.0 + gapX);
	    vertexData[10] = -vpSize.y () * sy * (1.0 + gapY);
	    vertexData[11] = 0;

	    streamingBuffer->addColors (4, colorData);
	    streamingBuffer->addVertices (4, vertexData);

	    streamingBuffer->end ();
	    streamingBuffer->render (sTransformW);
	}
	else
	{
	    GLMatrix cTransform;
	    cTransform.translate (0.0, 0.0, -DEFAULT_Z_CAMERA);

	    glCullFace (GL_BACK);

	    streamingBuffer->begin (GL_TRIANGLE_STRIP);

	    colorData[0]  = 0;
	    colorData[1]  = 0;
	    colorData[2]  = 0;
	    colorData[3]  = (1.0 * expoCam) * 65535;
	    colorData[4]  = 0;
	    colorData[5]  = 0;
	    colorData[6]  = 0;
	    colorData[7]  = (1.0 * expoCam) * 65535;
	    colorData[8]  = 0;
	    colorData[9]  = 0;
	    colorData[10] = 0;
	    colorData[11] = (0.5 * expoCam) * 65535;
	    colorData[12] = 0;
	    colorData[13] = 0;
	    colorData[14] = 0;
	    colorData[15] = (0.5 * expoCam) * 65535;

	    vertexData[0]  = -0.5;
	    vertexData[1]  = -0.5;
	    vertexData[2]  = 0;
	    vertexData[3]  = 0.5;
	    vertexData[4]  = -0.5;
	    vertexData[5]  = 0;
	    vertexData[6]  = -0.5;
	    vertexData[7]  = 0;
	    vertexData[8]  = 0;
	    vertexData[9]  = 0.5;
	    vertexData[10] = 0;
	    vertexData[11] = 0;

	    streamingBuffer->addColors (4, colorData);
	    streamingBuffer->addVertices (4, vertexData);

	    streamingBuffer->end ();
	    streamingBuffer->render (cTransform);

	    streamingBuffer->begin (GL_TRIANGLE_STRIP);

	    colorData[0]  = 0;
	    colorData[1]  = 0;
	    colorData[2]  = 0;
	    colorData[3]  = (0.5 * expoCam) * 65535;
	    colorData[4]  = 0;
	    colorData[5]  = 0;
	    colorData[6]  = 0;
	    colorData[7]  = (0.5 * expoCam) * 65535;
	    colorData[8]  = 0;
	    colorData[9]  = 0;
	    colorData[10] = 0;
	    colorData[11] = 0;
	    colorData[12] = 0;
	    colorData[13] = 0;
	    colorData[14] = 0;
	    colorData[15] = 0;

	    vertexData[0]  = -0.5;
	    vertexData[1]  = 0;
	    vertexData[2]  = 0;
	    vertexData[3]  = 0.5;
	    vertexData[4]  = 0;
	    vertexData[5]  = 0;
	    vertexData[6]  = -0.5;
	    vertexData[7]  = 0.5;
	    vertexData[8]  = 0;
	    vertexData[9]  = 0.5;
	    vertexData[10] = 0.5;
	    vertexData[11] = 0;

	    streamingBuffer->addColors (4, colorData);
	    streamingBuffer->addVertices (4, vertexData);

	    streamingBuffer->end ();
	    streamingBuffer->render (cTransform);
	}

	glCullFace (GL_BACK);

	if (optionGetGroundSize () > 0.0)
	{
	    float    groundSize = optionGetGroundSize ();
	    GLMatrix gTransform;
	    gTransform.translate (0.0, 0.0, -DEFAULT_Z_CAMERA);

	    streamingBuffer->begin (GL_TRIANGLE_STRIP);

	    vertexData[0]  = -0.5;
	    vertexData[1]  = -0.5;
	    vertexData[2]  = 0;
	    vertexData[3]  = 0.5;
	    vertexData[4]  = -0.5;
	    vertexData[5]  = 0;
	    vertexData[6]  = -0.5;
	    vertexData[7]  = -0.5 + groundSize;
	    vertexData[8]  = 0;
	    vertexData[9]  = 0.5;
	    vertexData[10] = -0.5 + groundSize;
	    vertexData[11] = 0;

	    streamingBuffer->addColors (1, optionGetGroundColor1 ());
	    streamingBuffer->addColors (1, optionGetGroundColor1 ());
	    streamingBuffer->addColors (1, optionGetGroundColor2 ());
	    streamingBuffer->addColors (1, optionGetGroundColor2 ());
	    streamingBuffer->addVertices (4, vertexData);

	    streamingBuffer->end ();
	    streamingBuffer->render (gTransform);
	}

	glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	/* just disable blending if it was disabled before */
	if (!glBlendEnabled)
	    glDisable (GL_BLEND);
    }

    expoActive = false;

    cScreen->setWindowPaintOffset (0, 0);

    gScreen->glPaintTransformedOutputSetCurrentIndex (glPaintTransformedOutputIndex);

    gScreen->setTextureFilter (oldFilter);
}

bool
ExpoScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			   const GLMatrix            &transform,
			   const CompRegion          &region,
			   CompOutput                *output,
			   unsigned int              mask)
{
    if (expoCam > 0.0)
	mask |= PAINT_SCREEN_TRANSFORMED_MASK | PAINT_SCREEN_CLEAR_MASK;

    return gScreen->glPaintOutput (attrib, transform, region, output, mask);
}

void
ExpoScreen::glPaintTransformedOutput (const GLScreenPaintAttrib &attrib,
				      const GLMatrix            &transform,
				      const CompRegion          &region,
				      CompOutput                *output,
				      unsigned int              mask)
{
    expoActive = false;

    if (expoCam > 0)
	mask |= PAINT_SCREEN_CLEAR_MASK;

    if (optionGetExpoAnimation () == ExpoScreen::ExpoAnimationZoom)
    {
	vpBrightness = 0.0f;
	vpSaturation = 0.0f;
    }
    else
    {
	vpBrightness = (1.0f - sigmoidProgress (expoCam));
	vpSaturation = (1.0f - sigmoidProgress (expoCam));
    }

    if (expoCam <= 0 || (expoCam > 0.0 && expoCam < 1.0 &&
			 optionGetExpoAnimation () != ExpoAnimationZoom))
	gScreen->glPaintTransformedOutput (attrib, transform, region,
					   output, mask);
    else
	gScreen->clearOutput (output, GL_COLOR_BUFFER_BIT);

    mask &= ~PAINT_SCREEN_CLEAR_MASK;

    if (expoCam > 0.0)
    {
	if (optionGetReflection ())
	    paintWall (attrib, transform, region, output, mask, true);

	paintWall (attrib, transform, region, output, mask, false);
	anyClick = false;
    }
}

bool
ExpoWindow::glDraw (const GLMatrix            &transform,
		    const GLWindowPaintAttrib &attrib,
		    const CompRegion          &region,
		    unsigned int              mask)
{
    if (eScreen->expoCam == 0.0f)
	return gWindow->glDraw (transform, attrib, region, mask);

    // Scaling factors to be applied to attrib later in glDrawTexture
    expoOpacity = 1.0f;

    int expoAnimation = eScreen->optionGetExpoAnimation ();

    if (eScreen->expoActive)
    {
	if (expoAnimation != ExpoScreen::ExpoAnimationZoom)
	    expoOpacity = eScreen->expoCam;

	if (window->wmType () & CompWindowTypeDockMask &&
	    eScreen->optionGetHideDocks ())
	{
	    if (expoAnimation == ExpoScreen::ExpoAnimationZoom &&
		eScreen->paintingVp == eScreen->selectedVp)
		expoOpacity = (1.0f - sigmoidProgress (eScreen->expoCam));
	    else
		expoOpacity = 0.0f;
	}
    }

    bool status = gWindow->glDraw (transform, attrib, region, mask);

    if (window->type () & CompWindowTypeDesktopMask &&
	eScreen->optionGetSelectedColor ()[3]	    &&  // colour is visible
	mGlowQuads				    &&
	eScreen->paintingVp == eScreen->selectedVp  &&
	region.numRects ())
    {
	/* reset geometry and paint */
	gWindow->vertexBuffer ()->begin ();
	gWindow->vertexBuffer ()->end ();
	paintGlow (transform, attrib, infiniteRegion, mask);
    }

    return status;
}

static const unsigned short EXPO_GRID_SIZE = 100;

void
ExpoWindow::glAddGeometry (const GLTexture::MatrixList &matrices,
			   const CompRegion            &region,
			   const CompRegion            &clip,
			   unsigned int                maxGridWidth,
			   unsigned int                maxGridHeight)
{
    if (eScreen->expoCam > 0.0		&&
	screen->desktopWindowCount ()	&&
	eScreen->optionGetDeform () == ExpoScreen::DeformCurve)
    {
	gWindow->glAddGeometry (matrices, region, clip,
				MIN (maxGridWidth, EXPO_GRID_SIZE),
				maxGridHeight);

	int     stride    = gWindow->vertexBuffer ()->getVertexStride ();
	int     oldVCount = gWindow->vertexBuffer ()->countVertices ();
	GLfloat *v        = gWindow->vertexBuffer ()->getVertices ();

	v += stride - 3;
	v += stride * oldVCount;

	CompPoint offset;

	if (!window->onAllViewports ())
	{
	    offset = eScreen->cScreen->windowPaintOffset ();
	    offset = window->getMovementForOffset (offset);
	}

	float       ang;
	float       lastX     = -1000000000.0f;
	float       lastZ     = 0.0f;
	const float radSquare = pow (eScreen->curveDistance, 2) + 0.25;

	for (int i = oldVCount; i < gWindow->vertexBuffer ()->countVertices (); ++i)
	{
	    if (v[0] == lastX)
		v[2] = lastZ;
	    else if (v[0] + offset.x () >= -EXPO_GRID_SIZE &&
		     v[0] + offset.x () < screen->width () + EXPO_GRID_SIZE)
	    {
		ang  = ((v[0] + offset.x ()) /
		       static_cast <float> (screen->width ())) - 0.5;
		ang *= ang;

		if (ang < radSquare)
		{
		    v[2]  = eScreen->curveDistance - sqrt (radSquare - ang);
		    v[2] *= sigmoidProgress (eScreen->expoCam);
		}
	    }

	    lastX = v[0];
	    lastZ = v[2];

	    v += stride;
	}
    }
    else
	gWindow->glAddGeometry (matrices, region, clip, maxGridWidth, maxGridHeight);
}

void
ExpoWindow::glDrawTexture (GLTexture                 *texture,
			   const GLMatrix            &transform,
			   const GLWindowPaintAttrib &attrib,
			   unsigned int              mask)
{
    GLWindowPaintAttrib wAttrib (attrib);

    if (eScreen->expoCam > 0.0)
    {
	wAttrib.opacity    *= expoOpacity;
	wAttrib.brightness *= eScreen->vpBrightness;
	wAttrib.saturation *= eScreen->vpSaturation;
    }

    if (eScreen->expoCam > 0.0					&&
	eScreen->optionGetDeform () == ExpoScreen::DeformCurve	&&
	eScreen->gScreen->lighting ()				&&
	screen->desktopWindowCount ())
    {
	CompPoint offset;

	if (!window->onAllViewports ())
	{
	    offset = eScreen->cScreen->windowPaintOffset ();
	    offset = window->getMovementForOffset (offset);
	}

	GLVertexBuffer *vb    = gWindow->vertexBuffer ();
	int            stride = vb->getVertexStride ();
	GLfloat        *v     = vb->getVertices () + stride - 3;
	GLfloat        normal[3];
	int            idx;
	float          x;

	for (int i = 0; i < vb->countVertices (); ++i)
	{
	    x = (v[0] + offset.x () - screen->width () / 2) *
		    eScreen->curveAngle / screen->width ();

	    while (x < 0)
		x += 360.0;

	    idx = floor (x);

	    normal[0] = -eScreen->vpNormals[idx * 3];
	    normal[1] = eScreen->vpNormals[(idx * 3) + 1];
	    normal[2] = eScreen->vpNormals[(idx * 3) + 2];
	    vb->addNormals (1, normal);

	    v += stride;
	}

/* I am not entirely certain if these ifdefs are necessary
 * since we should be doing normalization in the shader,
 * however I have them here for now */
#ifndef USE_GLES
	glEnable (GL_NORMALIZE);
#endif
	gWindow->glDrawTexture (texture, transform, wAttrib, mask);
#ifndef USE_GLES
	glDisable (GL_NORMALIZE);
#endif
    }
    else
    {
//	glEnable (GL_NORMALIZE);
	gWindow->glDrawTexture (texture, transform, wAttrib, mask);
//	glDisable (GL_NORMALIZE);
    }
}

bool
ExpoWindow::glPaint (const GLWindowPaintAttrib &attrib,
		     const GLMatrix            &transform,
		     const CompRegion          &region,
		     unsigned int              mask)
{
    GLMatrix            wTransform (transform);

    if (eScreen->expoActive)
    {
	if (eScreen->expoCam > 0.0)
	    mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

	float opacity  = 1.0;
	bool  zoomAnim = eScreen->optionGetExpoAnimation () ==
			 ExpoScreen::ExpoAnimationZoom;
	bool  hide     = eScreen->optionGetHideDocks () &&
			 (window->wmType () & CompWindowTypeDockMask);

	if (!zoomAnim)
	    opacity = attrib.opacity * eScreen->expoCam;

	if (hide)
	{
	    if (zoomAnim && eScreen->paintingVp == eScreen->selectedVp)
		opacity = attrib.opacity *
			  (1 - sigmoidProgress (eScreen->expoCam));
	    else
		opacity = 0;
	}

	if (opacity <= 0)
	    mask |= PAINT_WINDOW_NO_CORE_INSTANCE_MASK;

	/* Stretch maximized windows a little so that you don't
	 * have an awkward gap */
	if (((window->state () & MAXIMIZE_STATE) == MAXIMIZE_STATE) &&
	    (eScreen->dndWindow != window))
	{
	    CompOutput *o = &screen->outputDevs ()[screen->outputDeviceForGeometry(window->geometry())];
	    float yS = 1.0 + ((o->height () / (float) window->height ()) - 1.0f) * sigmoidProgress (eScreen->expoCam);
	    float xS = 1.0 + ((o->width () / (float) window->width ()) - 1.0f) * sigmoidProgress (eScreen->expoCam);
	    wTransform.translate (window->x () + window->width (),
				  window->y () + window->height (),
				  0.0f);
	    wTransform.scale (xS, yS, 1.0f);
	    wTransform.translate (-(window->x () + window->width ()),
				  -(window->y () + window->height ()),
				  0.0f);
	}
    }

    return gWindow->glPaint (attrib, wTransform, region, mask);
}

bool
ExpoWindow::damageRect (bool            initial,
			const CompRect  &rect)
{
    if (eScreen->expoCam > 0.0f)
	eScreen->cScreen->damageScreen ();

    return cWindow->damageRect (initial, rect);
}

void
ExpoWindow::resizeNotify (int dx, int dy, int dwidth, int dheight)
{
    window->resizeNotify (dx, dy, dwidth, dheight);

    if (!(window->type () & CompWindowTypeDesktopMask))
    {
	compLogMessage ("expo", CompLogLevelWarn, "Received a resizeNotify "\
						  "for a non-desktop window.");
	assert (window->type () & CompWindowTypeDesktopMask);
	return;
    }

    /* Desktop window was resized. Update our glowQuads. */
    foreach (GLTexture *tex, eScreen->outline_texture)
    {
	GLTexture::Matrix mat = tex->matrix ();
	computeGlowQuads (&mat);
    }
}

#define EXPOINITBIND(opt, func)                                \
    optionSet##opt##Initiate (boost::bind (&ExpoScreen::func,  \
					   this, _1, _2, _3));
#define EXPOTERMBIND(opt, func)                                \
    optionSet##opt##Terminate (boost::bind (&ExpoScreen::func, \
					    this, _1, _2, _3));

ExpoScreen::ExpoScreen (CompScreen *s) :
    PluginClassHandler<ExpoScreen, CompScreen> (s),
    ExpoOptions (),
    cScreen                (CompositeScreen::get (s)),
    gScreen                (GLScreen::get (s)),
    expoCam                (0.0f),
    expoActive             (false),
    expoMode               (false),
    dndState               (DnDNone),
    dndWindow              (NULL),
    origVp                 (s->vp ()),
    selectedVp             (s->vp ()),
    lastSelectedVp         (s->vp ()),
    vpUpdateMode           (VPUpdateNone),
    clickTime              (0),
    doubleClick            (false),
    vpNormals              (360 * 3),
    grabIndex              (0),
    mGlowTextureProperties (&glowTextureProperties)
{
    leftKey  = XKeysymToKeycode (s->dpy (), XStringToKeysym ("Left"));
    rightKey = XKeysymToKeycode (s->dpy (), XStringToKeysym ("Right"));
    upKey    = XKeysymToKeycode (s->dpy (), XStringToKeysym ("Up"));
    downKey  = XKeysymToKeycode (s->dpy (), XStringToKeysym ("Down"));

    dragCursor = XCreateFontCursor (screen->dpy (), XC_fleur);

    EXPOINITBIND (ExpoKey, doExpo);
    EXPOTERMBIND (ExpoKey, termExpo);
    EXPOINITBIND (ExpoButton, doExpo);
    EXPOTERMBIND (ExpoButton, termExpo);
    EXPOINITBIND (ExpoEdge, doExpo);
    EXPOTERMBIND (ExpoButton, termExpo);

    EXPOINITBIND (DndButton, dndInit);
    EXPOTERMBIND (DndButton, dndFini);
    EXPOINITBIND (ExitButton, exitExpo);
    EXPOINITBIND (NextVpButton, nextVp);
    EXPOINITBIND (PrevVpButton, prevVp);

    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);

    outline_texture = GLTexture::imageDataToTexture (mGlowTextureProperties->textureData,
						     CompSize (mGlowTextureProperties->textureSize,
							       mGlowTextureProperties->textureSize),
						     GL_RGBA, GL_UNSIGNED_BYTE);
}

ExpoScreen::~ExpoScreen ()
{
    if (dragCursor != None)
	XFreeCursor (screen->dpy (), dragCursor);
}

ExpoWindow::ExpoWindow (CompWindow *w) :
    PluginClassHandler<ExpoWindow, CompWindow> (w),
    window      (w),
    cWindow     (CompositeWindow::get (w)),
    gWindow     (GLWindow::get (w)),
    eScreen     (ExpoScreen::get (screen)),
    mGlowQuads  (NULL),
    expoOpacity (1.0f)
{
    WindowInterface::setHandler (window, false);
    CompositeWindowInterface::setHandler (cWindow, false);
    GLWindowInterface::setHandler (gWindow, false);

    if (window->type () & CompWindowTypeDesktopMask)
    {
	foreach (GLTexture *tex, eScreen->outline_texture)
	{
	    GLTexture::Matrix mat = tex->matrix ();
	    computeGlowQuads (&mat);
	}

	window->resizeNotifySetEnabled (this, true);
    }
}

ExpoWindow::~ExpoWindow ()
{
    computeGlowQuads (NULL);
}

bool
ExpoPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	return true;

    return false;
}
