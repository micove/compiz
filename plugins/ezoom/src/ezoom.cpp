/*
 * Copyright © 2005 Novell, Inc.
 * Copyright (C) 2007, 2008,2010 Kristian Lyngstøl
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
 *
 * Author(s):
 *	- Most features beyond basic zoom;
 *	  Kristian Lyngstol <kristian@bohemians.org>
 *	- Original zoom plug-in; David Reveman <davidr@novell.com>
 *	- Original port to C++ by Sam Spilsbury <smspillaz@gmail.com>
 *
 * Description:
 *
 * This plug-in offers zoom functionality with focus tracking,
 * fit-to-window actions, mouse panning, zoom area locking. Without
 * disabling input.
 *
 * Note on actual zoom process
 *
 * The animation is done in preparePaintScreen, while instant movements
 * are done by calling updateActualTranslate () after updating the
 * translations. This causes [xyz]trans to be re-calculated. We keep track
 * of each head separately.
 *
 * Note on input
 *
 * We can not redirect input yet, but this plug-in offers two fundamentally
 * different approaches to achieve input enabled zoom:
 *
 * 1.
 * Always have the zoomed area be in sync with the mouse cursor. This binds
 * the zoom area to the mouse position at any given time. It allows using
 * the original mouse cursor drawn by X, and is technically very safe.
 * First used in Beryl's inputzoom.
 *
 * 2.
 * Hide the real cursor and draw our own where it would be when zoomed in.
 * This allows us to navigate with the mouse without constantly moving the
 * zoom area. This is fairly close to what we want in the end when input
 * redirection is available.
 *
 * This second method has one huge issue, which is bugged XFixes. After
 * hiding the cursor once with XFixes, some mouse cursors will simply be
 * invisible. The Firefox loading cursor being one of them.
 *
 * An other minor annoyance is that mouse sensitivity seems to increase as
 * you zoom in, since the mouse isn't really zoomed at all.
 *
 * Todo:
 *  - Walk through C++ port and adjust comments for 2010.
 *  - See if anyone misses the filter setting
 *  - Verify XFixes fix... err.
 *  - Different multi head modes
 */

#include "ezoom.h"

COMPIZ_PLUGIN_20090315 (ezoom, ZoomPluginVTable)


/*
 * This toggles paint functions. We don't need to continually run code when we
 * are not doing anything.
 */
static inline void
toggleFunctions (bool state)
{
    ZOOM_SCREEN (screen);

    screen->handleEventSetEnabled (zs, state);
    zs->cScreen->preparePaintSetEnabled (zs, state);
    zs->gScreen->glPaintOutputSetEnabled (zs, state);
    zs->cScreen->donePaintSetEnabled (zs, state);
}

/* Check if the output is valid */
static inline bool
outputIsZoomArea (int out)
{
    ZOOM_SCREEN (screen);

    if (out < 0)
	return false;
    else if ((unsigned int) out >= zs->zooms.size ())
	zs->zooms.resize (screen->outputDevs ().size ());

    return true;
}

/* Check if zoom is active on the output specified */
static inline bool
isActive (int out)
{
    ZOOM_SCREEN (screen);

    if (!outputIsZoomArea (out))
	return false;

    if (zs->grabbed & (1 << zs->zooms.at (out).output))
	return true;

    return false;
}

/* Check if we are zoomed out and not going anywhere
 * (similar to isActive but based on actual zoom, not grab).
 */
static inline bool
isZoomed (int out)
{
    ZOOM_SCREEN (screen);

    if (!outputIsZoomArea (out))
	return false;

    if (zs->zooms.at (out).currentZoom != 1.0f	||
	zs->zooms.at (out).newZoom     != 1.0f	||
	zs->zooms.at (out).zVelocity   != 0.0f)
	return true;

    return false;
}

/* Returns the distance to the defined edge in zoomed pixels.  */
int
EZoomScreen::distanceToEdge (int                   out,
			     EZoomScreen::ZoomEdge edge)
{
    CompOutput *o = &screen->outputDevs ().at (out);

    if (!isActive (out))
	return 0;

    int x1, y1, x2, y2;

    convertToZoomedTarget (out, o->region ()->extents.x2,
			   o->region ()->extents.y2, &x2, &y2);
    convertToZoomedTarget (out, o->region ()->extents.x1,
			   o->region ()->extents.y1, &x1, &y1);
    switch (edge)
    {
	case WEST:  return o->region ()->extents.x1 - x1;
	case NORTH: return o->region ()->extents.y1 - y1;
	case EAST:  return x2 - o->region ()->extents.x2;
	case SOUTH: return y2 - o->region ()->extents.y2;
    }

    return 0; // Never reached.
}

/* Update/set translations based on zoom level and real translate. */
void
EZoomScreen::ZoomArea::updateActualTranslates ()
{
    xtrans = -realXTranslate * (1.0f - currentZoom);
    ytrans =  realYTranslate * (1.0f - currentZoom);
}

/* Returns true if the head in question is currently moving.
 * Since we don't always bother resetting everything when
 * canceling zoom, we check for the condition of being completely
 * zoomed out and not zooming in/out first.
 */
bool
EZoomScreen::isInMovement (int out)
{
    if (zooms.at (out).currentZoom == 1.0f  &&
	zooms.at (out).newZoom     == 1.0f  &&
	zooms.at (out).zVelocity   == 0.0f)
	return false;

    if (zooms.at (out).currentZoom != zooms.at (out).newZoom	    ||
	zooms.at (out).xVelocity				    ||
	zooms.at (out).yVelocity				    ||
	zooms.at (out).zVelocity				    ||
	zooms.at (out).xTranslate != zooms.at (out).realXTranslate  ||
	zooms.at (out).yTranslate != zooms.at (out).realYTranslate)
	return true;

    return false;
}

/* Set the initial values of a zoom area.  */
EZoomScreen::ZoomArea::ZoomArea (int out) :
    output (out),
    viewport (~0),
    currentZoom (1.0f),
    newZoom (1.0f),
    xVelocity (0.0f),
    yVelocity (0.0f),
    zVelocity (0.0f),
    xTranslate (0.0f),
    yTranslate (0.0f),
    realXTranslate (0.0f),
    realYTranslate (0.0f),
    xtrans (0.0f),
    ytrans (0.0f),
    locked (false)
{
    updateActualTranslates ();
}

EZoomScreen::ZoomArea::ZoomArea () :
    output (0),
    viewport (~0),
    currentZoom (1.0f),
    newZoom (1.0f),
    xVelocity (0.0f),
    yVelocity (0.0f),
    zVelocity (0.0f),
    xTranslate (0.0f),
    yTranslate (0.0f),
    realXTranslate (0.0f),
    realYTranslate (0.0f),
    xtrans (0.0f),
    ytrans (0.0f),
    locked (false)
{
}

/* Adjust the velocity in the z-direction. */
void
EZoomScreen::adjustZoomVelocity (int   out,
				 float chunk)
{
    float d      = (zooms.at (out).newZoom - zooms.at (out).currentZoom) * 75.0f;
    float adjust = d * 0.002f;
    float amount = fabs (d);

    if (amount < 1.0f)
	amount = 1.0f;
    else if (amount > 5.0f)
	amount = 5.0f;

    zooms.at (out).zVelocity = (amount * zooms.at (out).zVelocity + adjust) /
			       (amount + 1.0f);

    if (fabs (d) < 0.1f && fabs (zooms.at (out).zVelocity) < 0.005f)
    {
	zooms.at (out).currentZoom = zooms.at (out).newZoom;
	zooms.at (out).zVelocity   = 0.0f;
    }
    else
	zooms.at (out).currentZoom += (zooms.at (out).zVelocity * chunk) /
				      cScreen->redrawTime ();
}

/* Adjust the X/Y velocity based on target translation and real
 * translation. */
void
EZoomScreen::adjustXYVelocity (int   out,
			       float chunk)
{
    zooms.at (out).xVelocity /= 1.25f;
    zooms.at (out).yVelocity /= 1.25f;

    float xdiff =
	(zooms.at (out).xTranslate - zooms.at (out).realXTranslate) * 75.0f;
    float ydiff =
	(zooms.at (out).yTranslate - zooms.at (out).realYTranslate) * 75.0f;

    float xadjust = xdiff * 0.002f;
    float yadjust = ydiff * 0.002f;
    float xamount = fabs (xdiff);
    float yamount = fabs (ydiff);

    if (xamount < 1.0f)
	xamount = 1.0f;
    else if (xamount > 5.0)
	xamount = 5.0f;

    if (yamount < 1.0f)
	yamount = 1.0f;
    else if (yamount > 5.0)
	yamount = 5.0f;

    zooms.at (out).xVelocity =
	(xamount * zooms.at (out).xVelocity + xadjust) / (xamount + 1.0f);
    zooms.at (out).yVelocity =
	(yamount * zooms.at (out).yVelocity + yadjust) / (yamount + 1.0f);

    if ((fabs(xdiff) < 0.1f && fabs (zooms.at (out).xVelocity) < 0.005f) &&
	(fabs(ydiff) < 0.1f && fabs (zooms.at (out).yVelocity) < 0.005f))
    {
	zooms.at (out).realXTranslate = zooms.at (out).xTranslate;
	zooms.at (out).realYTranslate = zooms.at (out).yTranslate;
	zooms.at (out).xVelocity      = 0.0f;
	zooms.at (out).yVelocity      = 0.0f;
	return;
    }

    zooms.at (out).realXTranslate +=
	(zooms.at (out).xVelocity * chunk) / cScreen->redrawTime ();
    zooms.at (out).realYTranslate +=
	(zooms.at (out).yVelocity * chunk) / cScreen->redrawTime ();
}

/* Animate the movement (if any) in preparation of a paint screen. */
void
EZoomScreen::preparePaint (int msSinceLastPaint)
{
    if (grabbed)
    {
	float amount = msSinceLastPaint * 0.05f * optionGetSpeed ();
	int   steps  = amount / (0.5f * optionGetTimestep ());

	if (!steps)
	    steps = 1;

	float chunk  = amount / (float) steps;

	while (steps--)
	{
	    for (unsigned int out = 0; out < zooms.size (); ++out)
	    {
		if (!isInMovement (out) || !isActive (out))
		    continue;

		adjustXYVelocity (out, chunk);
		adjustZoomVelocity (out, chunk);
		zooms.at (out).updateActualTranslates ();

		if (!isZoomed (out))
		{
		    zooms.at (out).xVelocity = zooms.at (out).yVelocity = 0.0f;
		    grabbed &= ~(1 << zooms.at (out).output);

		    if (!grabbed)
		    {
			cScreen->damageScreen ();
			toggleFunctions (false);
		    }
		}
	    }
	}

	if (optionGetZoomMode () == EzoomOptions::ZoomModeSyncMouse)
	    syncCenterToMouse ();
    }

    cScreen->preparePaint (msSinceLastPaint);
}

/* Damage screen if we're still moving. */
void
EZoomScreen::donePaint ()
{
    if (grabbed)
    {
	for (unsigned int out = 0; out < zooms.size (); ++out)
	{
	    if (isInMovement (out) && isActive (out))
	    {
		cScreen->damageScreen ();
		break;
	    }
	}
    }
    else if (grabIndex)
	cScreen->damageScreen ();
    else
	toggleFunctions (false);

    cScreen->donePaint ();
}

/* Draws a box from the screen coordinates inx1, iny1 to inx2, iny2. */
void
EZoomScreen::drawBox (const GLMatrix &transform,
		      CompOutput     *output,
		      CompRect       box)
{
    GLMatrix       zTransform (transform);
    int            inx1, inx2, iny1, iny2;
    int            out = output->id ();
    GLushort       colorData[4];
    GLfloat        vertexData[12];
    GLVertexBuffer *streamingBuffer = GLVertexBuffer::streamingBuffer ();

    zTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);
    convertToZoomed (out, box.x1 (), box.y1 (), &inx1, &iny1);
    convertToZoomed (out, box.x2 (), box.y2 (), &inx2, &iny2);

    /* We can move in both directions from our starting point
     * so we need to calculate the right coordinates first. */
    int x1 = MIN (inx1, inx2);
    int y1 = MIN (iny1, iny2);
    int x2 = MAX (inx1, inx2);
    int y2 = MAX (iny1, iny2);

    const float MaxUShortFloat = std::numeric_limits <unsigned short>::max ();

    GLboolean glBlendEnabled = glIsEnabled (GL_BLEND);

    /* just enable blending if it is disabled */
    if (!glBlendEnabled)
	glEnable (GL_BLEND);

    /* Draw filled rectangle */
    float    alpha  = optionGetZoomBoxFillColorAlpha () / MaxUShortFloat;
    GLushort *color = optionGetZoomBoxFillColor ();

    colorData[0] = alpha * color[0];
    colorData[1] = alpha * color[1];
    colorData[2] = alpha * color[2];
    colorData[3] = alpha * MaxUShortFloat;

    vertexData[0]  = x1;
    vertexData[1]  = y1;
    vertexData[2]  = 0.0f;
    vertexData[3]  = x1;
    vertexData[4]  = y2;
    vertexData[5]  = 0.0f;
    vertexData[6]  = x2;
    vertexData[7]  = y1;
    vertexData[8]  = 0.0f;
    vertexData[9]  = x2;
    vertexData[10] = y2;
    vertexData[11] = 0.0f;

    /* fill rectangle */
    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    streamingBuffer->addColors (1, colorData);
    streamingBuffer->addVertices (4, vertexData);

    streamingBuffer->end ();
    streamingBuffer->render (zTransform);

    /* draw rectangle outline */
    alpha = optionGetZoomBoxOutlineColorAlpha () / MaxUShortFloat;
    color = optionGetZoomBoxOutlineColor ();

    colorData[0] = alpha * color[0];
    colorData[1] = alpha * color[1];
    colorData[2] = alpha * color[2];
    colorData[3] = alpha * MaxUShortFloat;

    vertexData[0]  = x1;
    vertexData[1]  = y1;
    vertexData[2]  = 0.0f;
    vertexData[3]  = x2;
    vertexData[4]  = y1;
    vertexData[5]  = 0.0f;
    vertexData[6]  = x2;
    vertexData[7]  = y2;
    vertexData[8]  = 0.0f;
    vertexData[9]  = x1;
    vertexData[10] = y2;
    vertexData[11] = 0.0f;

    glLineWidth (2.0);

    streamingBuffer->begin (GL_LINE_LOOP);

    streamingBuffer->addColors (1, colorData);
    streamingBuffer->addVertices (4, vertexData);

    streamingBuffer->end ();
    streamingBuffer->render (zTransform);

    /* just disable blending if it was disabled before */
    if (!glBlendEnabled)
	glDisable (GL_BLEND);

    /* Damage the zoom selection box region during draw. */
    cScreen->damageRegion (CompRegion (x1 - 1,
				       y1 - 1,
				       x2 - x1 + 1,
				       y2 - y1 + 1));
}

/* Apply the zoom if we are grabbed.
 * Make sure to use the correct filter.
 */
bool
EZoomScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			    const GLMatrix            &transform,
			    const CompRegion          &region,
			    CompOutput                *output,
			    unsigned int              mask)
{
    bool status;
    int  out = output->id ();

    if (isActive (out))
    {
	GLScreenPaintAttrib sa         = attrib;
	GLMatrix            zTransform = transform;

	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_CLEAR_MASK;

	zTransform.scale (1.0f / zooms.at (out).currentZoom,
			  1.0f / zooms.at (out).currentZoom,
			  1.0f);
	zTransform.translate (zooms.at (out).xtrans,
			      zooms.at (out).ytrans,
			      0);

	mask |= PAINT_SCREEN_TRANSFORMED_MASK;

	status = gScreen->glPaintOutput (sa, zTransform, region, output, mask);

	drawCursor (output, transform);
    }
    else
	status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    if (grabIndex)
	drawBox (transform, output, box);

    return status;
}

/* Makes sure we're not attempting to translate too far.
 * We are restricted to 0.5 to not go beyond the end
 * of the screen/head. */
static inline void
constrainZoomTranslate ()
{
    ZOOM_SCREEN (screen);

    for (unsigned int out = 0; out < zs->zooms.size (); ++out)
    {
	if (zs->zooms.at (out).xTranslate > 0.5f)
	    zs->zooms.at (out).xTranslate = 0.5f;
	else if (zs->zooms.at (out).xTranslate < -0.5f)
	    zs->zooms.at (out).xTranslate = -0.5f;

	if (zs->zooms.at (out).yTranslate > 0.5f)
	    zs->zooms.at (out).yTranslate = 0.5f;
	else if (zs->zooms.at (out).yTranslate < -0.5f)
	    zs->zooms.at (out).yTranslate = -0.5f;
    }
}

/* Functions for adjusting the zoomed area.
 * These are the core of the zoom plug-in; Anything wanting
 * to adjust the zoomed area must use setCenter or setZoomArea
 * and setScale or front ends to them. */

/* Sets the center of the zoom area to X,Y.
 * We have to be able to warp the pointer here: If we are moved by
 * anything except mouse movement, we have to sync the
 * mouse pointer. This is to allow input, and is NOT necessary
 * when input redirection is available to us or if we're cheating
 * and using a scaled mouse cursor to imitate IR.
 * The center is not the center of the screen. This is the target-center;
 * that is, it's the point that's the same regardless of zoom level.
 */
void
EZoomScreen::setCenter (int  x,
			int  y,
			bool instant)
{
    int         out = screen->outputDeviceForPoint (x, y);
    CompOutput  *o  = &screen->outputDevs ().at (out);

    if (zooms.at (out).locked)
	return;

    zooms.at (out).xTranslate = (float)
	((x - o->x1 ()) - o->width ()  / 2) / (o->width ());
    zooms.at (out).yTranslate = (float)
	((y - o->y1 ()) - o->height () / 2) / (o->height ());

    if (instant)
    {
	zooms.at (out).realXTranslate = zooms.at (out).xTranslate;
	zooms.at (out).realYTranslate = zooms.at (out).yTranslate;
	zooms.at (out).yVelocity = 0.0f;
	zooms.at (out).xVelocity = 0.0f;
	zooms.at (out).updateActualTranslates ();
    }

    if (optionGetZoomMode () == EzoomOptions::ZoomModePanArea)
	restrainCursor (out);
}

/* Zooms the area described.
 * The math could probably be cleaned up, but should be correct now. */
void
EZoomScreen::setZoomArea (int  x,
			  int  y,
			  int  width,
			  int  height,
			  bool instant)
{
    CompWindow::Geometry outGeometry (x, y, width, height, 0);
    int out = screen->outputDeviceForGeometry (outGeometry);

    if (zooms.at (out).newZoom == 1.0f ||
	zooms.at (out).locked)
	return;

    CompOutput *o = &screen->outputDevs ().at (out);

    zooms.at (out).xTranslate =
	 (float) -((o->width () / 2) - (x + (width / 2) - o->x1 ()))
	/ (o->width ());
    zooms.at (out).xTranslate /= (1.0f - zooms.at (out).newZoom);
    zooms.at (out).yTranslate =
	(float) -((o->height () / 2) - (y + (height / 2) - o->y1 ()))
	/ (o->height ());
    zooms.at (out).yTranslate /= (1.0f - zooms.at (out).newZoom);
    constrainZoomTranslate ();

    if (instant)
    {
	zooms.at (out).realXTranslate = zooms.at (out).xTranslate;
	zooms.at (out).realYTranslate = zooms.at (out).yTranslate;
	zooms.at (out).updateActualTranslates ();
    }

    if (optionGetZoomMode () == EzoomOptions::ZoomModePanArea)
	restrainCursor (out);
}

/* Moves the zoom area to the window specified */
void
EZoomScreen::areaToWindow (CompWindow *w)
{
    int left   = w->serverX () - w->border ().left;
    int top    = w->serverY () - w->border ().top;
    int width  = w->width ()   + w->border ().left + w->border ().right;
    int height = w->height ()  + w->border ().top  + w->border ().bottom;

    setZoomArea (left, top, width, height, false);
}

/* Pans the zoomed area vertically/horizontally by * value * zs->panFactor
 * TODO: Fix output. */
void
EZoomScreen::panZoom (int xvalue,
		      int yvalue)
{
    float panFactor = optionGetPanFactor ();

    for (unsigned int out = 0; out < zooms.size (); ++out)
    {
	zooms.at (out).xTranslate += panFactor * xvalue * zooms.at (out).currentZoom;
	zooms.at (out).yTranslate += panFactor * yvalue * zooms.at (out).currentZoom;
    }

    constrainZoomTranslate ();
}

/* Enables polling of mouse position, and refreshes currently
 * stored values.
 */
void
EZoomScreen::enableMousePolling ()
{
    pollHandle.start ();
    lastChange = time(NULL);
    mouse      = MousePoller::getCurrentPosition ();
}

/* Sets the zoom (or scale) level.
 * Cleans up if we are suddenly zoomed out.
 */
void
EZoomScreen::setScale (int   out,
		       float value)
{
    if (zooms.at (out).locked)
	return;

    if (value >= 1.0f)
	value = 1.0f;
    else
    {
	if (!pollHandle.active ())
	    enableMousePolling ();

	grabbed |= (1 << zooms.at (out).output);
	cursorZoomActive (out);
    }

    if (value == 1.0f)
    {
	zooms.at (out).xTranslate = 0.0f;
	zooms.at (out).yTranslate = 0.0f;
	cursorZoomInactive ();
    }

    if (value < optionGetMinimumZoom ())
	value = optionGetMinimumZoom ();

    zooms.at (out).newZoom = value;
    cScreen->damageScreen();
}

/* Sets the zoom factor to the bigger of the two floats supplied.
 * Convenience function for setting the scale factor for an area.
 */
static inline void
setScaleBigger (int   out,
		float x,
		float y)
{
    ZOOM_SCREEN (screen);
    zs->setScale (out, x > y ? x : y);
}

/* Mouse code...
 * This takes care of keeping the mouse in sync with the zoomed area and
 * vice versa.
 * See heading for description.
 */

/* Syncs the center, based on translations, back to the mouse.
 * This should be called when doing non-IR zooming and moving the zoom
 * area based on events other than mouse movement.
 */
void
EZoomScreen::syncCenterToMouse ()
{
    int out = screen->outputDeviceForPoint (mouse.x (), mouse.y ());

    if (!isInMovement (out))
	return;

    CompOutput *o = &screen->outputDevs ().at (out);

    int x = (int) ((zooms.at (out).realXTranslate * o->width ()) +
		   (o->width () / 2) + o->x1 ());
    int y = (int) ((zooms.at (out).realYTranslate * o->height ()) +
		   (o->height () / 2) + o->y1 ());

    if ((x != mouse.x () || y != mouse.y ())	&&
	grabbed					&&
	zooms.at (out).newZoom != 1.0f)
    {
	screen->warpPointer (x - pointerX , y - pointerY );
	mouse.setX (x);
	mouse.setY (y);
    }
}

/* Convert the point X, Y to where it would be when zoomed. */
void
EZoomScreen::convertToZoomed (int out,
			      int x,
			      int y,
			      int *resultX,
			      int *resultY)
{
    if (!outputIsZoomArea (out))
    {
	*resultX = x;
	*resultY = y;
    }

    CompOutput *o          = &screen->outputDevs ()[out];
    ZoomArea   &za         = zooms.at (out);
    int        oWidth      = o->width ();
    int        oHeight     = o->height ();
    int        halfOWidth  = oWidth  / 2;
    int        halfOHeight = oHeight / 2;

    x -= o->x1 ();
    y -= o->y1 ();

    *resultX = x - (za.realXTranslate *
		    (1.0f - za.currentZoom) * oWidth) - halfOWidth;
    *resultX /= za.currentZoom;
    *resultX += halfOWidth;
    *resultX += o->x1 ();
    *resultY = y - (za.realYTranslate *
		    (1.0f - za.currentZoom) * oHeight) - halfOHeight;
    *resultY /= za.currentZoom;
    *resultY += halfOHeight;
    *resultY += o->y1 ();
}

/* Same but use targeted translation, not real one. */
void
EZoomScreen::convertToZoomedTarget (int out,
				    int x,
				    int y,
				    int *resultX,
				    int *resultY)
{
    if (!outputIsZoomArea (out))
    {
	*resultX = x;
	*resultY = y;
    }

    CompOutput *o          = &screen->outputDevs ().at (out);
    ZoomArea   &za         = zooms.at (out);
    int        oWidth      = o->width ();
    int        oHeight     = o->height ();
    int        halfOWidth  = oWidth  / 2;
    int        halfOHeight = oHeight / 2;

    x -= o->x1 ();
    y -= o->y1 ();

    *resultX = x - (za.xTranslate * (1.0f - za.newZoom) * oWidth)  - halfOWidth;
    *resultX /= za.newZoom;
    *resultX += halfOWidth;
    *resultX += o->x1 ();
    *resultY = y - (za.yTranslate * (1.0f - za.newZoom) * oHeight) - halfOHeight;
    *resultY /= za.newZoom;
    *resultY += halfOHeight;
    *resultY += o->y1 ();
}

/* Make sure the given point + margin is visible;
 * Translate to make it visible if necessary.
 * Returns false if the point isn't on a actively zoomed head
 * or the area is locked. */
bool
EZoomScreen::ensureVisibility (int x,
			       int y,
			       int margin)
{
    int out = screen->outputDeviceForPoint (x, y);

    if (!isActive (out))
	return false;

    int zoomX, zoomY;
    convertToZoomedTarget (out, x, y, &zoomX, &zoomY);
    ZoomArea &za = zooms.at (out);

    if (za.locked)
	return false;

    CompOutput *o = &screen->outputDevs ().at (out);

#define FACTOR (za.newZoom / (1.0f - za.newZoom))
    if (zoomX + margin > o->x2 ())
	za.xTranslate +=
	    (FACTOR * (float) (zoomX + margin - o->x2 ())) /
	    (float) o->width ();
    else if (zoomX - margin < o->x1 ())
	za.xTranslate +=
	    (FACTOR * (float) (zoomX - margin - o->x1 ())) /
	    (float) o->width ();

    if (zoomY + margin > o->y2 ())
	za.yTranslate +=
	    (FACTOR * (float) (zoomY + margin - o->y2 ())) /
	    (float) o->height ();
    else if (zoomY - margin < o->y1 ())
	za.yTranslate +=
	    (FACTOR * (float) (zoomY - margin - o->y1 ())) /
	    (float) o->height ();
#undef FACTOR
    constrainZoomTranslate ();
    return true;
}

/* Attempt to ensure the visibility of an area defined by x1/y1 and x2/y2.
 * See ensureVisibility () for details.
 *
 * This attempts to find the translations that leaves the biggest part of
 * the area visible.
 *
 * gravity defines what part of the window that should get
 * priority if it isn't possible to fit all of it.
 */
void
EZoomScreen::ensureVisibilityArea (int         x1,
				   int         y1,
				   int         x2,
				   int         y2,
				   int         margin,
				   ZoomGravity gravity)
{
    int        out  = screen->outputDeviceForPoint (x1 + (x2 - x1 / 2), y1 + (y2 - y1 / 2));
    CompOutput *o   = &screen->outputDevs ().at (out);

    bool widthOkay  = (float)(x2-x1) / (float)o->width ()  < zooms.at (out).newZoom;
    bool heightOkay = (float)(y2-y1) / (float)o->height () < zooms.at (out).newZoom;

    if (widthOkay &&
	heightOkay)
    {
	ensureVisibility (x1, y1, margin);
	ensureVisibility (x2, y2, margin);
	return;
    }

    int targetX, targetY, targetW, targetH;

    switch (gravity)
    {
	case NORTHWEST:
	    targetX = x1;
	    targetY = y1;

	    if (widthOkay)
		targetW = x2 - x1;
	    else
		targetW = o->width () * zooms.at (out).newZoom;

	    if (heightOkay)
		targetH = y2 - y1;
	    else
		targetH = o->height () * zooms.at (out).newZoom;

	    break;

	case NORTHEAST:
	    targetY = y1;

	    if (widthOkay)
	    {
		targetX = x1;
		targetW = x2-x1;
	    }
	    else
	    {
		targetX = x2 - o->width () * zooms.at (out).newZoom;
		targetW = o->width () * zooms.at (out).newZoom;
	    }

	    if (heightOkay)
		targetH = y2-y1;
	    else
		targetH = o->height () * zooms.at (out).newZoom;

	    break;

	case SOUTHWEST:
	    targetX = x1;

	    if (widthOkay)
		targetW = x2-x1;
	    else
		targetW = o->width () * zooms.at (out).newZoom;

	    if (heightOkay)
	    {
		targetY = y1;
		targetH = y2-y1;
	    }
	    else
	    {
		targetY = y2 - (o->width () * zooms.at (out).newZoom);
		targetH = o->width () * zooms.at (out).newZoom;
	    }

	    break;

	case SOUTHEAST:
	    if (widthOkay)
	    {
		targetX = x1;
		targetW = x2-x1;
	    }
	    else
	    {
		targetW = o->width () * zooms.at (out).newZoom;
		targetX = x2 - targetW;
	    }

	    if (heightOkay)
	    {
		targetY = y1;
		targetH = y2 - y1;
	    }
	    else
	    {
		targetH = o->height () * zooms.at (out).newZoom;
		targetY = y2 - targetH;
	    }

	    break;

	case CENTER:
	default:
	    setCenter (x1 + (x2 - x1 / 2), y1 + (y2 - y1 / 2), false);
	    return;

	    break;
    }

    setZoomArea (targetX, targetY, targetW, targetH, false);

    return ;
}

/* Ensures that the cursor is visible on the given head.
 * Note that we check if currentZoom is 1.0f, because that often means that
 * mouseX and mouseY is not up-to-date (since the polling timer just
 * started).
 */
void
EZoomScreen::restrainCursor (int out)
{
    int         x1, y1, x2, y2;
    int         diffX = 0, diffY = 0;
    CompOutput  *o = &screen->outputDevs ().at (out);

    float z      = zooms.at (out).newZoom;
    int   margin = optionGetRestrainMargin ();
    int   north  = distanceToEdge (out, NORTH);
    int   south  = distanceToEdge (out, SOUTH);
    int   east   = distanceToEdge (out, EAST);
    int   west   = distanceToEdge (out, WEST);

    if (zooms.at (out).currentZoom == 1.0f)
    {
	lastChange = time(NULL);
	mouse = MousePoller::getCurrentPosition ();
    }

    convertToZoomedTarget (out, mouse.x () - cursor.hotX,
			   mouse.y () - cursor.hotY, &x1, &y1);
    convertToZoomedTarget
	(out,
	 mouse.x () - cursor.hotX + cursor.width,
	 mouse.y () - cursor.hotY + cursor.height,
	 &x2, &y2);

    if ((x2 - x1 > o->x2 () - o->x1 ()) ||
       (y2 - y1 > o->y2 () - o->y1 ()))
	return;

    if (x2 > o->x2 () - margin && east > 0)
	diffX = x2 - o->x2 () + margin;
    else if (x1 < o->x1 () + margin && west > 0)
	diffX = x1 - o->x1 () - margin;

    if (y2 > o->y2 () - margin && south > 0)
	diffY = y2 - o->y2 () + margin;
    else if (y1 < o->y1 () + margin && north > 0)
	diffY = y1 - o->y1 () - margin;

    if (abs(diffX)*z > 0  || abs(diffY)*z > 0)
	screen->warpPointer ((int) (mouse.x () - pointerX) -
				 (int) ((float)diffX * z),
			     (int) (mouse.y () - pointerY) -
				 (int) ((float)diffY * z));
}

/* Check if the cursor is still visible.
 * We also make sure to activate/deactivate cursor scaling here
 * so we turn on/off the pointer if it moves from one head to another.
 * FIXME: Detect an actual output change instead of spamming.
 * FIXME: The second ensureVisibility (sync with restrain).
 */
void
EZoomScreen::cursorMoved ()
{
    int out = screen->outputDeviceForPoint (mouse.x (), mouse.y ());

    if (isActive (out))
    {
	if (optionGetRestrainMouse ())
	    restrainCursor (out);

	if (optionGetZoomMode () == EzoomOptions::ZoomModePanArea)
	    ensureVisibilityArea (mouse.x () - cursor.hotX,
				  mouse.y () - cursor.hotY,
				  mouse.x () + cursor.width -
				  cursor.hotX,
				  mouse.y () + cursor.height -
				  cursor.hotY,
				  optionGetRestrainMargin (),
				  NORTHWEST);
	cursorZoomActive (out);
    }
    else
	cursorZoomInactive ();
}

/* Update the mouse position.
 * Based on the zoom engine in use, we will have to move the zoom area.
 * This might have to be added to a timer.
 */
void
EZoomScreen::updateMousePosition (const CompPoint &p)
{
    mouse.setX (p.x ());
    mouse.setY (p.y ());

    int out = screen->outputDeviceForPoint (mouse.x (), mouse.y ());
    lastChange = time(NULL);

    if (optionGetZoomMode () == EzoomOptions::ZoomModeSyncMouse &&
	!isInMovement (out))
	setCenter (mouse.x (), mouse.y (), true);

    cursorMoved ();
    cScreen->damageScreen ();
}

/* Timeout handler to poll the mouse. Returns false (and thereby does not
 * get re-added to the queue) when zoom is not active. */
void
EZoomScreen::updateMouseInterval (const CompPoint &p)
{
    updateMousePosition (p);

    if (!grabbed)
    {
	cursorMoved ();

	if (pollHandle.active ())
	    pollHandle.stop ();
    }
}

/* Free a cursor */
void
EZoomScreen::freeCursor (CursorTexture *cursor)
{
    if (!cursor->isSet)
	return;

    cursor->isSet = false;
    glDeleteTextures (1, &cursor->texture);
    cursor->texture = 0;
}

/* Translate into place and draw the scaled cursor.  */
void
EZoomScreen::drawCursor (CompOutput    *output,
			const GLMatrix &transform)
{
    int out = output->id ();

    if (cursor.isSet)
    {
	/*
	 * XXX: expo knows how to handle mouse when zoomed, so we back off
	 * when expo is active.
	 */
	if (screen->grabExist ("expo"))
	{
	    cursorZoomInactive ();
	    return;
	}

	GLMatrix       sTransform = transform;
	float          scaleFactor;
	int            ax, ay;
	GLfloat        textureData[8];
	GLfloat        vertexData[12];
	GLVertexBuffer *streamingBuffer = GLVertexBuffer::streamingBuffer ();
	const GLWindowPaintAttrib attrib = { OPAQUE, BRIGHT, COLOR, 0, 0, 0, 0 };

	sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);
	convertToZoomed (out, mouse.x (), mouse.y (), &ax, &ay);
	sTransform.translate ((float) ax, (float) ay, 0.0f);

	if (optionGetScaleMouseDynamic ())
	    scaleFactor = 1.0f / zooms.at (out).currentZoom;
	else
	    scaleFactor = 1.0f / optionGetScaleMouseStatic ();

	sTransform.scale (scaleFactor, scaleFactor, 1.0f);
	int x = -cursor.hotX;
	int y = -cursor.hotY;

	GLboolean glBlendEnabled = glIsEnabled (GL_BLEND);

	if (!glBlendEnabled)
	    glEnable (GL_BLEND);

	glBindTexture (GL_TEXTURE_2D, cursor.texture);

	streamingBuffer->begin (GL_TRIANGLE_STRIP);
	streamingBuffer->colorDefault ();

	vertexData[0]  = x;
	vertexData[1]  = y;
	vertexData[2]  = 0.0f;
	vertexData[3]  = x;
	vertexData[4]  = y + cursor.height;
	vertexData[5]  = 0.0f;
	vertexData[6]  = x + cursor.width;
	vertexData[7]  = y;
	vertexData[8]  = 0.0f;
	vertexData[9]  = x + cursor.width;
	vertexData[10] = y + cursor.height;
	vertexData[11] = 0.0f;

	streamingBuffer->addVertices (4, vertexData);

	textureData[0] = 0;
	textureData[1] = 0;
	textureData[2] = 0;
	textureData[3] = 1;
	textureData[4] = 1;
	textureData[5] = 0;
	textureData[6] = 1;
	textureData[7] = 1;

	streamingBuffer->addTexCoords (0, 4, textureData);

	streamingBuffer->end ();
	streamingBuffer->render (sTransform, attrib);

	glBindTexture (GL_TEXTURE_2D, 0);
	glDisable (GL_BLEND);
    }
}

/* Create (if necessary) a texture to store the cursor,
 * fetch the cursor with XFixes. Store it. */
void
EZoomScreen::updateCursor (CursorTexture * cursor)
{
    int     i;
    Display *dpy = screen->dpy ();

    if (!cursor->isSet)
    {
	cursor->isSet = true;
	cursor->screen = screen;

	glGenTextures (1, &cursor->texture);
	glBindTexture (GL_TEXTURE_2D, cursor->texture);

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
			 gScreen->textureFilter ());
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			 gScreen->textureFilter ());
    }

    XFixesCursorImage *ci = XFixesGetCursorImage (dpy);
    unsigned char     *pixels;
    unsigned long     pix;

    if (ci)
    {
	cursor->width  = ci->width;
	cursor->height = ci->height;
	cursor->hotX = ci->xhot;
	cursor->hotY = ci->yhot;
	pixels = (unsigned char *) malloc (ci->width * ci->height * 4);

	if (!pixels)
	{
	    XFree (ci);
	    return;
	}

	for (i = 0; i < ci->width * ci->height; ++i)
	{
	    pix                 = ci->pixels[i];
	    pixels[i * 4]       = pix & 0xff;
	    pixels[(i * 4) + 1] = (pix >> 8) & 0xff;
	    pixels[(i * 4) + 2] = (pix >> 16) & 0xff;
	    pixels[(i * 4) + 3] = (pix >> 24) & 0xff;
	}

	XFree (ci);
    }
    else
    {
	/* Fallback R: 255 G: 255 B: 255 A: 255
	 * FIXME: Draw a cairo mouse cursor */

	cursor->width  = 1;
	cursor->height = 1;
	cursor->hotX   = 0;
	cursor->hotY   = 0;
	pixels = (unsigned char *) malloc (cursor->width * cursor->height * 4);

	if (!pixels)
	    return;

	for (i = 0; i < cursor->width * cursor->height; ++i)
	{
	    pix                 = 0x00ffffff;
	    pixels[i * 4]       = pix & 0xff;
	    pixels[(i * 4) + 1] = (pix >> 8) & 0xff;
	    pixels[(i * 4) + 2] = (pix >> 16) & 0xff;
	    pixels[(i * 4) + 3] = (pix >> 24) & 0xff;
	}

	compLogMessage ("ezoom", CompLogLevelWarn, "unable to get system cursor image!");
    }

    glBindTexture (GL_TEXTURE_2D, cursor->texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, cursor->width,
		  cursor->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture (GL_TEXTURE_2D, 0);

    free (pixels);
}

/* We are no longer zooming the cursor, so display it.  */
void
EZoomScreen::cursorZoomInactive ()
{
    if (!fixesSupported)
	return;

    if (cursorInfoSelected)
    {
	cursorInfoSelected = false;
	XFixesSelectCursorInput (screen->dpy (), screen->root (), 0);
    }

    if (cursor.isSet)
	freeCursor (&cursor);

    if (cursorHidden)
    {
	cursorHidden = false;
	XFixesShowCursor (screen->dpy (), screen->root ());
    }
}

/* Cursor zoom is active: We need to hide the original,
 * register for Cursor notifies and display the new one.
 * This can be called multiple times, not just on initial
 * activation.
 */
void
EZoomScreen::cursorZoomActive (int out)
{
    if (!fixesSupported)
	return;

    /* Force cursor hiding and mouse panning if this output is locked
     * and cursor hiding is not enabled and we are syncing the mouse
     */
    if (!optionGetScaleMouse ()					&&
	optionGetZoomMode () == EzoomOptions::ZoomModeSyncMouse &&
	optionGetHideOriginalMouse ()				&&
	!zooms.at (out).locked)
	return;

    if (!cursorInfoSelected)
    {
	cursorInfoSelected = true;
	XFixesSelectCursorInput (screen->dpy (), screen->root (),
				 XFixesDisplayCursorNotifyMask);
	updateCursor (&cursor);
    }

    if (canHideCursor &&
	!cursorHidden &&
	(optionGetHideOriginalMouse () || zooms.at (out).locked))
    {
	cursorHidden = true;
	XFixesHideCursor (screen->dpy (), screen->root ());
    }
}

/* Set the zoom area
 * This is an interface for scripting.
 * int32:x1: left x coordinate
 * int32:y1: top y coordinate
 * int32:x2: right x
 * int32:y2: bottom y
 * x2 and y2 can be omitted to assume x1==x2+1 y1==y2+1
 * boolean:scale: True if we should modify the zoom level, false to just
 *                adjust the movement/translation.
 * boolean:restrain: True to warp the pointer so it's visible.
 */
bool
EZoomScreen::setZoomAreaAction (CompAction         *action,
				CompAction::State  state,
				CompOption::Vector options)
{
    int x1 = CompOption::getIntOptionNamed (options, "x1", -1);
    int y1 = CompOption::getIntOptionNamed (options, "y1", -1);

    if (x1 < 0 || y1 < 0)
	return false;

    int x2 = CompOption::getIntOptionNamed (options, "x2", -1);
    int y2 = CompOption::getIntOptionNamed (options, "y2", -1);

    if (x2 < 0)
	x2 = x1 + 1;

    if (y2 < 0)
	y2 = y1 + 1;

    bool scale    = CompOption::getBoolOptionNamed (options, "scale", false);
    bool restrain = CompOption::getBoolOptionNamed (options, "restrain", false);
    int  out      = screen->outputDeviceForPoint (x1, y1);
    int  width    = x2 - x1;
    int  height   = y2 - y1;

    setZoomArea (x1, y1, width, height, false);
    CompOutput *o = &screen->outputDevs (). at(out);

    if (scale && width && height)
	setScaleBigger (out, width  / static_cast <float> (o->width ()),
			     height / static_cast <float> (o->height ()));

    if (restrain)
	restrainCursor (out);

    toggleFunctions (true);

    return true;
}

/* Ensure visibility of an area defined by x1->x2/y1->y2
 * int:x1: left X coordinate
 * int:x2: right X Coordinate
 * int:y1: top Y coordinate
 * int:y2: bottom Y coordinate
 * bool:scale: zoom out if necessary to ensure visibility
 * bool:restrain: Restrain the mouse cursor
 * int:margin: The margin to use (default: 0)
 * if x2/y2 is omitted, it is ignored.
 */
bool
EZoomScreen::ensureVisibilityAction (CompAction         *action,
				     CompAction::State  state,
				     CompOption::Vector options)
{
    int  x1       = CompOption::getIntOptionNamed (options, "x1", -1);
    int  y1       = CompOption::getIntOptionNamed (options, "y1", -1);

    if (x1 < 0 || y1 < 0)
	return false;

    int  x2       = CompOption::getIntOptionNamed (options, "x2", -1);
    int  y2       = CompOption::getIntOptionNamed (options, "y2", -1);
    int  margin   = CompOption::getBoolOptionNamed (options, "margin", 0);
    bool scale    = CompOption::getBoolOptionNamed (options, "scale", false);
    bool restrain = CompOption::getBoolOptionNamed (options, "restrain", false);

    if (x2 < 0)
	y2 = y1 + 1;

    int out = screen->outputDeviceForPoint (x1, y1);
    ensureVisibility (x1, y1, margin);

    if (x2 >= 0 && y2 >= 0)
	ensureVisibility (x2, y2, margin);

    CompOutput *o = &screen->outputDevs (). at(out);

    int width  = x2 - x1;
    int height = y2 - y1;

    if (scale && width && height)
	setScaleBigger (out, width  / static_cast <float> (o->width ()),
			     height / static_cast <float> (o->height ()));

    if (restrain)
	restrainCursor (out);

    toggleFunctions (true);

    return true;
}

/* Finished here */

bool
EZoomScreen::zoomBoxActivate (CompAction         *action,
			      CompAction::State  state,
			      CompOption::Vector options)
{
    grabIndex = screen->pushGrab (None, "ezoom");
    clickPos.setX (pointerX);
    clickPos.setY (pointerY);
    box.setGeometry (pointerX, pointerY, 0, 0);

    if (state & CompAction::StateInitButton)
	action->setState (action->state () | CompAction::StateTermButton);

    toggleFunctions (true);

    return true;
}

bool
EZoomScreen::zoomBoxDeactivate (CompAction         *action,
				CompAction::State  state,
				CompOption::Vector options)
{
    if (grabIndex)
    {
	screen->removeGrab (grabIndex, NULL);
	grabIndex = 0;

	if (pointerX < clickPos.x ())
	{
	    box.setX (pointerX);
	    box.setWidth (clickPos.x () - pointerX);
	}
	else
	    box.setWidth (pointerX - clickPos.x ());

	if (pointerY < clickPos.y ())
	{
	    box.setY (pointerY);
	    box.setHeight (clickPos.y () - pointerY);
	}
	else
	    box.setHeight (pointerY - clickPos.y ());

	int x      = MIN (box.x1 (), box.x2 ());
	int y      = MIN (box.y1 (), box.y2 ());
	int width  = MAX (box.x1 (), box.x2 ()) - x;
	int height = MAX (box.y1 (), box.y2 ()) - y;

	CompWindow::Geometry outGeometry (x, y, width, height, 0);

	int        out = screen->outputDeviceForGeometry (outGeometry);
	CompOutput *o  = &screen->outputDevs (). at (out);
	setScaleBigger (out, width  / static_cast <float> (o->width ()),
			     height / static_cast <float> (o->height ()));
	setZoomArea (x, y, width, height, false);
    }

    toggleFunctions (true);

    return true;
}

/* Zoom in to the area pointed to by the mouse.
 */
bool
EZoomScreen::zoomIn (CompAction         *action,
		     CompAction::State  state,
		     CompOption::Vector options)
{
    int out = screen->outputDeviceForPoint (pointerX, pointerY);

    if (optionGetZoomMode () == EzoomOptions::ZoomModeSyncMouse &&
	!isInMovement (out))
	setCenter (pointerX, pointerY, true);

    setScale (out, zooms.at (out).newZoom / optionGetZoomFactor ());

    toggleFunctions (true);

    return true;
}

/* Locks down the current zoom area
 */
bool
EZoomScreen::lockZoomAction (CompAction         *action,
			     CompAction::State  state,
			     CompOption::Vector options)
{
    int out = screen->outputDeviceForPoint (pointerX, pointerY);
    zooms.at (out).locked = !zooms.at (out).locked;

    return true;
}

/* Zoom to a specific level.
 * target defines the target zoom level.
 * First set the scale level and mark the display as grabbed internally (to
 * catch the FocusIn event). Either target the focused window or the mouse,
 * depending on settings.
 * FIXME: A bit of a mess...
 */
bool
EZoomScreen::zoomSpecific (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector options,
			   SpecificZoomTarget target)
{
    int   out = screen->outputDeviceForPoint (pointerX, pointerY);
    float zoom_level;

    switch (target)
    {
	case ZoomTargetFirst:
	    zoom_level = optionGetZoomSpec1 ();
	    break;

	case ZoomTargetSecond:
	    zoom_level = optionGetZoomSpec2 ();
	    break;

	case ZoomTargetThird:
	    zoom_level = optionGetZoomSpec3 ();
	    break;

	default:
	    return false;
    }

    if ((zoom_level == zooms.at (out).newZoom) ||
	screen->otherGrabExist (NULL))
	return false;

    setScale (out, zoom_level);

    CompWindow *w = screen->findWindow (screen->activeWindow ());

    if (optionGetSpecTargetFocus () && w)
	areaToWindow (w);
    else
    {
	int x = CompOption::getIntOptionNamed (options, "x", 0);
	int y = CompOption::getIntOptionNamed (options, "y", 0);
	setCenter (x, y, false);
    }

    toggleFunctions (true);

    return true;
}

/* TODO: Add specific zoom boost::bind's */

/* Zooms to fit the active window to the screen without cutting
 * it off and targets it.
 */
bool
EZoomScreen::zoomToWindow (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector options)
{
    Window     xid = CompOption::getIntOptionNamed (options, "window", 0);
    CompWindow *w  = screen->findWindow (xid);

    if (!w)
	return true;

    int        width  = w->width ()  + w->border ().left + w->border ().right;
    int        height = w->height () + w->border ().top  + w->border ().bottom;
    int        out    = screen->outputDeviceForGeometry (w->geometry ());
    CompOutput *o     = &screen->outputDevs ().at (out);

    setScaleBigger (out, width  / static_cast <float> (o->width ()),
			 height / static_cast <float> (o->height ()));
    areaToWindow (w);
    toggleFunctions (true);

    return true;
}

bool
EZoomScreen::zoomPan (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options,
		      float              horizAmount,
		      float              vertAmount)
{
    panZoom (horizAmount, vertAmount);

    return true;
}

/* Centers the mouse based on zoom level and translation.
 */
bool
EZoomScreen::zoomCenterMouse (CompAction         *action,
			      CompAction::State  state,
			      CompOption::Vector options)
{
    int out = screen->outputDeviceForPoint (pointerX, pointerY);

    screen->warpPointer ((int) (screen->outputDevs ().at (out).width () / 2 +
				screen->outputDevs ().at (out).x1 () - pointerX)
			 + ((float) screen->outputDevs ().at (out).width () *
			    -zooms.at (out).xtrans),
			 (int) (screen->outputDevs ().at (out).height () / 2 +
				screen->outputDevs ().at (out).y1 () - pointerY)
			 + ((float) screen->outputDevs ().at (out).height () *
			    zooms.at (out).ytrans));
    return true;
}

/* Resize a window to fit the zoomed area.
 * This could probably do with some moving-stuff too.
 * IE: Move the zoom area afterwards. And ensure
 * the window isn't resized off-screen.
 */
bool
EZoomScreen::zoomFitWindowToZoom (CompAction         *action,
				  CompAction::State  state,
				  CompOption::Vector options)
{
    CompWindow *w = screen->findWindow (CompOption::getIntOptionNamed (options,
								       "window", 0));
    if (!w)
	return true;

    unsigned int   mask = CWWidth | CWHeight;
    XWindowChanges xwc;

    int out = screen->outputDeviceForGeometry (w->geometry ());
    xwc.x   = w->serverX ();
    xwc.y   = w->serverY ();

    xwc.width  = (int) (screen->outputDevs ().at (out).width () *
			zooms.at (out).currentZoom -
			(int) ((w->border ().left + w->border ().right)));
    xwc.height = (int) (screen->outputDevs ().at (out).height () *
			zooms.at (out).currentZoom -
			(int) ((w->border ().top + w->border ().bottom)));

    w->constrainNewWindowSize (xwc.width,
			       xwc.height,
			       &xwc.width,
			       &xwc.height);

    if (xwc.width == w->serverWidth ())
	mask &= ~CWWidth;

    if (xwc.height == w->serverHeight ())
	mask &= ~CWHeight;

    if (w->mapNum () && (mask & (CWWidth | CWHeight)))
	w->sendSyncRequest ();

    w->configureXWindow (mask, &xwc);

    toggleFunctions (true);

    return true;
}

bool
EZoomScreen::initiate (CompAction         *action,
		       CompAction::State  state,
		       CompOption::Vector options)
{
    zoomIn (action, state, options);

    if (state & CompAction::StateInitKey)
	action->setState (action->state () | CompAction::StateTermKey);

    if (state & CompAction::StateInitButton)
	action->setState (action->state () | CompAction::StateTermButton);

    toggleFunctions (true);

    return true;
}

bool
EZoomScreen::zoomOut (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options)
{
    int out = screen->outputDeviceForPoint (pointerX, pointerY);

    setScale (out,
	      zooms.at (out).newZoom *
	      optionGetZoomFactor ());

    toggleFunctions (true);

    return true;
}

bool
EZoomScreen::terminate (CompAction         *action,
			CompAction::State  state,
			CompOption::Vector options)
{
    int out = screen->outputDeviceForPoint (pointerX, pointerY);

    if (grabbed)
    {
	zooms.at (out).newZoom = 1.0f;
	cScreen->damageScreen ();
    }

    toggleFunctions (true);

    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

    return false;
}

/* Focus-track related event handling.
 * The lastMapped is a hack to ensure that newly mapped windows are
 * caught even if the grab that (possibly) triggered them affected
 * the mode. Windows created by a key binding (like creating a terminal
 * on a key binding) tends to trigger FocusIn events with mode other than
 * Normal. This works around this problem.
 * FIXME: Cleanup.
 * TODO: Avoid maximized windows.
 */
void
EZoomScreen::focusTrack (XEvent *event)
{
    static Window lastMapped = 0;

    if (event->type == MapNotify)
    {
	lastMapped = event->xmap.window;
	return;
    }
    else if (event->type != FocusIn)
	return;

    if ((event->xfocus.mode != NotifyNormal) &&
	(lastMapped != event->xfocus.window))
	return;

    lastMapped = 0;
    CompWindow *w = screen->findWindow (event->xfocus.window);

    if (w == NULL						||
	w->id () == screen->activeWindow ()			||
	time(NULL) - lastChange < optionGetFollowFocusDelay ()	||
	!optionGetFollowFocus ())
	return;

    int out = screen->outputDeviceForGeometry (w->geometry ());

    if (!isActive (out) &&
	!optionGetAlwaysFocusFitWindow ())
	return;

    if (optionGetFocusFitWindow ())
    {
	int width  = w->width ()  + w->border ().left + w->border ().right;
	int height = w->height () + w->border ().top  + w->border ().bottom;
	float scale = MAX (width  / static_cast <float> (screen->outputDevs ().at (out).width ()),
			   height / static_cast <float> (screen->outputDevs ().at (out).height ()));

	if (scale > optionGetAutoscaleMin ())
	    setScale (out, scale);
    }

    areaToWindow (w);

    toggleFunctions (true);
}

/* Event handler. Pass focus-related events on and handle XFixes events. */
void
EZoomScreen::handleEvent (XEvent *event)
{
    switch (event->type)
    {
	case MotionNotify:
	    if (grabIndex)
	    {
		if (pointerX < clickPos.x ())
		{
		    box.setX (pointerX);
		    box.setWidth (clickPos.x () - pointerX);
		}
		else
		    box.setWidth (pointerX - clickPos.x ());

		if (pointerY < clickPos.y ())
		{
		    box.setY (pointerY);
		    box.setHeight (clickPos.y () - pointerY);
		}
		else
		    box.setHeight (pointerY - clickPos.y ());

		cScreen->damageScreen ();
	    }

	    break;

	case FocusIn:
	case MapNotify:
	    focusTrack (event);
	    break;

	default:
	    if (event->type == fixesEventBase + XFixesCursorNotify)
	    {
		//XFixesCursorNotifyEvent *cev = (XFixesCursorNotifyEvent *)
		//event;
		if (cursor.isSet)
		    updateCursor (&cursor);
	    }

	    break;
    }

    screen->handleEvent (event);
}

/* TODO: Use this ctor carefully */

EZoomScreen::CursorTexture::CursorTexture () :
    isSet (false),
    screen (0),
    width (0),
    height (0),
    hotX (0),
    hotY (0)
{
}

EZoomScreen::EZoomScreen (CompScreen *screen) :
    PluginClassHandler <EZoomScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    grabbed (0),
    grabIndex (0),
    lastChange (0),
    cursorInfoSelected (false),
    cursorHidden (false)
{
    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);

    int major, minor;

    fixesSupported =
	XFixesQueryExtension(screen->dpy (),
			     &fixesEventBase,
			     &fixesErrorBase);

    XFixesQueryVersion (screen->dpy (), &major, &minor);

    if (major >= 4)
	canHideCursor = true;
    else
	canHideCursor = false;

    unsigned int n = screen->outputDevs ().size ();

    for (unsigned int i = 0; i < n; ++i)
    {
	/* zs->grabbed is a mask ... Thus this limit */
	if (i > sizeof (long int) * 8)
	    break;

	ZoomArea za (i);
	zooms.push_back (za);
    }

    pollHandle.setCallback (boost::bind (
				&EZoomScreen::updateMouseInterval, this, _1));

    optionSetZoomInButtonInitiate (boost::bind (&EZoomScreen::zoomIn, this, _1,
						_2, _3));
    optionSetZoomOutButtonInitiate (boost::bind (&EZoomScreen::zoomOut, this, _1,
						 _2, _3));
    optionSetZoomInKeyInitiate (boost::bind (&EZoomScreen::zoomIn, this, _1,
						_2, _3));
    optionSetZoomOutKeyInitiate (boost::bind (&EZoomScreen::zoomOut, this, _1,
						_2, _3));

    optionSetZoomSpecific1KeyInitiate (boost::bind (&EZoomScreen::zoomSpecific,
						    this, _1, _2, _3,
						    ZoomTargetFirst));
    optionSetZoomSpecific2KeyInitiate (boost::bind (&EZoomScreen::zoomSpecific,
						    this, _1, _2, _3,
						    ZoomTargetSecond));
    optionSetZoomSpecific3KeyInitiate (boost::bind (&EZoomScreen::zoomSpecific,
						    this, _1, _2, _3,
						    ZoomTargetThird));

    optionSetPanLeftKeyInitiate (boost::bind (&EZoomScreen::zoomPan, this, _1,
					      _2, _3, -1, 0));
    optionSetPanRightKeyInitiate (boost::bind (&EZoomScreen::zoomPan, this, _1,
					        _2, _3, 1, 0));
    optionSetPanUpKeyInitiate (boost::bind (&EZoomScreen::zoomPan, this, _1, _2,
					     _3, 0, -1));
    optionSetPanDownKeyInitiate (boost::bind (&EZoomScreen::zoomPan, this, _1,
					       _2, _3, 0, 1));

    optionSetFitToWindowKeyInitiate (boost::bind (&EZoomScreen::zoomToWindow,
						  this, _1, _2, _3));
    optionSetCenterMouseKeyInitiate (boost::bind (&EZoomScreen::zoomCenterMouse,
						  this, _1, _2, _3));
    optionSetFitToZoomKeyInitiate (boost::bind (
					&EZoomScreen::zoomFitWindowToZoom, this,
					_1, _2, _3));

    optionSetLockZoomKeyInitiate (boost::bind (&EZoomScreen::lockZoomAction,
						this, _1, _2, _3));
    optionSetZoomBoxButtonInitiate (boost::bind (&EZoomScreen::zoomBoxActivate,
						 this, _1, _2, _3));
    optionSetZoomBoxButtonTerminate (boost::bind (
					&EZoomScreen::zoomBoxDeactivate, this,
					_1, _2, _3));
    optionSetSetZoomAreaInitiate (boost::bind (
					&EZoomScreen::setZoomAreaAction, this,
					_1, _2, _3));
    optionSetEnsureVisibilityInitiate (boost::bind (
					&EZoomScreen::ensureVisibilityAction, this,
					_1, _2, _3));
}

EZoomScreen::~EZoomScreen ()
{
    if (pollHandle.active ())
	pollHandle.stop ();

    if (zooms.size ())
	zooms.clear ();

    cScreen->damageScreen ();
    cursorZoomInactive ();
}

bool
ZoomPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI)	&&
	CompPlugin::checkPluginABI ("mousepoll", COMPIZ_MOUSEPOLL_ABI))
	return true;

    return false;
}
