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

#include "zoom.h"

COMPIZ_PLUGIN_20090315 (zoom, ZoomPluginVTable)

static int
adjustZoomVelocity (ZoomScreen *zs)
{
    float d, adjust, amount;

    d = (1.0f - zs->scale) * 10.0f;

    adjust = d * 0.002f;
    amount = fabs (d);
    if (amount < 1.0f)
	amount = 1.0f;
    else if (amount > 5.0f)
	amount = 5.0f;

    zs->velocity = (amount * zs->velocity + adjust) / (amount + 1.0f);

    return (fabs (d) < 0.02f && fabs (zs->velocity) < 0.005f);
}

void
ZoomScreen::zoomInEvent ()
{
    CompOption::Vector o (0);

    o.push_back (CompOption ("root", CompOption::TypeInt));
    o.push_back (CompOption ("output", CompOption::TypeInt));
    o.push_back (CompOption ("x1", CompOption::TypeInt));
    o.push_back (CompOption ("y1", CompOption::TypeInt));
    o.push_back (CompOption ("x2", CompOption::TypeInt));
    o.push_back (CompOption ("y2", CompOption::TypeInt));

    o[0].value ().set ((int) screen->root ());
    o[1].value ().set ((int) zoomOutput);
    o[2].value ().set ((int) current[zoomOutput].x1);
    o[3].value ().set ((int) current[zoomOutput].y1);
    o[4].value ().set ((int) current[zoomOutput].x2);
    o[5].value ().set ((int) current[zoomOutput].y2);

    screen->handleCompizEvent ("zoom", "in", o);
}

void
ZoomScreen::zoomOutEvent ()
{
    CompOption::Vector o (0);

    o.push_back (CompOption ("root", CompOption::TypeInt));
    o.push_back (CompOption ("output", CompOption::TypeInt));

    o[0].value ().set ((int) screen->root ());
    o[1].value ().set ((int) zoomOutput);

    screen->handleCompizEvent ("zoom", "out", o);
}

void
ZoomScreen::preparePaint (int ms)
{
    if (adjust)
    {
	int   steps;
	float amount;

	amount = ms * 0.35f * optionGetSpeed ();
	steps  = amount / (0.5f * optionGetTimestep ());
	if (!steps) steps = 1;

	while (steps--)
	{
	    if (adjustZoomVelocity (this))
	    {
		BoxPtr pBox =
		    &screen->outputDevs ()[zoomOutput].region ()->extents;

		scale = 1.0f;
		velocity = 0.0f;
		adjust = false;

		if (current[zoomOutput].x1 == pBox->x1 &&
		    current[zoomOutput].y1 == pBox->y1 &&
		    current[zoomOutput].x2 == pBox->x2 &&
		    current[zoomOutput].y2 == pBox->y2)
		{
		    zoomed &= ~(1 << zoomOutput);
		    zoomOutEvent ();
		}
		else
		{
		    zoomInEvent ();
		}

		break;
	    }
	    else
	    {
		scale += (velocity * ms) / (float) cScreen->redrawTime ();
	    }
	}
    }

    cScreen->preparePaint (ms);
}

void
ZoomScreen::donePaint ()
{
    if (adjust)
	cScreen->damageScreen ();

    if (!adjust && zoomed == 0)
    {
	cScreen->preparePaintSetEnabled (this, false);
	cScreen->donePaintSetEnabled (this, false);
	gScreen->glPaintOutputSetEnabled (this, false);
    }

    cScreen->donePaint ();
}

bool
ZoomScreen::glPaintOutput (const GLScreenPaintAttrib &sAttrib,
			   const GLMatrix &transform,
			   const CompRegion &region,
			   CompOutput *output,
			   unsigned int mask)
{
    GLMatrix zTransform (transform);
    bool     status;

    if ((unsigned int) output->id () != (unsigned int) ~0 &&
    	(zoomed & (1 << (unsigned int) output->id ())))
    {
	GLTexture::Filter saveFilter;
	ZoomBox           box;
	float             scale, x, y, x1, y1;
	float             oWidth = output->width ();
	float             oHeight = output->height ();

	mask &= ~PAINT_SCREEN_REGION_MASK;

	getCurrentZoom (output->id (), &box);

	x1 = box.x1 - output->x1 ();
	y1 = box.y1 - output->y1 ();

	scale = oWidth / (box.x2 - box.x1);

	x = ((oWidth  / 2.0f) - x1) / oWidth;
	y = ((oHeight / 2.0f) - y1) / oHeight;

	x = 0.5f - x * scale;
	y = 0.5f - y * scale;

	zTransform.translate (-x, y, 0.0f);
	zTransform.scale (scale, scale, 1.0f);

	mask |= PAINT_SCREEN_TRANSFORMED_MASK;

	saveFilter = gScreen->filter (SCREEN_TRANS_FILTER);

	if (((unsigned int) zoomOutput != (unsigned int) output->id () || !adjust) &&
	     scale > 3.9f &&
	    !optionGetFilterLinear ())
	    gScreen->setFilter (SCREEN_TRANS_FILTER, GLTexture::Fast);

	status = gScreen->glPaintOutput (sAttrib, zTransform, region, output,
					 mask);

	gScreen->setFilter (SCREEN_TRANS_FILTER, saveFilter);
    }
    else
    {
	status = gScreen->glPaintOutput (sAttrib, transform, region, output,
					 mask);
    }

    if (status && grab)
    {
	int x1, x2, y1, y2;

	x1 = MIN (this->x1, this->x2);
	y1 = MIN (this->y1, this->y2);
	x2 = MAX (this->x1, this->x2);
	y2 = MAX (this->y1, this->y2);

	if (grabIndex)
	{
	    zTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	    glPushMatrix ();
	    glLoadMatrixf (zTransform.getMatrix ());
	    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	    glEnable (GL_BLEND);
	    glColor4us (0x2fff, 0x2fff, 0x4fff, 0x4fff);
	    glRecti (x1, y2, x2, y1);
	    glColor4us (0x2fff, 0x2fff, 0x4fff, 0x9fff);
	    glBegin (GL_LINE_LOOP);
	    glVertex2i (x1, y1);
	    glVertex2i (x2, y1);
	    glVertex2i (x2, y2);
	    glVertex2i (x1, y2);
	    glEnd ();
	    glColor4usv (defaultColor);
	    glDisable (GL_BLEND);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	    glPopMatrix ();
	}
    }

    return status;
}


void
ZoomScreen::initiateForSelection (int output)
{
    int tmp;

    if (x1 > x2)
    {
	tmp = x1;
	x1 = x2;
	x2 = tmp;
    }

    if (y1 > y2)
    {
	tmp = y1;
	y1 = y2;
	y2 = tmp;
    }

    if (x1 < x2 && y1 < y2)
    {
	float  oWidth, oHeight;
	float  xScale, yScale, scale;
	BoxRec box;
	int    cx, cy;
	int    width, height;

	oWidth  = screen->outputDevs ()[output].width ();
	oHeight = screen->outputDevs ()[output].height ();

	cx = (int) ((x1 + x2) / 2.0f + 0.5f);
	cy = (int) ((y1 + y2) / 2.0f + 0.5f);

	width  = x2 - x1;
	height = y2 - y1;

	xScale = oWidth  / width;
	yScale = oHeight / height;

	scale = MAX (MIN (xScale, yScale), 1.0f);

	box.x1 = cx - (oWidth  / scale) / 2.0f;
	box.y1 = cy - (oHeight / scale) / 2.0f;
	box.x2 = cx + (oWidth  / scale) / 2.0f;
	box.y2 = cy + (oHeight / scale) / 2.0f;

	if (box.x1 < screen->outputDevs ()[output].x1 ())
	{
	    box.x2 += screen->outputDevs ()[output].x1 () - box.x1;
	    box.x1 = screen->outputDevs ()[output].x1 ();
	}
	else if (box.x2 > screen->outputDevs ()[output].x2 ())
	{
	    box.x1 -= box.x2 - screen->outputDevs ()[output].x2 ();
	    box.x2 = screen->outputDevs ()[output].x2 ();
	}

	if (box.y1 < screen->outputDevs ()[output].y1 ())
	{
	    box.y2 += screen->outputDevs ()[output].y1 () - box.y1;
	    box.y1 = screen->outputDevs ()[output].y1 ();
	}
	else if (box.y2 > screen->outputDevs ()[output].y2 ())
	{
	    box.y1 -= box.y2 - screen->outputDevs ()[output].y2 ();
	    box.y2 = screen->outputDevs ()[output].y2 ();
	}

	if (zoomed & (1 << output))
	{
	    getCurrentZoom (output, &last[output]);
	}
	else
	{
	    last[output].x1 = screen->outputDevs ()[output].x1 ();
	    last[output].y1 = screen->outputDevs ()[output].y1 ();
	    last[output].x2 = screen->outputDevs ()[output].x2 ();
	    last[output].y2 = screen->outputDevs ()[output].y2 ();
	}

	current[output].x1 = box.x1;
	current[output].y1 = box.y1;
	current[output].x2 = box.x2;
	current[output].y2 = box.y2;

	this->scale = 0.0f;
	adjust = true;
	cScreen->preparePaintSetEnabled (this, true);
	cScreen->donePaintSetEnabled (this, true);
	gScreen->glPaintOutputSetEnabled (this, true);
	zoomOutput = output;
	zoomed |= (1 << output);

	cScreen->damageScreen ();
    }
}

static bool
zoomIn (CompAction         *action,
	CompAction::State  state,
	CompOption::Vector &options)
{
    float   w, h, x0, y0;
    int     output;
    ZoomBox box;

    ZOOM_SCREEN (screen);

    output = screen->outputDeviceForPoint (pointerX, pointerY);

    if (!zs->grabIndex)
    {
	zs->grabIndex = screen->pushGrab (None, "zoom");
	screen->handleEventSetEnabled (zs, true);
    }

    if (zs->zoomed & (1 << output))
    {
	zs->getCurrentZoom (output, &box);
    }
    else
    {
	box.x1 = screen->outputDevs ()[output].x1 ();
	box.y1 = screen->outputDevs ()[output].y1 ();
	box.x2 = screen->outputDevs ()[output].x2 ();
	box.y2 = screen->outputDevs ()[output].y2 ();
    }

    w = (box.x2 - box.x1) / zs->optionGetZoomFactor ();
    h = (box.y2 - box.y1) / zs->optionGetZoomFactor ();

    x0 = (pointerX - screen->outputDevs ()[output].x1 ()) / (float)
	screen->outputDevs ()[output].width ();
    y0 = (pointerY - screen->outputDevs ()[output].y1 ()) / (float)
	screen->outputDevs ()[output].height ();

    zs->x1 = box.x1 + (x0 * (box.x2 - box.x1) - x0 * w + 0.5f);
    zs->y1 = box.y1 + (y0 * (box.y2 - box.y1) - y0 * h + 0.5f);
    zs->x2 = zs->x1 + w;
    zs->y2 = zs->y1 + h;

    zs->initiateForSelection (output);

    return true;
}

static bool
zoomInitiate (CompAction         *action,
	      CompAction::State  state,
	      CompOption::Vector &options)
{
    int   output, x1, y1;
    float scale;

    ZOOM_SCREEN (screen);

    if (screen->otherGrabExist ("zoom", NULL))
	return false;

    if (!zs->grabIndex)
    {
	zs->grabIndex = screen->pushGrab (None, "zoom");
	screen->handleEventSetEnabled (zs, true);
    }

    if (state & CompAction::StateInitButton)
	action->setState (action->state () | CompAction::StateTermButton);

    /* start selection zoom rectangle */

    output = screen->outputDeviceForPoint (pointerX, pointerY);

    if (zs->zoomed & (1 << output))
    {
	ZoomBox box;
	float   oWidth;

	zs->getCurrentZoom (output, &box);

	oWidth = screen->outputDevs ()[output].width ();
	scale = oWidth / (box.x2 - box.x1);

	x1 = box.x1;
	y1 = box.y1;
    }
    else
    {
	scale = 1.0f;
	x1 = screen->outputDevs ()[output].x1 ();
	y1 = screen->outputDevs ()[output].y1 ();
    }

    zs->x1 = zs->x2 = x1 +
	((pointerX - screen->outputDevs ()[output].x1 ()) /
	    scale + 0.5f);
    zs->y1 = zs->y2 = y1 +
	((pointerY - screen->outputDevs ()[output].y1 ()) /
	    scale + 0.5f);

    zs->zoomOutput = output;

    zs->grab = true;
    zs->gScreen->glPaintOutputSetEnabled (zs, true);

    zs->cScreen->damageScreen ();

    return true;
}

static bool
zoomOut (CompAction         *action,
	 CompAction::State  state,
	 CompOption::Vector &options)
{
    int output;

    ZOOM_SCREEN (screen);

    output = screen->outputDeviceForPoint (pointerX, pointerY);

    zs->getCurrentZoom (output, &zs->last[output]);

    zs->current[output].x1 = screen->outputDevs ()[output].x1 ();
    zs->current[output].y1 = screen->outputDevs ()[output].y1 ();
    zs->current[output].x2 = screen->outputDevs ()[output].x2 ();
    zs->current[output].y2 = screen->outputDevs ()[output].y2 ();

    zs->zoomOutput = output;
    zs->scale = 0.0f;
    zs->adjust = true;
    zs->grab = false;

    if (zs->grabIndex)
    {
	screen->removeGrab (zs->grabIndex, NULL);
	zs->grabIndex = 0;
	screen->handleEventSetEnabled (zs, false);
    }

    zs->cScreen->damageScreen ();

    return true;
}

static bool
zoomTerminate (CompAction         *action,
	       CompAction::State  state,
	       CompOption::Vector &options)
{
    ZOOM_SCREEN (screen);

    if (zs->grab)
    {
	int output;

	output = screen->outputDeviceForPoint (zs->x1, zs->y1);

	if (zs->x2 > screen->outputDevs ()[output].x2 ())
	    zs->x2 = screen->outputDevs ()[output].x2 ();

	if (zs->y2 > screen->outputDevs ()[output].y2 ())
	    zs->y2 = screen->outputDevs ()[output].y2 ();

	zs->initiateForSelection (output);

	zs->grab = false;
    }
    else
    {
	zoomOut (action, state, noOptions);
    }
    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

    return true;
}

static bool
zoomInitiatePan (CompAction         *action,
		 CompAction::State  state,
		 CompOption::Vector &options)
{
    int output;

    ZOOM_SCREEN (screen);

    output = screen->outputDeviceForPoint (pointerX, pointerY);

    if (!(zs->zoomed & (1 << output)))
	return false;

    if (screen->otherGrabExist ("zoom", NULL))
	return false;

    if (state & CompAction::StateInitButton)
	action->setState (action->state () | CompAction::StateTermButton);

    if (!zs->panGrabIndex)
	zs->panGrabIndex = screen->pushGrab (zs->panCursor, "zoom-pan");

    zs->zoomOutput = output;

    return true;
}

static bool
zoomTerminatePan (CompAction         *action,
		  CompAction::State  state,
		  CompOption::Vector &options)
{
    ZOOM_SCREEN (screen);

    if (zs->panGrabIndex)
    {
	screen->removeGrab (zs->panGrabIndex, NULL);
	zs->panGrabIndex = 0;

	zs->zoomInEvent ();
    }

    return true;
}

void
ZoomScreen::getCurrentZoom (int output, ZoomBox *pBox)
{
    if (output == zoomOutput)
    {
	float inverse;

	inverse = 1.0f - scale;

	pBox->x1 = scale * current[output].x1 +
	    inverse * last[output].x1;
	pBox->y1 = scale * current[output].y1 +
	    inverse * last[output].y1;
	pBox->x2 = scale * current[output].x2 +
	    inverse * last[output].x2;
	pBox->y2 = scale * current[output].y2 +
	    inverse * last[output].y2;
    }
    else
    {
	pBox->x1 = current[output].x1;
	pBox->y1 = current[output].y1;
	pBox->x2 = current[output].x2;
	pBox->y2 = current[output].y2;
    }
}

void
ZoomScreen::handleMotionEvent (int xRoot, int yRoot)
{
    if (grabIndex)
    {
	int     output = zoomOutput;
	ZoomBox box;
	float   scale, oWidth = screen->outputDevs ()[output].width ();

	getCurrentZoom (output, &box);

	if (zoomed & (1 << output))
	    scale = oWidth / (box.x2 - box.x1);
	else
	    scale = 1.0f;

	if (panGrabIndex)
	{
	    float dx, dy;

	    dx = (xRoot - lastPointerX) / scale;
	    dy = (yRoot - lastPointerY) / scale;

	    box.x1 -= dx;
	    box.y1 -= dy;
	    box.x2 -= dx;
	    box.y2 -= dy;

	    if (box.x1 < screen->outputDevs ()[output].x1 ())
	    {
		box.x2 += screen->outputDevs ()[output].x1 () - box.x1;
		box.x1 = screen->outputDevs ()[output].x1 ();
	    }
	    else if (box.x2 > screen->outputDevs ()[output].x2 ())
	    {
		box.x1 -= box.x2 - screen->outputDevs ()[output].x2 ();
		box.x2 = screen->outputDevs ()[output].x2 ();
	    }

	    if (box.y1 < screen->outputDevs ()[output].y1 ())
	    {
		box.y2 += screen->outputDevs ()[output].y1 () - box.y1;
		box.y1 = screen->outputDevs ()[output].y1 ();
	    }
	    else if (box.y2 > screen->outputDevs ()[output].y2 ())
	    {
		box.y1 -= box.y2 - screen->outputDevs ()[output].y2 ();
		box.y2 = screen->outputDevs ()[output].y2 ();
	    }

	    current[output] = box;

	    cScreen->damageScreen ();
	}
	else
	{
	    int x1, y1;

	    if (zoomed & (1 << output))
	    {
		x1 = box.x1;
		y1 = box.y1;
	    }
	    else
	    {
		x1 = screen->outputDevs ()[output].x1 ();
		y1 = screen->outputDevs ()[output].y1 ();
	    }

	    this->x2 = x1 +
		((xRoot - screen->outputDevs ()[output].x1 ()) /
		 scale + 0.5f);
	    this->y2 = y1 +
		((yRoot - screen->outputDevs ()[output].y1 ()) /
		 scale + 0.5f);

	    cScreen->damageScreen ();
	}
    }
}

void
ZoomScreen::handleEvent (XEvent *event)
{
    switch (event->type) {
	case MotionNotify:
	    if (event->xmotion.root == screen->root ())
		handleMotionEvent (pointerX, pointerY);
	    break;
	case EnterNotify:
	case LeaveNotify:
	    if (event->xcrossing.root == screen->root ())
		handleMotionEvent (pointerX, pointerY);
	default:
	    break;
    }

    screen->handleEvent (event);
}

ZoomScreen::ZoomScreen (CompScreen *screen) :
    PluginClassHandler<ZoomScreen,CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    grabIndex (0),
    grab (false),
    zoomed (0),
    adjust (false),
    panGrabIndex (0),
    velocity (0.0),
    scale (0.0),
    zoomOutput (0)
{
    panCursor = XCreateFontCursor (screen->dpy (), XC_fleur);

    memset (&current, 0, sizeof (current));
    memset (&last, 0, sizeof (last));

    optionSetInitiateButtonInitiate (zoomInitiate);
    optionSetInitiateButtonTerminate (zoomTerminate);

    optionSetZoomInButtonInitiate (zoomIn);
    optionSetZoomOutButtonInitiate (zoomOut);

    optionSetZoomPanButtonInitiate (zoomInitiatePan);
    optionSetZoomPanButtonTerminate (zoomTerminatePan);

    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);
}


ZoomScreen::~ZoomScreen ()
{
    if (panCursor)
	XFreeCursor (screen->dpy (), panCursor);
}

bool
ZoomPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION) |
        !CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI) |
        !CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	 return false;

    return true;
}

