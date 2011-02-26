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
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <core/core.h>
#include <decoration.h>
#include "decor.h"

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

COMPIZ_PLUGIN_20090315 (decor, DecorPluginVTable)

/* From core */

bool
isAncestorTo (CompWindow *window,
              CompWindow *candidate)
{
    if (window->transientFor ())
    {
        if (window->transientFor () == candidate->id ())
            return true;

	window = screen->findWindow (window->transientFor ());
	if (window)
	    return isAncestorTo (window, candidate);
    }

    return false;
}

/* Make shadows look nice, don't paint shadows on top of
 * things they don't make sense on top of, eg, menus
 * need shadows but they don't need to be painted when
 * another menu is adjacent and covering the shadow
 * region. Also panel shadows are nice, but not
 * when they obscure client window shadows
 *
 * We need to use the current clip region here
 * and take an intersection of that to ensure
 * that we don't unintentionally expand the clip
 * region that core already reduced by doing
 * occlusion detection
 */
void
DecorWindow::computeShadowRegion ()
{
    shadowRegion = CompRegion (window->outputRect ());

    if (window->type () == CompWindowTypeDockMask)
    {
        /* windows above this one in the stack should
         * clip the shadow */

        CompWindowList::iterator it = std::find (screen->windows ().begin (),
                                                 screen->windows ().end (),
                                                 window);

        for (it--; it != screen->windows ().end (); it--)
        {
            CompRegion inter;

            if (!(*it)->isViewable ())
                continue;

            if ((*it)->type () & CompWindowTypeDesktopMask)
                continue;

	    inter = shadowRegion.intersected ((*it)->inputRect ());

            if (!inter.isEmpty ())
		shadowRegion = shadowRegion.subtracted (inter);

        }
    }
    else if (window->type () == CompWindowTypeDropdownMenuMask ||
             window->type () == CompWindowTypePopupMenuMask)
    {
        /* Other transient menus should clip
         * this menu's shadows, also the panel
         * which is a transient parent should
         * too */

        CompWindowList::iterator it = std::find (screen->windows ().begin (),
                                                 screen->windows ().end (),
                                                 window);

        for (it--; it != screen->windows ().end (); it--)
        {
            CompRegion inter;

            if (!(*it)->isViewable ())
                continue;

            if (!((*it)->type () == CompWindowTypeDropdownMenuMask ||
                  (*it)->type () == CompWindowTypePopupMenuMask ||
                  (*it)->type () == CompWindowTypeDockMask))
		continue;

            /* window needs to be a transient parent */
            if (!isAncestorTo (window, (*it)))
                continue;

	    inter = shadowRegion.intersected ((*it)->inputRect ());

            if (!inter.isEmpty ())
		shadowRegion = shadowRegion.subtracted (inter);
        }

        /* If the region didn't change, then it is safe to
         * say that that this window was probably the first
         * menu in the "chain" of dropdown menus that comes
         * from a menu-bar - in that case there isn't any
         * window that the shadow would necessarily occlude
         * here so clip the shadow to the top of the input
         * rect.
         *
         * FIXME: We need a better way to detect exactly
         * where the menubar is for the dropdown menu,
         * that will look a lot better.
         */
        if (window->type () == CompWindowTypeDropdownMenuMask &&
	    shadowRegion == CompRegion (window->outputRect ()))
        {
            CompRect area (window->outputRect ().x1 (),
                           window->outputRect ().y1 (),
                           window->outputRect ().width (),
                           window->inputRect ().y1 () -
                           window->outputRect ().y1 ());

	    shadowRegion = shadowRegion.subtracted (area);
        }
    }
}

bool
DecorWindow::glDraw (const GLMatrix     &transform,
		     GLFragment::Attrib &attrib,
		     const CompRegion   &region,
		     unsigned int       mask)
{
    bool status;

    status = gWindow->glDraw (transform, attrib, region, mask);

    const CompRegion reg = (mask & PAINT_WINDOW_TRANSFORMED_MASK) ?
			   infiniteRegion : shadowRegion.intersected (region);

    if (wd && !reg.isEmpty () &&
	wd->decor->type == WINDOW_DECORATION_TYPE_PIXMAP)
    {
	CompRect box;
	GLTexture::MatrixList ml (1);
	mask |= PAINT_WINDOW_BLEND_MASK;

	gWindow->geometry ().reset ();

	for (int i = 0; i < wd->nQuad; i++)
	{
	    box.setGeometry (wd->quad[i].box.x1,
			     wd->quad[i].box.y1,
			     wd->quad[i].box.x2 - wd->quad[i].box.x1,
			     wd->quad[i].box.y2 - wd->quad[i].box.y1);

	    if (box.width () > 0 && box.height () > 0)
	    {
		ml[0] = wd->quad[i].matrix;
		gWindow->glAddGeometry (ml, CompRegion (box), reg);
	    }
	}

	if (gWindow->geometry ().vCount)
	    gWindow->glDrawTexture (wd->decor->texture->textures[0],
				    attrib, mask);
    }
    else if (wd && !reg.isEmpty () &&
	     wd->decor->type == WINDOW_DECORATION_TYPE_WINDOW)
    {
	GLTexture::MatrixList ml (1);

	if (gWindow->textures ().empty ())
	    gWindow->bind ();
	if (gWindow->textures ().empty ())
	    return status;

	if (gWindow->textures ().size () == 1)
	{
	    ml[0] = gWindow->matrices ()[0];
	    gWindow->geometry ().reset ();
	    gWindow->glAddGeometry (ml, window->frameRegion (), reg);

	    if (gWindow->geometry ().vCount)
		gWindow->glDrawTexture (gWindow->textures ()[0], attrib, mask);
	}
	else
	{
	    if (updateReg)
		updateWindowRegions ();
	    for (unsigned int i = 0; i < gWindow->textures ().size (); i++)
	    {
		ml[0] = gWindow->matrices ()[i];
		gWindow->geometry ().reset ();
		gWindow->glAddGeometry (ml, regions[i], reg);

		if (gWindow->geometry ().vCount)
		    gWindow->glDrawTexture (gWindow->textures ()[i], attrib,
					    mask);
	    }
	}
    }

    return status;
}

static bool bindFailed;

DecorTexture::DecorTexture (Pixmap pixmap) :
    status (true),
    refCount (1),
    pixmap (pixmap),
    damage (None)
{
    unsigned int width, height, depth, ui;
    Window	 root;
    int		 i;

    if (!XGetGeometry (screen->dpy (), pixmap, &root,
		       &i, &i, &width, &height, &ui, &depth))
    {
        status = false;
	return;
    }

    bindFailed = false;
    textures = GLTexture::bindPixmapToTexture (pixmap, width, height, depth);
    if (textures.size () != 1)
    {
	bindFailed = true;
        status = false;
	return;
    }

    if (!DecorScreen::get (screen)->optionGetMipmap ())
	textures[0]->setMipmap (false);

    damage = XDamageCreate (screen->dpy (), pixmap,
			     XDamageReportRawRectangles);
}

DecorTexture::~DecorTexture ()
{
    if (damage)
	XDamageDestroy (screen->dpy (), damage);
}

DecorTexture *
DecorScreen::getTexture (Pixmap pixmap)
{
    if (!cmActive)
	return NULL;

    foreach (DecorTexture *t, textures)
	if (t->pixmap == pixmap)
	{
	    t->refCount++;
	    return t;
	}

    DecorTexture *texture = new DecorTexture (pixmap);

    if (!texture->status)
    {
	delete texture;
	return NULL;
    }

    textures.push_back (texture);

    return texture;
}


void
DecorScreen::releaseTexture (DecorTexture *texture)
{
    texture->refCount--;
    if (texture->refCount)
	return;

    std::list<DecorTexture *>::iterator it =
	std::find (textures.begin (), textures.end (), texture);

    if (it == textures.end ())
	return;

    textures.erase (it);
    delete texture;
}

static void
computeQuadBox (decor_quad_t *q,
		int	     width,
		int	     height,
		int	     *return_x1,
		int	     *return_y1,
		int	     *return_x2,
		int	     *return_y2,
		float        *return_sx,
		float        *return_sy)
{
    int   x1, y1, x2, y2;
    float sx = 1.0f;
    float sy = 1.0f;

    decor_apply_gravity (q->p1.gravity, q->p1.x, q->p1.y, width, height,
			 &x1, &y1);
    decor_apply_gravity (q->p2.gravity, q->p2.x, q->p2.y, width, height,
			 &x2, &y2);

    if (q->clamp & CLAMP_HORZ)
    {
	if (x1 < 0)
	    x1 = 0;
	if (x2 > width)
	    x2 = width;
    }

    if (q->clamp & CLAMP_VERT)
    {
	if (y1 < 0)
	    y1 = 0;
	if (y2 > height)
	    y2 = height;
    }

    if (q->stretch & STRETCH_X)
    {
	sx = (float) q->max_width / ((float) (x2 - x1));
    }
    else if (q->max_width < x2 - x1)
    {
	if (q->align & ALIGN_RIGHT)
	    x1 = x2 - q->max_width;
	else
	    x2 = x1 + q->max_width;
    }

    if (q->stretch & STRETCH_Y)
    {
	sy = (float) q->max_height / ((float) (y2 - y1));
    }
    else if (q->max_height < y2 - y1)
    {
	if (q->align & ALIGN_BOTTOM)
	    y1 = y2 - q->max_height;
	else
	    y2 = y1 + q->max_height;
    }

    *return_x1 = x1;
    *return_y1 = y1;
    *return_x2 = x2;
    *return_y2 = y2;

    if (return_sx)
	*return_sx = sx;
    if (return_sy)
	*return_sy = sy;
}

Decoration *
Decoration::create (Window id,
		    Atom   decorAtom)
{
    Decoration	    *decoration;
    Atom	    actual;
    int		    result, format;
    unsigned long   n, nleft;
    unsigned char   *data;
    long	    *prop;
    Pixmap	    pixmap = None;
    decor_extents_t border;
    decor_extents_t input;
    decor_extents_t maxBorder;
    decor_extents_t maxInput;
    decor_quad_t    *quad = NULL;
    int		    nQuad = 0;
    int		    minWidth;
    int		    minHeight;
    int		    left, right, top, bottom;
    int		    x1, y1, x2, y2;
    int		    type;

    result = XGetWindowProperty (screen->dpy (), id,
				 decorAtom, 0L, 1024L, false,
				 XA_INTEGER, &actual, &format,
				 &n, &nleft, &data);

    if (result != Success || !n || !data)
	return NULL;

    prop = (long *) data;

    if (decor_property_get_version (prop) != decor_version ())
    {
	compLogMessage ("decoration", CompLogLevelWarn,
			"Property ignored because "
			"version is %d and decoration plugin version is %d\n",
			decor_property_get_version (prop), decor_version ());

	XFree (data);
	return NULL;
    }

    type = decor_property_get_type (prop);

    if (type ==  WINDOW_DECORATION_TYPE_PIXMAP &&
	!DecorScreen::get (screen)->cmActive)
	return NULL;

    if (type == WINDOW_DECORATION_TYPE_PIXMAP)
    {
	nQuad = (n - BASE_PROP_SIZE) / QUAD_PROP_SIZE;

	quad = new decor_quad_t [nQuad];
	if (!quad)
	{
	    XFree (data);
	    return NULL;
	}

	nQuad = decor_pixmap_property_to_quads (prop, n, &pixmap, &input,
						&border, &maxInput,
						&maxBorder, &minWidth, &minHeight, quad);

	XFree (data);

	if (!nQuad)
	{
	    delete [] quad;
	    return NULL;
	}
    }
    else if (type == WINDOW_DECORATION_TYPE_WINDOW)
    {
	if (!decor_window_property (prop, n, &input, &maxInput,
				    &minWidth, &minHeight))
	{
	    XFree (data);
	    return NULL;
	}

	border = input;
	maxBorder = maxInput;
	
	XFree (data);
    }
    else
	return NULL;

    decoration = new Decoration ();
    if (!decoration)
    {
	delete [] quad;
	return NULL;
    }

    if (pixmap)
	decoration->texture = DecorScreen::get (screen)->getTexture (pixmap);
    else
	decoration->texture = NULL;

    if (!decoration->texture && type == WINDOW_DECORATION_TYPE_PIXMAP)
    {
	delete decoration;
	delete [] quad;
	return NULL;
    }

    decoration->minWidth  = minWidth;
    decoration->minHeight = minHeight;
    decoration->quad	  = quad;
    decoration->nQuad	  = nQuad;

    if (type == WINDOW_DECORATION_TYPE_PIXMAP)
    {
	left   = 0;
	right  = minWidth;
	top    = 0;
	bottom = minHeight;

	while (nQuad--)
	{
	    computeQuadBox (quad, minWidth, minHeight, &x1, &y1, &x2, &y2,
			    NULL, NULL);

	    if (x1 < left)
		left = x1;
	    if (y1 < top)
		top = y1;
	    if (x2 > right)
		right = x2;
	    if (y2 > bottom)
		bottom = y2;

	    quad++;
	}

	decoration->output.left   = -left;
	decoration->output.right  = right - minWidth;
	decoration->output.top    = -top;
	decoration->output.bottom = bottom - minHeight;
    }
    else
    {
	decoration->output.left   = MAX (input.left, maxInput.left);
	decoration->output.right  = MAX (input.right, maxInput.right);
	decoration->output.top    = MAX (input.top, maxInput.top);
	decoration->output.bottom = MAX (input.bottom, maxInput.bottom);
    }

    /* Extents of actual frame window */

    decoration->input.left   = input.left;
    decoration->input.right  = input.right;
    decoration->input.top    = input.top;
    decoration->input.bottom = input.bottom;

    /* Border extents */

    decoration->border.left   = border.left;
    decoration->border.right  = border.right;
    decoration->border.top    = border.top;
    decoration->border.bottom = border.bottom;

    /* Extents of actual frame window */

    decoration->maxInput.left   = maxInput.left;
    decoration->maxInput.right  = maxInput.right;
    decoration->maxInput.top    = maxInput.top;
    decoration->maxInput.bottom = maxInput.bottom;

    /* Border extents */

    decoration->maxBorder.left   = maxBorder.left;
    decoration->maxBorder.right  = maxBorder.right;
    decoration->maxBorder.top    = maxBorder.top;
    decoration->maxBorder.bottom = maxBorder.bottom;

    decoration->refCount = 1;
    decoration->type = type;

    return decoration;
}

void
Decoration::release (Decoration *decoration)
{
    decoration->refCount--;
    if (decoration->refCount)
	return;

    if (decoration->texture)
	DecorScreen::get (screen)->releaseTexture (decoration->texture);

    delete [] decoration->quad;
    delete decoration;
}

void
DecorWindow::updateDecoration ()
{
    Decoration *decoration;

    bindFailed = false;
    decoration = Decoration::create (window->id (), dScreen->winDecorAtom);

    if (decor)
	Decoration::release (decor);

    if (bindFailed)
	pixmapFailed = true;
    else
	pixmapFailed = false;

    decor = decoration;
}

WindowDecoration *
WindowDecoration::create (Decoration *d)
{
    WindowDecoration *wd;

    wd = new WindowDecoration ();
    if (!wd)
	return NULL;

    if (d->type == WINDOW_DECORATION_TYPE_PIXMAP)
    {
	wd->quad = new ScaledQuad[d->nQuad];

	if (!wd->quad)
	{
	    delete wd;
	    return NULL;
	}
    }
    else
	wd->quad = NULL;

    d->refCount++;

    wd->decor = d;
    wd->nQuad = d->nQuad;

    return wd;
}

void
WindowDecoration::destroy (WindowDecoration *wd)
{
    Decoration::release (wd->decor);
    delete [] wd->quad;
    delete wd;
}

void
DecorWindow::setDecorationMatrices ()
{
    int		      i;
    float	      x0, y0;
    decor_matrix_t    a;
    GLTexture::Matrix b;

    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	wd->quad[i].matrix = wd->decor->texture->textures[0]->matrix ();

	x0 = wd->decor->quad[i].m.x0;
	y0 = wd->decor->quad[i].m.y0;

	a = wd->decor->quad[i].m;
	b = wd->quad[i].matrix;

	wd->quad[i].matrix.xx = a.xx * b.xx + a.yx * b.xy;
	wd->quad[i].matrix.yx = a.xx * b.yx + a.yx * b.yy;
	wd->quad[i].matrix.xy = a.xy * b.xx + a.yy * b.xy;
	wd->quad[i].matrix.yy = a.xy * b.yx + a.yy * b.yy;
	wd->quad[i].matrix.x0 = x0 * b.xx + y0 * b.xy + b.x0;
	wd->quad[i].matrix.y0 = x0 * b.yx + y0 * b.yy + b.y0;

	wd->quad[i].matrix.xx *= wd->quad[i].sx;
	wd->quad[i].matrix.yx *= wd->quad[i].sx;
	wd->quad[i].matrix.xy *= wd->quad[i].sy;
	wd->quad[i].matrix.yy *= wd->quad[i].sy;

	if (wd->decor->quad[i].align & ALIGN_RIGHT)
	    x0 = wd->quad[i].box.x2 - wd->quad[i].box.x1;
	else
	    x0 = 0.0f;

	if (wd->decor->quad[i].align & ALIGN_BOTTOM)
	    y0 = wd->quad[i].box.y2 - wd->quad[i].box.y1;
	else
	    y0 = 0.0f;

	wd->quad[i].matrix.x0 -=
	    x0 * wd->quad[i].matrix.xx +
	    y0 * wd->quad[i].matrix.xy;

	wd->quad[i].matrix.y0 -=
	    y0 * wd->quad[i].matrix.yy +
	    x0 * wd->quad[i].matrix.yx;

	wd->quad[i].matrix.x0 -=
	    wd->quad[i].box.x1 * wd->quad[i].matrix.xx +
	    wd->quad[i].box.y1 * wd->quad[i].matrix.xy;

	wd->quad[i].matrix.y0 -=
	    wd->quad[i].box.y1 * wd->quad[i].matrix.yy +
	    wd->quad[i].box.x1 * wd->quad[i].matrix.yx;
    }
}

void
DecorWindow::updateDecorationScale ()
{
    int		     x1, y1, x2, y2;
    float            sx, sy;
    int		     i;

    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	int x, y;

	computeQuadBox (&wd->decor->quad[i], window->size ().width (),
			window->size ().height (),
			&x1, &y1, &x2, &y2, &sx, &sy);

	x = window->geometry ().x ();
	y = window->geometry ().y ();

	wd->quad[i].box.x1 = x1 + x;
	wd->quad[i].box.y1 = y1 + y;
	wd->quad[i].box.x2 = x2 + x;
	wd->quad[i].box.y2 = y2 + y;
	wd->quad[i].sx     = sx;
	wd->quad[i].sy     = sy;
    }

    setDecorationMatrices ();
}

bool
DecorWindow::checkSize (Decoration *decor)
{
    return (decor->minWidth <= (int) window->size ().width () &&
	    decor->minHeight <= (int) window->size ().height ());
}

int
DecorWindow::shiftX ()
{
    switch (window->sizeHints ().win_gravity) {
	case WestGravity:
	case NorthWestGravity:
	case SouthWestGravity:
	    return window->input ().left;
	case EastGravity:
	case NorthEastGravity:
	case SouthEastGravity:
	    return -window->input ().right;
    }

    return 0;
}

int
DecorWindow::shiftY ()
{
    switch (window->sizeHints ().win_gravity) {
	case NorthGravity:
	case NorthWestGravity:
	case NorthEastGravity:
	    return window->input ().top;
	case SouthGravity:
	case SouthWestGravity:
	case SouthEastGravity:
	    return -window->input ().bottom;
    }

    return 0;
}

static bool
decorOffsetMove (CompWindow *w, XWindowChanges xwc, unsigned int mask)
{
    CompOption::Vector o (1);

    o.at (0).setName ("window", CompOption::TypeInt);
    o.at (0).value ().set ((int) w->id ());

    w->configureXWindow (mask, &xwc);
    screen->handleCompizEvent ("decor", "window_decorated", o);
    return false;
}

bool
DecorWindow::update (bool allowDecoration)
{
    Decoration	     *old, *decoration = NULL;
    bool	     decorate = false;
    int		     moveDx, moveDy;
    int		     oldShiftX = 0;
    int		     oldShiftY  = 0;

    old = (wd) ? wd->decor : NULL;

    switch (window->type ()) {
	case CompWindowTypeDialogMask:
	case CompWindowTypeModalDialogMask:
	case CompWindowTypeUtilMask:
	case CompWindowTypeMenuMask:
	case CompWindowTypeNormalMask:
	    if (window->mwmDecor () & (MwmDecorAll | MwmDecorTitle))
		decorate = window->frame ()? true : false;
	default:
	    break;
    }

    if (window->overrideRedirect () && !isSwitcher)
	decorate = false;

    if (window->wmType () & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))
	decorate = false;

    if (decorate)
    {
	if (!dScreen->optionGetDecorationMatch ().evaluate (window))
	    decorate = false;
    }

    if (isSwitcher)
	decorate = true;

    if (decorate)
    {
	if (decor && checkSize (decor))
	    decoration = decor;
	else
	{

	    if (dScreen->dmSupports & WINDOW_DECORATION_TYPE_PIXMAP &&
	        dScreen->cmActive &&
		!(dScreen->dmSupports & WINDOW_DECORATION_TYPE_WINDOW &&
		  pixmapFailed))
	    {
		if (window->id () == screen->activeWindow ())
		    decoration = dScreen->decor[DECOR_ACTIVE];
		else
		    decoration = dScreen->decor[DECOR_NORMAL];
	    }
	    else if (dScreen->dmSupports & WINDOW_DECORATION_TYPE_WINDOW)
		decoration = &dScreen->windowDefault;
	}
    }
    else
    {
	if (dScreen->optionGetShadowMatch ().evaluate (window))
	{
	    if (window->region ().numRects () == 1 && !window->alpha ())
		decoration = dScreen->decor[DECOR_BARE];

	    if (decoration)
	    {
		if (!checkSize (decoration))
		    decoration = NULL;
	    }
	}
    }

    if (!dScreen->dmWin || !allowDecoration)
	decoration = NULL;

    if (decoration == old)
	return false;

    if (dScreen->cmActive)
    {
	cWindow->damageOutputExtents ();
	computeShadowRegion ();
    }

    if (old)
    {
	oldShiftX = shiftX ();
	oldShiftY = shiftY ();

	WindowDecoration::destroy (wd);

	wd = NULL;
    }

    if (decoration)
    {
	wd = WindowDecoration::create (decoration);
	if (!wd)
	    return false;

	if ((window->state () & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	    window->setWindowFrameExtents (&wd->decor->maxBorder,
					   &wd->decor->maxInput);
	else if (!window->hasUnmapReference ())
	    window->setWindowFrameExtents (&wd->decor->border,
					   &wd->decor->input);

	moveDx = shiftX () - oldShiftX;
	moveDy = shiftY () - oldShiftY;

	updateFrame ();
	window->updateWindowOutputExtents ();
	if (dScreen->cmActive)
	    cWindow->damageOutputExtents ();
	updateDecorationScale ();
    }
    else
    {
	CompWindowExtents emptyExtents;
	wd = NULL;

	updateFrame ();

	memset (&emptyExtents, 0, sizeof (CompWindowExtents));

	window->setWindowFrameExtents (&emptyExtents, &emptyExtents);

	moveDx = -oldShiftX;
	moveDy = -oldShiftY;
    }

    if (window->placed () && !window->overrideRedirect () &&
	(moveDx || moveDy))
    {
	XWindowChanges xwc;
	unsigned int   mask = CWX | CWY;

	memset (&xwc, 0, sizeof (XWindowChanges));

	xwc.x = window->serverGeometry ().x () + moveDx;
	xwc.y = window->serverGeometry ().y () + moveDy;

	if (window->state () & CompWindowStateFullscreenMask)
	    mask &= ~(CWX | CWY);

	if (window->state () & CompWindowStateMaximizedHorzMask)
	    mask &= ~CWX;

	if (window->state () & CompWindowStateMaximizedVertMask)
	    mask &= ~CWY;

	if (window->saveMask () & CWX)
	    window->saveWc ().x += moveDx;

	if (window->saveMask () & CWY)
	    window->saveWc ().y += moveDy;

	if (mask)
	{
	    /* allowDecoration is only false in the case of
	     * the destructor calling the update function so since it
	     * is not safe to put the function in a timer (since
	     * it will get unref'd on the vtable destruction) we
	     * need to do it immediately
	     *
	     * FIXME: CompTimer should really be PIMPL and allow
	     * refcounting in case we need to keep it alive
	     */
	    if (!allowDecoration)
		decorOffsetMove (window, xwc, mask);
	    else
		moveUpdate.start (boost::bind (decorOffsetMove, window, xwc, mask), 0);
	}
    }

    return true;
}

void
DecorWindow::updateFrame ()
{
    if (!wd || !(window->input ().left || window->input ().right ||
		 window->input ().top || window->input ().bottom) ||
        (wd->decor->type == WINDOW_DECORATION_TYPE_PIXMAP && outputFrame) ||
        (wd->decor->type == WINDOW_DECORATION_TYPE_WINDOW && inputFrame))
    {
	if (inputFrame)
	{
	    XDeleteProperty (screen->dpy (), window->id (),
			     dScreen->inputFrameAtom);
	    XDestroyWindow (screen->dpy (), inputFrame);

	    inputFrame = None;
	    frameRegion = CompRegion ();

	    oldX = 0;
	    oldY = 0;
	    oldWidth  = 0;
	    oldHeight = 0;
	}
	if (outputFrame)
	{
	    XDamageDestroy (screen->dpy (), frameDamage);
	    XDeleteProperty (screen->dpy (), window->id (),
			     dScreen->outputFrameAtom);
	    XDestroyWindow (screen->dpy (), outputFrame);
	    dScreen->frames.erase (outputFrame);

	    outputFrame = None;
	    frameRegion = CompRegion ();

	    oldX = 0;
	    oldY = 0;
	    oldWidth  = 0;
	    oldHeight = 0;
	}
    }
    if (wd && (window->input ().left || window->input ().right ||
	       window->input ().top || window->input ().bottom))
    {
	if (wd->decor->type == WINDOW_DECORATION_TYPE_PIXMAP)
	    updateInputFrame ();
	else if (wd->decor->type == WINDOW_DECORATION_TYPE_WINDOW)
	    updateOutputFrame ();
    }
}

void
DecorWindow::updateInputFrame ()
{
    XRectangle           rects[4];
    int                  x, y, width, height;
    CompWindow::Geometry server = window->serverGeometry ();
    int                  bw = server.border () * 2;
    CompWindowExtents	 input;
    CompWindowExtents    border;
    Window		 parent;

    if (isSwitcher)
	parent = screen->root ();
    else
	parent = window->frame ();

    if ((window->state () & MAXIMIZE_STATE) == MAXIMIZE_STATE)
    {
	border = wd->decor->maxBorder;
	input = wd->decor->maxInput;
    }
    else
    {
	border = wd->decor->border;
	input = wd->decor->input;
    }

    x      = window->input ().left - border.left;
    y      = window->input ().top - border.top;
    width  = server.width () + input.left + input.right + bw;
    height = server.height ()+ input.top  + input.bottom + bw;

    if (isSwitcher)
    {
	x += window->x ();
	y += window->y ();
    }

    if (window->shaded ())
	height = input.top + input.bottom;

    XGrabServer (screen->dpy ());

    if (!inputFrame)
    {
	XSetWindowAttributes attr;

	attr.event_mask	   = StructureNotifyMask;
	attr.override_redirect = true;

	inputFrame = XCreateWindow (screen->dpy (), parent,
				    x, y, width, height, 0, CopyFromParent,
				    InputOnly, CopyFromParent,
				    CWOverrideRedirect | CWEventMask,
				    &attr);

	XGrabButton (screen->dpy (), AnyButton, AnyModifier, inputFrame,
		     true, ButtonPressMask | ButtonReleaseMask |
		     ButtonMotionMask, GrabModeSync, GrabModeSync, None,
		     None);

	XMapWindow (screen->dpy (), inputFrame);

	XChangeProperty (screen->dpy (), window->id (),
			 dScreen->inputFrameAtom, XA_WINDOW, 32,
			 PropModeReplace, (unsigned char *) &inputFrame, 1);

	if (screen->XShape ())
	    XShapeSelectInput (screen->dpy (), inputFrame, ShapeNotifyMask);

	oldX = 0;
	oldY = 0;
	oldWidth  = 0;
	oldHeight = 0;
    }

    if (x != oldX || y != oldY || width != oldWidth || height != oldHeight)
    {
	int    i = 0;
	oldX = x;
	oldY = y;
	oldWidth  = width;
	oldHeight = height;

	XMoveResizeWindow (screen->dpy (), inputFrame, x, y,
			   width, height);
	XLowerWindow (screen->dpy (), inputFrame);


	rects[i].x	= 0;
	rects[i].y	= 0;
	rects[i].width  = width;
	rects[i].height = input.top;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= 0;
	rects[i].y	= input.top;
	rects[i].width  = input.left;
	rects[i].height = height - input.top - input.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= width - input.right;
	rects[i].y	= input.top;
	rects[i].width  = input.right;
	rects[i].height = height - input.top - input.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= 0;
	rects[i].y	= height - input.bottom;
	rects[i].width  = width;
	rects[i].height = input.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	XShapeCombineRectangles (screen->dpy (), inputFrame,
				 ShapeInput, 0, 0, rects, i,
				 ShapeSet, YXBanded);

	frameRegion = CompRegion ();
    }

    XUngrabServer (screen->dpy ());
}

void
DecorWindow::updateOutputFrame ()
{
    XRectangle           rects[4];
    int                  x, y, width, height;
    CompWindow::Geometry server = window->serverGeometry ();
    int                  bw = server.border () * 2;
    CompWindowExtents	 input;

    if ((window->state () & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	input = wd->decor->maxInput;
    else
	input = wd->decor->input;

    x      = window->input ().left - input.left;
    y      = window->input ().top - input.top;
    width  = server.width () + input.left + input.right + bw;
    height = server.height ()+ input.top  + input.bottom + bw;

    if (window->shaded ())
	height = input.top + input.bottom;

    XGrabServer (screen->dpy ());

    if (!outputFrame)
    {
	XSetWindowAttributes attr;

	attr.background_pixel  = 0x0;
	attr.event_mask        = StructureNotifyMask;
	attr.override_redirect = true;

	outputFrame = XCreateWindow (screen->dpy (), window->frame (),
				     x, y, width, height, 0, CopyFromParent,
				     InputOutput, CopyFromParent,
				     CWOverrideRedirect | CWEventMask,
				     &attr);

	XGrabButton (screen->dpy (), AnyButton, AnyModifier, outputFrame,
			true, ButtonPressMask | ButtonReleaseMask |
			ButtonMotionMask, GrabModeSync, GrabModeSync, None,
			None);

	XMapWindow (screen->dpy (), outputFrame);

	XChangeProperty (screen->dpy (), window->id (),
			 dScreen->outputFrameAtom, XA_WINDOW, 32,
			 PropModeReplace, (unsigned char *) &outputFrame, 1);

	if (screen->XShape ())
	    XShapeSelectInput (screen->dpy (), outputFrame,
			       ShapeNotifyMask);

	oldX = 0;
	oldY = 0;
	oldWidth  = 0;
	oldHeight = 0;

	frameDamage = XDamageCreate (screen->dpy (), outputFrame,
			             XDamageReportRawRectangles);

	dScreen->frames[outputFrame] = this;
    }

    if (x != oldX || y != oldY || width != oldWidth || height != oldHeight)
    {
	int    i = 0;
	oldX = x;
	oldY = y;
	oldWidth  = width;
	oldHeight = height;

	XMoveResizeWindow (screen->dpy (), outputFrame, x, y, width, height);
	XLowerWindow (screen->dpy (), outputFrame);


	rects[i].x	= 0;
	rects[i].y	= 0;
	rects[i].width  = width;
	rects[i].height = input.top;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= 0;
	rects[i].y	= input.top;
	rects[i].width  = input.left;
	rects[i].height = height - input.top - input.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= width - input.right;
	rects[i].y	= input.top;
	rects[i].width  = input.right;
	rects[i].height = height - input.top - input.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= 0;
	rects[i].y	= height - input.bottom;
	rects[i].width  = width;
	rects[i].height = input.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	XShapeCombineRectangles (screen->dpy (), outputFrame,
				 ShapeBounding, 0, 0, rects, i,
				 ShapeSet, YXBanded);

	frameRegion = CompRegion ();
    }

    XUngrabServer (screen->dpy ());
}

void
DecorScreen::checkForDm (bool updateWindows)
{
    Atom	  actual;
    int		  result, format, dmSupports = 0;
    unsigned long n, left;
    unsigned char *data;
    Window	  dmWin = None;

    result = XGetWindowProperty (screen->dpy (), screen->root (),
				 supportingDmCheckAtom, 0L, 1L, false,
				 XA_WINDOW, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	XWindowAttributes attr;

	memcpy (&dmWin, data, sizeof (Window));
	XFree (data);

	CompScreen::checkForError (screen->dpy ());

	XGetWindowAttributes (screen->dpy (), dmWin, &attr);

	if (CompScreen::checkForError (screen->dpy ()))
	    dmWin = None;
	else
	{
	    result = XGetWindowProperty (screen->dpy (), dmWin,
					 decorTypeAtom, 0L, 2L, false,
					 XA_ATOM, &actual, &format,
					 &n, &left, &data);
	    if (result == Success && n && data)
	    {
		Atom *ret = (Atom *) data;

		for (unsigned long i = 0; i < n; i++)
		{
		    if (ret[i] == decorTypePixmapAtom)
			dmSupports |= WINDOW_DECORATION_TYPE_PIXMAP;
		    else if (ret[i] == decorTypeWindowAtom)
			dmSupports |= WINDOW_DECORATION_TYPE_WINDOW;
		}

		if (!dmSupports)
		    dmWin = None;

		XFree (data);
	    }
	    else
		dmWin = None;
	}
    }

    if (dmWin != this->dmWin)
    {
	int i;

	this->dmSupports = dmSupports;

	if (dmWin)
	{
	    for (i = 0; i < DECOR_NUM; i++)
		decor[i] = Decoration::create (screen->root (), decorAtom[i]);
	}
	else
	{
	    for (i = 0; i < DECOR_NUM; i++)
	    {
		if (decor[i])
		{
		    Decoration::release (decor[i]);
		    decor[i] = 0;
		}
	    }

	    foreach (CompWindow *w, screen->windows ())
	    {
		DecorWindow *dw = DecorWindow::get (w);

		if (dw->decor)
		{
		    Decoration::release (dw->decor);
		    dw->decor = 0;
		}
	    }
	}

	this->dmWin = dmWin;

	if (updateWindows)
	{
	    foreach (CompWindow *w, screen->windows ())
		if (w->shaded () || w->isViewable ())
		    DecorWindow::get (w)->update (true);
	}
    }
}

void
DecorWindow::updateFrameRegion (CompRegion &region)
{
    window->updateFrameRegion (region);
    if (wd)
    {
	if (!frameRegion.isEmpty ())
	{
	    int x, y;

	    x = window->geometry (). x ();
	    y = window->geometry (). y ();

	    region += frameRegion.translated (x - wd->decor->input.left,
					      y - wd->decor->input.top);
	}
	else
	{
	    region += infiniteRegion;
	}
    }
    updateReg = true;
}

void
DecorWindow::updateWindowRegions ()
{
    CompRect input (window->inputRect ());

    if (regions.size () != gWindow->textures ().size ())
	regions.resize (gWindow->textures ().size ());

    for (unsigned int i = 0; i < gWindow->textures ().size (); i++)
    {
	regions[i] = CompRegion (*gWindow->textures ()[i]);
	regions[i].translate (input.x (), input.y ());
	regions[i] &= window->frameRegion ();
    }
    updateReg = false;
}

void
DecorWindow::windowNotify (CompWindowNotify n)
{
    DECOR_SCREEN (screen);

    switch (n)
    {
	case CompWindowNotifyMap:
	case CompWindowNotifyUnmap:
	    if (dScreen->cmActive)
	    {
		foreach (CompWindow *cw, DecorScreen::get (screen)->cScreen->getWindowPaintList ())
		{
		    DecorWindow::get (cw)->computeShadowRegion ();
		}
	    }
	    break;
	case CompWindowNotifyUnreparent:
	    /* We don't get a DestroyNotify when the wrapper window
	     * or frame window gets destroyed, which destroys our
	     * window too, so we need to manually
	     * remove our input frame window and property here
	     * FIXME: the decoration manager might not like this -
	     * maybe add a CompWindowNotifyBeforeUnreparent so that we
	     * can tell it that the window is about to go away
	     */
	    if (inputFrame)
	    {
		inputFrame = None;
		XDeleteProperty (screen->dpy (), window->id (),
				 ds->inputFrameAtom);
	    }
	    else if (outputFrame)
	    {
		outputFrame = None;
		XDeleteProperty (screen->dpy (), window->id (),
				 ds->outputFrameAtom);
	    }
	    
	    break;
	    
	case CompWindowNotifyReparent:
	    update (true);
	    break;
	case CompWindowNotifyShade:
	    /* We get the notification for shade before the window is
	     * actually resized which means that calling update ->
	     * damageOutputExtents here will not do anything useful for us
	     * so we need to track when windows are (un)shading and then wait
	     * for the following resize notification to actually
	     * update their decoration (since at this point they would have
	     * been resized)
	     */
	    shading = true;
	    unshading = false;
	    break;
	case CompWindowNotifyUnshade:
	    unshading = true;
	    shading = false;
	    break;
	default:
	    break;
    }

    window->windowNotify (n);
}

void
DecorWindow::updateSwitcher ()
{
    Atom	  actualType;
    int	      	  actualFmt;
    unsigned long nitems, nleft;
    unsigned long *data;

    DECOR_SCREEN (screen);

    if (XGetWindowProperty (screen->dpy (), window->id (),
		    	    ds->decorSwitchWindowAtom, 0L, 1024L,
		    	    false, XA_WINDOW, &actualType, &actualFmt,
		    	    &nitems, &nleft, (unsigned char **) &data) == Success)
    {
	if (nitems == 1)
	{
	    isSwitcher = true;
	    return;
	}
    }

    isSwitcher = false;
}

void
DecorScreen::handleEvent (XEvent *event)
{
    Window  activeWindow = screen->activeWindow ();
    CompWindow *w;

    switch (event->type) {
	case DestroyNotify:
	    w = screen->findWindow (event->xdestroywindow.window);
	    if (w)
	    {
		if (w->id () == dmWin)
		    checkForDm (true);
	    }
	    break;
	case ClientMessage:
	    if (event->xclient.message_type == requestFrameExtentsAtom)
	    {
		w = screen->findWindow (event->xclient.window);
		if (w)
		    DecorWindow::get (w)->update (true);
	    }
	default:
	    if (cmActive &&
		event->type == cScreen->damageEvent () + XDamageNotify)
	    {
		XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;

		if (frames.find (de->drawable) != frames.end ())
		    frames[de->drawable]->cWindow->damageOutputExtents ();

		foreach (DecorTexture *t, textures)
		{
		    if (t->pixmap == de->drawable)
		    {
			foreach (CompWindow *w, screen->windows ())
			{
			    if (w->shaded () || w->mapNum ())
			    {
				DECOR_WINDOW (w);

				if (dw->wd && dw->wd->decor->texture == t)
				    dw->cWindow->damageOutputExtents ();
			    }
			}
			return;
		    }
		}
	    }
	    break;
    }

    screen->handleEvent (event);

    if (screen->activeWindow () != activeWindow)
    {
	w = screen->findWindow (activeWindow);
	if (w && !w->hasUnmapReference ())
	    DecorWindow::get (w)->update (true);

	w = screen->findWindow (screen->activeWindow ());
	if (w)
	    DecorWindow::get (w)->update (true);
    }

    switch (event->type) {
	case PropertyNotify:
	    if (event->xproperty.atom == decorSwitchWindowAtom)
	    {
		CompWindow    *w = screen->findWindow (event->xproperty.window);

		DECOR_WINDOW (w);

		if (dw->isSwitcher && !event->xproperty.state == PropertyDelete)
		    dw->updateSwitcher ();
	    }
	    else if (event->xproperty.atom == winDecorAtom)
	    {
		w = screen->findWindow (event->xproperty.window);
		if (w)
		{
		    DECOR_WINDOW (w);
		    dw->updateDecoration ();
		    dw->update (true);
		}
	    }
	    else if (event->xproperty.atom == Atoms::mwmHints)
	    {
		w = screen->findWindow (event->xproperty.window);
		if (w)
		    DecorWindow::get (w)->update (true);
	    }
	    else if (event->xproperty.atom == XA_WM_TRANSIENT_FOR)
	    {
		if (cmActive)
		{
		    foreach (CompWindow *cw, cScreen->getWindowPaintList ())
		    {
			DecorWindow::get (cw)->computeShadowRegion ();
		    }
		}
	    }
	    else
	    {
		if (event->xproperty.window == screen->root ())
		{
		    if (event->xproperty.atom == supportingDmCheckAtom)
		    {
			checkForDm (true);
		    }
		    else
		    {
			int i;

			for (i = 0; i < DECOR_NUM; i++)
			{
			    if (event->xproperty.atom == decorAtom[i])
			    {
				if (decor[i])
				    Decoration::release (decor[i]);

				decor[i] =
				    Decoration::create (screen->root (),
							decorAtom[i]);

				foreach (CompWindow *w, screen->windows ())
				    DecorWindow::get (w)->update (true);
			    }
			}
		    }
		}
	    }
	    break;
	case ConfigureNotify:
	    w = screen->findTopLevelWindow (event->xconfigure.window);
	    if (w)
	    {
		DECOR_WINDOW (w);
		if (dw->decor)
		{
		    dw->updateFrame ();
		}
	    }
	    break;
	case DestroyNotify:
	    /* Only for when the client window gets destroyed */
	    w = screen->findTopLevelWindow (event->xproperty.window);
	    if (w)
	    {
		DECOR_WINDOW (w);
		if (dw->inputFrame &&
		    dw->inputFrame == event->xdestroywindow.window)
		{
		    XDeleteProperty (screen->dpy (), w->id (),
				     inputFrameAtom);
		    dw->inputFrame = None;
		}
		else if (dw->outputFrame &&
		         dw->outputFrame == event->xdestroywindow.window)
		{
		    XDeleteProperty (screen->dpy (), w->id (),
				     outputFrameAtom);
		    dw->outputFrame = None;
		}
	    }
	    break;
	default:
	    if (screen->XShape () && event->type ==
		screen->shapeEvent () + ShapeNotify)
	    {
		w = screen->findWindow (((XShapeEvent *) event)->window);
		if (w)
		    DecorWindow::get (w)->update (true);
		else
		{
		    foreach (w, screen->windows ())
		    {
			DECOR_WINDOW (w);
			if (dw->inputFrame ==
			    ((XShapeEvent *) event)->window)
			{
			    XRectangle *shapeRects = 0;
			    int order, n;

			    dw->frameRegion = CompRegion ();

			    shapeRects =
				XShapeGetRectangles (screen->dpy (),
				    dw->inputFrame, ShapeInput,
				    &n, &order);
			    if (!shapeRects || !n)
				break;

			    for (int i = 0; i < n; i++)
				dw->frameRegion +=
				    CompRegion (shapeRects[i].x,
					        shapeRects[i].y,
						shapeRects[i].width,
						shapeRects[i].height);

			    w->updateFrameRegion ();

			    XFree (shapeRects);
			}
			else if (dw->outputFrame ==
			         ((XShapeEvent *) event)->window)
			{
			    XRectangle *shapeRects = 0;
			    int order, n;

			    dw->frameRegion = CompRegion ();

			    shapeRects =
				XShapeGetRectangles (screen->dpy (),
				    dw->outputFrame, ShapeBounding,
				    &n, &order);
			    if (!n || !shapeRects)
				break;

			    for (int i = 0; i < n; i++)
				dw->frameRegion +=
				    CompRegion (shapeRects[i].x,
					        shapeRects[i].y,
						shapeRects[i].width,
						shapeRects[i].height);

			    w->updateFrameRegion ();

			    XFree (shapeRects);
			}
		    }
		}
	    }
	    break;
    }
}

bool
DecorWindow::damageRect (bool initial, const CompRect &rect)
{
    if (initial)
	update (true);

    return cWindow->damageRect (initial, rect);
}

void
DecorWindow::getOutputExtents (CompWindowExtents& output)
{
    window->getOutputExtents (output);

    if (wd)
    {
	CompWindowExtents *e = &wd->decor->output;

	if (e->left > output.left)
	    output.left = e->left;
	if (e->right > output.right)
	    output.right = e->right;
	if (e->top > output.top)
	    output.top = e->top;
	if (e->bottom > output.bottom)
	    output.bottom = e->bottom;
    }
}

void
DecorScreen::updateDefaultShadowProperty ()
{
    long data[4];
    CompOption *colorOption = CompOption::findOption (getOptions (), "shadow_color");
    char *colorString;
    XTextProperty xtp;

    if (!colorOption)
	return;

    colorString = strdup (CompOption::colorToString (colorOption->value ().c ()).c_str ());

    /* 1) Shadow Radius
     * 2) Shadow Opacity
     * 3) Shadow Offset X
     * 4) Shadow Offset Y
     */

    /* the precision is 0.0001, so multiply by 1000 */
    data[0] = optionGetShadowRadius () * 1000;
    data[1] = optionGetShadowOpacity () * 1000;
    data[2] = optionGetShadowXOffset ();
    data[3] = optionGetShadowYOffset ();

    XChangeProperty (screen->dpy (), screen->root (),
		      shadowInfoAtom, XA_INTEGER, 32,
		      PropModeReplace, (unsigned char *) data, 4);

    if (XStringListToTextProperty (&colorString, 1, &xtp))
    {
	XSetTextProperty (screen->dpy (), screen->root (), &xtp, shadowColorAtom);
	XFree (xtp.value);
    }

    free (colorString);


}

bool
DecorScreen::setOption (const CompString  &name,
			CompOption::Value &value)
{
    unsigned int index;

    bool rv = DecorOptions::setOption (name, value);

    if (!rv || !CompOption::findOption (getOptions (), name, &index))
	return false;

    switch (index) {
	case DecorOptions::Command:
	    if (!dmWin)
		screen->runCommand (optionGetCommand ());
	    break;
	case DecorOptions::ShadowMatch:
	    {
		CompString matchString;

		/*
		Make sure RGBA matching is always present and disable shadows
		for RGBA windows by default if the user didn't specify an
		RGBA match.
		Reasoning for that is that shadows are desired for some RGBA
		windows (e.g. rectangular windows that just happen to have an
		RGBA colormap), while it's absolutely undesired for others
		(especially shaped ones) ... by enforcing no shadows for RGBA
		windows by default, we are flexible to user desires while still
		making sure we don't show ugliness by default
		*/

		matchString = optionGetShadowMatch ().toString ();
		if (matchString.find ("rgba=") == CompString::npos)
		{
		    CompMatch rgbaMatch ("rgba=0");
		    optionGetShadowMatch () &= rgbaMatch;
		}
	    }
	    /* fall-through intended */
	case DecorOptions::DecorationMatch:
	    foreach (CompWindow *w, screen->windows ())
		DecorWindow::get (w)->update (true);
	    break;
	case DecorOptions::ShadowRadius:
	case DecorOptions::ShadowOpacity:
	case DecorOptions::ShadowColor:
	case DecorOptions::ShadowXOffset:
	case DecorOptions::ShadowYOffset:
	    updateDefaultShadowProperty ();
	    break;
	default:
	    break;
    }

    return rv;
}

void
DecorWindow::moveNotify (int dx, int dy, bool immediate)
{
    if (wd)
    {
	int		 i;

	for (i = 0; i < wd->nQuad; i++)
	{
	    wd->quad[i].box.x1 += dx;
	    wd->quad[i].box.y1 += dy;
	    wd->quad[i].box.x2 += dx;
	    wd->quad[i].box.y2 += dy;
	}

	setDecorationMatrices ();
    }
    updateReg = true;

    if (dScreen->cmActive)
    {
	foreach (CompWindow *cw,
		 DecorScreen::get (screen)->cScreen->getWindowPaintList ())
	{
	    DecorWindow::get (cw)->computeShadowRegion ();
	}
    }

    window->moveNotify (dx, dy, immediate);
}

bool
DecorWindow::resizeTimeout ()
{
    if (shading || unshading)
    {
	shading = false;
	unshading = false;

	updateDecoration ();
    }
    update (true);
    return false;
}

void
DecorWindow::resizeNotify (int dx, int dy, int dwidth, int dheight)
{
    /* FIXME: we should not need a timer for calling decorWindowUpdate,
       and only call updateWindowDecorationScale if decorWindowUpdate
       returns false. Unfortunately, decorWindowUpdate may call
       updateWindowOutputExtents, which may call WindowResizeNotify. As
       we never should call a wrapped function that's currently
       processed, we need the timer for the moment. updateWindowOutputExtents
       should be fixed so that it does not emit a resize notification. */
    resizeUpdate.start (boost::bind (&DecorWindow::resizeTimeout, this), 0);
    updateDecorationScale ();
    updateReg = true;

    if (dScreen->cmActive)
    {
	foreach (CompWindow *cw,
		 DecorScreen::get (screen)->cScreen->getWindowPaintList ())
	{
	    DecorWindow::get (cw)->computeShadowRegion ();
	}
    }

    window->resizeNotify (dx, dy, dwidth, dheight);
}

void
DecorWindow::stateChangeNotify (unsigned int lastState)
{
    if (!update (true))
    {
	if (wd && wd->decor)
	{
	    if ((window->state () & MAXIMIZE_STATE) == MAXIMIZE_STATE)
		window->setWindowFrameExtents (&wd->decor->maxBorder,
					       &wd->decor->maxInput);
	    else
		window->setWindowFrameExtents (&wd->decor->border,
					       &wd->decor->input);

	    updateFrame ();
	}
    }

    window->stateChangeNotify (lastState);
}

void
DecorScreen::matchPropertyChanged (CompWindow *w)
{
    DecorWindow::get (w)->update (true);

    screen->matchPropertyChanged (w);
}

void
DecorScreen::addSupportedAtoms (std::vector<Atom> &atoms)
{
    screen->addSupportedAtoms (atoms);

    atoms.push_back (requestFrameExtentsAtom);
}

bool
DecorScreen::decoratorStartTimeout ()
{
    if (!dmWin)
	screen->runCommand (optionGetCommand ());

    return false;
}

DecorScreen::DecorScreen (CompScreen *s) :
    PluginClassHandler<DecorScreen,CompScreen> (s),
    cScreen (CompositeScreen::get (s)),
    textures (),
    dmWin (None),
    dmSupports (0),
    cmActive (false)
{
    supportingDmCheckAtom =
	XInternAtom (s->dpy (), DECOR_SUPPORTING_DM_CHECK_ATOM_NAME, 0);
    winDecorAtom =
	XInternAtom (s->dpy (), DECOR_WINDOW_ATOM_NAME, 0);
    decorAtom[DECOR_BARE] =
	XInternAtom (s->dpy (), DECOR_BARE_ATOM_NAME, 0);
    decorAtom[DECOR_NORMAL] =
	XInternAtom (s->dpy (), DECOR_NORMAL_ATOM_NAME, 0);
    decorAtom[DECOR_ACTIVE] =
	XInternAtom (s->dpy (), DECOR_ACTIVE_ATOM_NAME, 0);
    inputFrameAtom =
	XInternAtom (s->dpy (), DECOR_INPUT_FRAME_ATOM_NAME, 0);
    outputFrameAtom =
	XInternAtom (s->dpy (), DECOR_OUTPUT_FRAME_ATOM_NAME, 0);
    decorTypeAtom =
	XInternAtom (s->dpy (), DECOR_TYPE_ATOM_NAME, 0);
    decorTypePixmapAtom =
	XInternAtom (s->dpy (), DECOR_TYPE_PIXMAP_ATOM_NAME, 0);
    decorTypeWindowAtom =
	XInternAtom (s->dpy (), DECOR_TYPE_WINDOW_ATOM_NAME, 0);
    decorSwitchWindowAtom =
	XInternAtom (s->dpy (), DECOR_SWITCH_WINDOW_ATOM_NAME, 0);
    requestFrameExtentsAtom =
        XInternAtom (s->dpy (), "_NET_REQUEST_FRAME_EXTENTS", 0);
    shadowColorAtom =
	XInternAtom (s->dpy (), "_COMPIZ_NET_CM_SHADOW_COLOR", 0);
    shadowInfoAtom =
	XInternAtom (s->dpy (), "_COMPIZ_NET_CM_SHADOW_PROPERTIES", 0);

    windowDefault.texture   = NULL;
    windowDefault.minWidth  = 0;
    windowDefault.minHeight = 0;
    windowDefault.quad      = NULL;
    windowDefault.nQuad     = 0;
    windowDefault.type      = WINDOW_DECORATION_TYPE_WINDOW;

    windowDefault.input.left   = 0;
    windowDefault.input.right  = 0;
    windowDefault.input.top    = 1;
    windowDefault.input.bottom = 0;

    windowDefault.border = windowDefault.maxBorder =
	windowDefault.maxInput = windowDefault.output =
	    windowDefault.input;

    windowDefault.refCount = 1;

    cmActive = (cScreen) ? cScreen->compositingActive () &&
               GLScreen::get (s) != NULL : false;

    for (unsigned int i = 0; i < DECOR_NUM; i++)
	decor[i] = NULL;

    checkForDm (false);

    decoratorStart.start (boost::bind (&DecorScreen::decoratorStartTimeout,
				       this),
			  0);

    ScreenInterface::setHandler (s);
    screen->updateSupportedWmHints ();
}

DecorScreen::~DecorScreen ()
{
    for (unsigned int i = 0; i < DECOR_NUM; i++)
	if (decor[i])
	    Decoration::release (decor[i]);

    screen->addSupportedAtomsSetEnabled (this, false);
    screen->updateSupportedWmHints ();
}

DecorWindow::DecorWindow (CompWindow *w) :
    PluginClassHandler<DecorWindow,CompWindow> (w),
    window (w),
    gWindow (GLWindow::get (w)),
    cWindow (CompositeWindow::get (w)),
    dScreen (DecorScreen::get (screen)),
    wd (NULL),
    decor (NULL),
    inputFrame (None),
    outputFrame (None),
    pixmapFailed (false),
    regions (),
    updateReg (true),
    unshading (false),
    shading (false),
    isSwitcher (false)
{
    WindowInterface::setHandler (window);

    /* FIXME :DecorWindow::update can call updateWindowOutputExtents
     * which will call a zero-diff resizeNotify. Since this window
     * might be part of a startup procedure, we can't assume that
     * all other windows in the list are necessarily safe to use
     * (since DecorWindow::DecorWindow might not have been called
     * for them) so we need to turn off resize notifications
     * and turn them back on once we're done updating the decoration
     */

    window->resizeNotifySetEnabled (this, false);

    if (dScreen->cmActive)
    {
	gWindow = GLWindow::get (w);
        cWindow = CompositeWindow::get (w);
	CompositeWindowInterface::setHandler (cWindow);
	GLWindowInterface::setHandler (gWindow);
    }

    updateSwitcher ();

    if (!w->overrideRedirect () || isSwitcher)
	updateDecoration ();

    if (w->shaded () || w->isViewable ())
	update (true);

    window->resizeNotifySetEnabled (this, true);
}


DecorWindow::~DecorWindow ()
{
    if (!window->destroyed ())
	update (false);

    if (wd)
	WindowDecoration::destroy (wd);

    if (decor)
	Decoration::release (decor);
}

bool
DecorPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
	 return false;

    return true;
}

