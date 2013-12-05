/**
 * Compiz Fusion Freewins plugin
 *
 * input.cpp
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

#include "freewins.h"
#include <cairo/cairo-xlib.h>


/* ------ Input Prevention -------------------------------------------*/

/* Shape the IPW
 * Thanks to Joel Bosveld (b0le)
 * for helping me with this section
 */
void
FWWindow::shapeIPW ()
{
    if (mInput)
    {
	Window xipw = mInput->ipw;
	CompWindow *ipw = screen->findWindow (xipw);

	if (ipw)
	{
	    cairo_t 				*cr;
	    int               width, height;

	    width = mInputRect.width ();
	    height = mInputRect.height ();

	    Pixmap b = XCreatePixmap (screen->dpy (), xipw, width, height, 1);

	    cairo_surface_t *bitmap =
		    cairo_xlib_surface_create_for_bitmap (screen->dpy (),
							  b,
							  DefaultScreenOfDisplay (screen->dpy ()),
							  width, height);

	    cr = cairo_create (bitmap);

	    cairo_save (cr);
	    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	    cairo_paint (cr);
	    cairo_restore (cr);

	    /* Move to our first corner (TopLeft) */

	    cairo_move_to (cr,
			   mOutput.shapex1 - MIN(mInputRect.x1 (), mInputRect.x2 ()),
			   mOutput.shapey1 - MIN(mInputRect.y1 (), mInputRect.y2 ()));

	    /* Line to TopRight */

	    cairo_line_to (cr,
			   mOutput.shapex2 - MIN(mInputRect.x1 (), mInputRect.x2 ()),
			   mOutput.shapey2 - MIN(mInputRect.y1 (), mInputRect.y2 ()));

	    /* Line to BottomRight */

	    cairo_line_to (cr,
			   mOutput.shapex4 - MIN(mInputRect.x1 (), mInputRect.x2 ()),
			   mOutput.shapey4 - MIN(mInputRect.y1 (), mInputRect.y2 ()));

	    /* Line to BottomLeft */

	    cairo_line_to (cr,
			   mOutput.shapex3 - MIN(mInputRect.x1 (), mInputRect.x2 ()),
			   mOutput.shapey3 - MIN(mInputRect.y1 (), mInputRect.y2 ()));

	    /* Line to TopLeft */

	    cairo_line_to (cr,
			   mOutput.shapex1 - MIN(mInputRect.x1 (), mInputRect.x2 ()),
			   mOutput.shapey1 - MIN(mInputRect.y1 (), mInputRect.y2 ()));

	    /* Ensure it's all closed up */

	    cairo_close_path (cr);

	    /* Fill in the box */

	    cairo_set_source_rgb (cr, 1.0f, 1.0f, 1.0f);
	    cairo_fill (cr);

	    /* This takes the bitmap we just drew with cairo
	     * and scans out the white bits (You can see these)
	     * if you uncomment the following line after this
	     * comment. Then, all the bits we drew on are clickable,
	     * leaving us with a nice and neat window shape. Yummy.
	     */

	    /* XWriteBitmapFile (ipw->screen->display->display,
								 "/path/to/your/image.bmp",
								 b,
								 ipw->width,
								 ipw->height,
								 -1, -1);  */

	    XShapeCombineMask (screen->dpy (), xipw,
			       ShapeBounding,
			       0,
			       0,
			       b,
			       ShapeSet);

	    XFreePixmap (screen->dpy (), b);
	    cairo_surface_destroy (bitmap);
	    cairo_destroy (cr);
	}
    }
}

void
FWWindow::saveInputShape (XRectangle **retRects,
			  int       *retCount,
			  int       *retOrdering)
{
    XRectangle *rects;
    int        count = 0, ordering;
    Display    *dpy = screen->dpy ();

    rects = XShapeGetRectangles (dpy, window->id (), ShapeInput, &count, &ordering);

    /* check if the returned shape exactly matches the window shape -
       if that is true, the window currently has no set input shape */
    if ((count == 1) &&
	(rects[0].x == -window->geometry ().border ()) &&
	(rects[0].y == -window->geometry ().border ()) &&
	(rects[0].width == (window->serverWidth () +
			    window->serverGeometry ().border ())) &&
	(rects[0].height == (window->serverHeight () +
			     window->serverGeometry (). border ())))
    {
	count = 0;
    }
    
    *retRects    = rects;
    *retCount    = count;
    *retOrdering = ordering;
}

void
FWScreen::addWindowToList (FWWindowInputInfo *info)
{
    mTransformedWindows.push_back (info);
}

void
FWScreen::removeWindowFromList (FWWindowInputInfo *info)
{
    mTransformedWindows.remove (info);
}

/* Adjust size and location of the input prevention window */
void
FWWindow::adjustIPW ()
{
    XWindowChanges xwc;
    Display        *dpy = screen->dpy ();
    float          f_width, f_height;

    if (!mInput || !mInput->ipw)
	return;

    f_width  = mInputRect.width ();
    f_height = mInputRect.height ();

    xwc.x          = mInputRect.x ();
    xwc.y          = mInputRect.y ();
    xwc.width      = (int) f_width;
    xwc.height     = (int) f_height;
    xwc.stack_mode = Below;
    /* XXX: This causes XConfigureWindow to break */
    //xwc.sibling    = window->id ();

    XMapWindow (dpy, mInput->ipw);

    XConfigureWindow (dpy, mInput->ipw,
		      CWStackMode | CWX | CWY | CWWidth | CWHeight,
		      &xwc);

    shapeIPW ();
}

void
FWScreen::adjustIPWStacking ()
{

    foreach (FWWindowInputInfo *run, mTransformedWindows)
    {
	if (!run->w->prev || run->w->prev->id () != run->ipw)
	    FWWindow::get (run->w)->adjustIPW ();
    }
}

/* Create an input prevention window */
void
FWWindow::createIPW ()
{
    Window               ipw;
    XSetWindowAttributes attrib;
    XWindowChanges       xwc;

    if (!mInput || mInput->ipw)
	return;

    attrib.override_redirect = true;
    //attrib.event_mask        = 0;

    xwc.x = mInputRect.x ();
    xwc.y = mInputRect.y ();
    xwc.width = mInputRect.width ();
    xwc.height = mInputRect.height ();

    ipw = XCreateWindow (screen->dpy (),
			 screen->root (),
			 xwc.x, xwc.y, xwc.width, xwc.height, 0, CopyFromParent, InputOnly,
			 CopyFromParent, CWOverrideRedirect, &attrib);

    XMapWindow (screen->dpy (), ipw);

    //XConfigureWindow (screen->dpy (), ipw, CWStackMode | CWX | CWY | CWWidth | CWHeight, &xwc);

    mInput->ipw = ipw;

    //shapeIPW ();
}

FWWindowInputInfo::FWWindowInputInfo (CompWindow *window) :
    w (window),
    ipw (None),
    inputRects (NULL),
    nInputRects (0),
    inputRectOrdering (0),
    frameInputRects (NULL),
    frameNInputRects (0),
    frameInputRectOrdering (0)
{
}

FWWindowInputInfo::~FWWindowInputInfo ()
{
}

bool
FWWindow::handleWindowInputInfo ()
{
    FREEWINS_SCREEN (screen);

    if (!mTransformed && mInput)
    {
	if (mInput->ipw)
	    XDestroyWindow (screen->dpy (), mInput->ipw);

	unshapeInput ();
	fws->removeWindowFromList (mInput);

	delete mInput;
	mInput = NULL;

	return false;
    }
    else if (mTransformed && !mInput)
    {
	mInput = new FWWindowInputInfo (window);
	if (!mInput)
	    return false;

	shapeInput ();
	createIPW ();
	fws->addWindowToList (mInput);
    }

    return true;
}

/* Shape the input of the window when scaled.
 * Since the IPW will be dealing with the input, removing input
 * from the window entirely is a perfectly good solution. */
void
FWWindow::shapeInput ()
{
    Window     frame;
    Display    *dpy = screen->dpy();

    saveInputShape (&mInput->inputRects,
		    &mInput->nInputRects,
		    &mInput->inputRectOrdering);

    frame = window->frame();
    if (frame)
    {
	saveInputShape (&mInput->frameInputRects, &mInput->frameNInputRects,
			&mInput->frameInputRectOrdering);
    }
    else
    {
	mInput->frameInputRects        = NULL;
	mInput->frameNInputRects       = -1;
	mInput->frameInputRectOrdering = 0;
    }

    /* clear shape */
    XShapeSelectInput (dpy, window->id(), NoEventMask);
    XShapeCombineRectangles  (dpy, window->id(), ShapeInput, 0, 0,
			      NULL, 0, ShapeSet, 0);
    
    if (frame)
	XShapeCombineRectangles  (dpy, window->frame(), ShapeInput, 0, 0,
				  NULL, 0, ShapeSet, 0);

    XShapeSelectInput (dpy, window->id(), ShapeNotify);
}

/* Restores the shape of the window:
 * If the window had a custom shape defined by inputRects then we restore
 * that in order with XShapeCombineRectangles.
 * Most windows have no specific defined shape so we can restore it with
 * setting the shape to a 0x0 mask
 */
void
FWWindow::unshapeInput ()
{
    Display *dpy = screen->dpy ();

    if (mInput->nInputRects)
    {
	XShapeCombineRectangles (dpy, window->id(), ShapeInput, 0, 0,
				 mInput->inputRects, mInput->nInputRects,
				 ShapeSet, mInput->inputRectOrdering);
    }
    else
    {
	XShapeCombineMask (dpy, window->id(), ShapeInput, 0, 0, None, ShapeSet);
    }

    if (mInput->frameNInputRects >= 0)
    {
	if (mInput->frameNInputRects)
	{
	    XShapeCombineRectangles (dpy, window->frame(), ShapeInput, 0, 0,
				     mInput->frameInputRects,
				     mInput->frameNInputRects,
				     ShapeSet,
				     mInput->frameInputRectOrdering);
	}
	else
	{
	    XShapeCombineMask (dpy, window->frame(), ShapeInput, 0, 0, None, ShapeSet);
	}
    }
}
