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
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <compiz.h>
#include <decoration.h>

typedef struct _Vector {
    int	dx;
    int	dy;
    int	x0;
    int	y0;
} Vector;

#define DECOR_BARE   0
#define DECOR_NORMAL 1
#define DECOR_ACTIVE 2
#define DECOR_NUM    3

typedef struct _DecorTexture {
    struct _DecorTexture *next;
    int			 refCount;
    Pixmap		 pixmap;
    Damage		 damage;
    CompTexture		 texture;
} DecorTexture;

typedef struct _Decoration {
    int		      refCount;
    DecorTexture      *texture;
    CompWindowExtents output;
    CompWindowExtents input;
    CompWindowExtents maxInput;
    int		      minWidth;
    int		      minHeight;
    decor_quad_t      *quad;
    int		      nQuad;
} Decoration;

typedef struct _ScaledQuad {
    CompMatrix matrix;
    BoxRec     box;
    float      sx;
    float      sy;
} ScaledQuad;

typedef struct _WindowDecoration {
    Decoration *decor;
    ScaledQuad *quad;
    int	       nQuad;
} WindowDecoration;

#define DECOR_SHADOW_RADIUS_DEFAULT    8.0f
#define DECOR_SHADOW_RADIUS_MIN        0.0f
#define DECOR_SHADOW_RADIUS_MAX       48.0f
#define DECOR_SHADOW_RADIUS_PRECISION  0.1f

#define DECOR_SHADOW_OPACITY_DEFAULT   0.5f
#define DECOR_SHADOW_OPACITY_MIN       0.01f
#define DECOR_SHADOW_OPACITY_MAX       6.0f
#define DECOR_SHADOW_OPACITY_PRECISION 0.01f

#define DECOR_SHADOW_COLOR_RED_DEFAULT   0x0000
#define DECOR_SHADOW_COLOR_GREEN_DEFAULT 0x0000
#define DECOR_SHADOW_COLOR_BLUE_DEFAULT  0x0000

#define DECOR_SHADOW_OFFSET_DEFAULT   1
#define DECOR_SHADOW_OFFSET_MIN     -16
#define DECOR_SHADOW_OFFSET_MAX      16

#define DECOR_MIPMAP_DEFAULT FALSE

#define DECOR_DECOR_MATCH_DEFAULT  "any"
#define DECOR_SHADOW_MATCH_DEFAULT "any"

#define DECOR_DISPLAY_OPTION_SHADOW_RADIUS   0
#define DECOR_DISPLAY_OPTION_SHADOW_OPACITY  1
#define DECOR_DISPLAY_OPTION_SHADOW_COLOR    2
#define DECOR_DISPLAY_OPTION_SHADOW_OFFSET_X 3
#define DECOR_DISPLAY_OPTION_SHADOW_OFFSET_Y 4
#define DECOR_DISPLAY_OPTION_COMMAND         5
#define DECOR_DISPLAY_OPTION_MIPMAP          6
#define DECOR_DISPLAY_OPTION_DECOR_MATCH     7
#define DECOR_DISPLAY_OPTION_SHADOW_MATCH    8
#define DECOR_DISPLAY_OPTION_NUM             9

static int displayPrivateIndex;

typedef struct _DecorDisplay {
    int			     screenPrivateIndex;
    HandleEventProc	     handleEvent;
    MatchPropertyChangedProc matchPropertyChanged;
    DecorTexture	     *textures;
    Atom		     supportingDmCheckAtom;
    Atom		     winDecorAtom;
    Atom		     decorAtom[DECOR_NUM];

    CompOption opt[DECOR_DISPLAY_OPTION_NUM];
} DecorDisplay;

typedef struct _DecorScreen {
    int	windowPrivateIndex;

    Window dmWin;

    Decoration *decor[DECOR_NUM];

    DrawWindowProc		  drawWindow;
    DamageWindowRectProc	  damageWindowRect;
    GetOutputExtentsForWindowProc getOutputExtentsForWindow;

    WindowMoveNotifyProc   windowMoveNotify;
    WindowResizeNotifyProc windowResizeNotify;

    WindowStateChangeNotifyProc windowStateChangeNotify;
} DecorScreen;

typedef struct _DecorWindow {
    WindowDecoration *wd;
    Decoration	     *decor;
} DecorWindow;

#define GET_DECOR_DISPLAY(d)				      \
    ((DecorDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define DECOR_DISPLAY(d)		     \
    DecorDisplay *dd = GET_DECOR_DISPLAY (d)

#define GET_DECOR_SCREEN(s, dd)				          \
    ((DecorScreen *) (s)->privates[(dd)->screenPrivateIndex].ptr)

#define DECOR_SCREEN(s)							   \
    DecorScreen *ds = GET_DECOR_SCREEN (s, GET_DECOR_DISPLAY (s->display))

#define GET_DECOR_WINDOW(w, ds)					  \
    ((DecorWindow *) (w)->privates[(ds)->windowPrivateIndex].ptr)

#define DECOR_WINDOW(w)					       \
    DecorWindow *dw = GET_DECOR_WINDOW  (w,		       \
		      GET_DECOR_SCREEN  (w->screen,	       \
		      GET_DECOR_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static Bool
decorDrawWindow (CompWindow	      *w,
		 const CompTransform  *transform,
		 const FragmentAttrib *attrib,
		 Region		      region,
		 unsigned int	      mask)
{
    Bool status;

    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    UNWRAP (ds, w->screen, drawWindow);
    status = (*w->screen->drawWindow) (w, transform, attrib, region, mask);
    WRAP (ds, w->screen, drawWindow, decorDrawWindow);

    if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	region = &infiniteRegion;

    if (dw->wd && region->numRects)
    {
	WindowDecoration *wd = dw->wd;
	REGION	     box;
	int		     i;

	mask |= PAINT_WINDOW_BLEND_MASK;

	box.rects	 = &box.extents;
	box.numRects = 1;

	w->vCount = w->indexCount = 0;

	for (i = 0; i < wd->nQuad; i++)
	{
	    box.extents = wd->quad[i].box;

	    if (box.extents.x1 < box.extents.x2 &&
		box.extents.y1 < box.extents.y2)
	    {
		(*w->screen->addWindowGeometry) (w,
						 &wd->quad[i].matrix, 1,
						 &box,
						 region);
	    }
	}

	if (w->vCount)
	    (*w->screen->drawWindowTexture) (w,
					     &wd->decor->texture->texture,
					     attrib, mask);
    }

    return status;
}

static DecorTexture *
decorGetTexture (CompScreen *screen,
		 Pixmap	    pixmap)
{
    DecorTexture *texture;
    unsigned int width, height, depth, ui;
    Window	 root;
    int		 i;

    DECOR_DISPLAY (screen->display);

    for (texture = dd->textures; texture; texture = texture->next)
    {
	if (texture->pixmap == pixmap)
	{
	    texture->refCount++;
	    return texture;
	}
    }

    texture = malloc (sizeof (DecorTexture));
    if (!texture)
	return NULL;

    initTexture (screen, &texture->texture);

    if (!XGetGeometry (screen->display->display, pixmap, &root,
		       &i, &i, &width, &height, &ui, &depth))
    {
	finiTexture (screen, &texture->texture);
	free (texture);
	return NULL;
    }

    if (!bindPixmapToTexture (screen, &texture->texture, pixmap,
			      width, height, depth))
    {
	finiTexture (screen, &texture->texture);
	free (texture);
	return NULL;
    }

    if (!dd->opt[DECOR_DISPLAY_OPTION_MIPMAP].value.b)
	texture->texture.mipmap = FALSE;

    texture->damage = XDamageCreate (screen->display->display, pixmap,
				     XDamageReportRawRectangles);

    texture->refCount = 1;
    texture->pixmap   = pixmap;
    texture->next     = dd->textures;

    dd->textures = texture;

    return texture;
}

static void
decorReleaseTexture (CompScreen   *screen,
		     DecorTexture *texture)
{
    DECOR_DISPLAY (screen->display);

    texture->refCount--;
    if (texture->refCount)
	return;

    if (texture == dd->textures)
    {
	dd->textures = texture->next;
    }
    else
    {
	DecorTexture *t;

	for (t = dd->textures; t; t = t->next)
	{
	    if (t->next == texture)
	    {
		t->next = texture->next;
		break;
	    }
	}
    }

    finiTexture (screen, &texture->texture);
    free (texture);
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
	sx = (float)q->max_width / ((float)(x2 - x1));
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
	sy = (float)q->max_height / ((float)(y2 - y1));
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

static Decoration *
decorCreateDecoration (CompScreen *screen,
		       Window	  id,
		       Atom	  decorAtom)
{
    Decoration	    *decoration;
    Atom	    actual;
    int		    result, format;
    unsigned long   n, nleft;
    unsigned char   *data;
    long	    *prop;
    Pixmap	    pixmap;
    decor_extents_t input;
    decor_extents_t maxInput;
    decor_quad_t    *quad;
    int		    nQuad;
    int		    minWidth;
    int		    minHeight;
    int		    left, right, top, bottom;
    int		    x1, y1, x2, y2;

    result = XGetWindowProperty (screen->display->display, id,
				 decorAtom, 0L, 1024L, FALSE,
				 XA_INTEGER, &actual, &format,
				 &n, &nleft, &data);

    if (result != Success || !n || !data)
	return NULL;

    prop = (long *) data;

    if (decor_property_get_version (prop) != decor_version ())
    {
	fprintf (stderr, "%s: decoration: property ignored because "
		 "version is %d and decoration plugin version is %d\n",
		 programName, decor_property_get_version (prop),
		 decor_version ());

	XFree (data);
	return NULL;
    }

    nQuad = (n - BASE_PROP_SIZE) / QUAD_PROP_SIZE;

    quad = malloc (sizeof (decor_quad_t) * nQuad);
    if (!quad)
    {
	XFree (data);
	return NULL;
    }

    nQuad = decor_property_to_quads (prop,
				     n,
				     &pixmap,
				     &input,
				     &maxInput,
				     &minWidth,
				     &minHeight,
				     quad);

    XFree (data);

    if (!nQuad)
    {
	free (quad);
	return NULL;
    }

    decoration = malloc (sizeof (Decoration));
    if (!decoration)
    {
	free (quad);
	return NULL;
    }

    decoration->texture = decorGetTexture (screen, pixmap);
    if (!decoration->texture)
    {
	free (decoration);
	free (quad);
	return NULL;
    }

    decoration->minWidth  = minWidth;
    decoration->minHeight = minHeight;
    decoration->quad	  = quad;
    decoration->nQuad	  = nQuad;

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

    decoration->input.left   = input.left;
    decoration->input.right  = input.right;
    decoration->input.top    = input.top;
    decoration->input.bottom = input.bottom;

    decoration->maxInput.left   = maxInput.left;
    decoration->maxInput.right  = maxInput.right;
    decoration->maxInput.top    = maxInput.top;
    decoration->maxInput.bottom = maxInput.bottom;

    decoration->refCount = 1;

    return decoration;
}

static void
decorReleaseDecoration (CompScreen *screen,
			Decoration *decoration)
{
    decoration->refCount--;
    if (decoration->refCount)
	return;

    decorReleaseTexture (screen, decoration->texture);

    free (decoration->quad);
    free (decoration);
}

static void
decorWindowUpdateDecoration (CompWindow *w)
{
    Decoration *decoration;

    DECOR_DISPLAY (w->screen->display);
    DECOR_WINDOW (w);

    decoration = decorCreateDecoration (w->screen, w->id, dd->winDecorAtom);

    if (dw->decor)
	decorReleaseDecoration (w->screen, dw->decor);

    dw->decor = decoration;
}

static WindowDecoration *
createWindowDecoration (Decoration *d)
{
    WindowDecoration *wd;

    wd = malloc (sizeof (WindowDecoration) +
		 sizeof (ScaledQuad) * d->nQuad);
    if (!wd)
	return NULL;

    d->refCount++;

    wd->decor = d;
    wd->quad  = (ScaledQuad *) (wd + 1);
    wd->nQuad = d->nQuad;

    return wd;
}

static void
destroyWindowDecoration (CompScreen	  *screen,
			 WindowDecoration *wd)
{
    decorReleaseDecoration (screen, wd->decor);
    free (wd);
}

static void
setDecorationMatrices (CompWindow *w)
{
    WindowDecoration *wd;
    int		     i;
    float	     x0, y0;
    decor_matrix_t   a;
    CompMatrix       b;


    DECOR_WINDOW (w);

    wd = dw->wd;
    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	wd->quad[i].matrix = wd->decor->texture->texture.matrix;

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

static void
updateWindowDecorationScale (CompWindow *w)
{
    WindowDecoration *wd;
    int		     x1, y1, x2, y2;
    float            sx, sy;
    int		     i;

    DECOR_WINDOW (w);

    wd = dw->wd;
    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	computeQuadBox (&wd->decor->quad[i], w->width, w->height,
			&x1, &y1, &x2, &y2, &sx, &sy);

	wd->quad[i].box.x1 = x1 + w->attrib.x;
	wd->quad[i].box.y1 = y1 + w->attrib.y;
	wd->quad[i].box.x2 = x2 + w->attrib.x;
	wd->quad[i].box.y2 = y2 + w->attrib.y;
	wd->quad[i].sx     = sx;
	wd->quad[i].sy     = sy;
    }

    setDecorationMatrices (w);
}

static Bool
decorCheckSize (CompWindow *w,
		Decoration *decor)
{
    return (decor->minWidth <= w->width && decor->minHeight <= w->height);
}

static Bool
decorWindowUpdate (CompWindow *w,
		   Bool	      move)
{
    WindowDecoration *wd;
    Decoration	     *old, *decor = NULL;
    Bool	     decorate = FALSE;
    CompMatch	     *match;

    DECOR_DISPLAY (w->screen->display);
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    wd = dw->wd;
    old = (wd) ? wd->decor : NULL;

    switch (w->type) {
    case CompWindowTypeDialogMask:
    case CompWindowTypeModalDialogMask:
    case CompWindowTypeUtilMask:
    case CompWindowTypeNormalMask:
	if (w->mwmDecor & (MwmDecorAll | MwmDecorTitle))
	    decorate = TRUE;
    default:
	break;
    }

    if (w->attrib.override_redirect)
	decorate = FALSE;

    if (decorate)
    {
	match = &dd->opt[DECOR_DISPLAY_OPTION_DECOR_MATCH].value.match;
	if (!matchEval (match, w))
	    decorate = FALSE;
    }

    if (decorate)
    {
	if (dw->decor && decorCheckSize (w, dw->decor))
	{
	    decor = dw->decor;
	}
	else
	{
	    if (w->id == w->screen->display->activeWindow)
		decor = ds->decor[DECOR_ACTIVE];
	    else
		decor = ds->decor[DECOR_NORMAL];
	}
    }
    else
    {
	match = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_MATCH].value.match;
	if (matchEval (match, w))
	{
	    if (w->region->numRects == 1 && !w->alpha)
		decor = ds->decor[DECOR_BARE];

	    /* no decoration on windows with below state */
	    if (w->state & CompWindowStateBelowMask)
		decor = NULL;

	    if (decor)
	    {
		if (!decorCheckSize (w, decor))
		    decor = NULL;
	    }
	}
    }

    if (!ds->dmWin)
	decor = NULL;

    if (decor == old)
	return FALSE;

    damageWindowOutputExtents (w);

    if (old)
	destroyWindowDecoration (w->screen, wd);

    if (decor)
    {
	dw->wd = createWindowDecoration (decor);
	if (!dw->wd)
	    return FALSE;

	if ((w->state & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	    setWindowFrameExtents (w, &decor->maxInput);
	else
	    setWindowFrameExtents (w, &decor->input);

	updateWindowOutputExtents (w);
	damageWindowOutputExtents (w);
	updateWindowDecorationScale (w);
    }
    else
    {
	dw->wd = NULL;
    }

    return TRUE;
}

static void
decorCheckForDmOnScreen (CompScreen *s,
			 Bool	    updateWindows)
{
    CompDisplay   *d = s->display;
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    Window	  dmWin = None;

    DECOR_DISPLAY (s->display);
    DECOR_SCREEN (s);

    result = XGetWindowProperty (d->display, s->root,
				 dd->supportingDmCheckAtom, 0L, 1L, FALSE,
				 XA_WINDOW, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	XWindowAttributes attr;

	memcpy (&dmWin, data, sizeof (Window));
	XFree (data);

	compCheckForError (d->display);

	XGetWindowAttributes (d->display, dmWin, &attr);

	if (compCheckForError (d->display))
	    dmWin = None;
    }

    if (dmWin != ds->dmWin)
    {
	CompWindow *w;
	int	   i;

	if (dmWin)
	{
	    for (i = 0; i < DECOR_NUM; i++)
		ds->decor[i] =
		    decorCreateDecoration (s, s->root, dd->decorAtom[i]);
	}
	else
	{
	    for (i = 0; i < DECOR_NUM; i++)
	    {
		if (ds->decor[i])
		{
		    decorReleaseDecoration (s, ds->decor[i]);
		    ds->decor[i] = 0;
		}
	    }

	    for (w = s->windows; w; w = w->next)
	    {
		DECOR_WINDOW (w);

		if (dw->decor)
		{
		    decorReleaseDecoration (s, dw->decor);
		    dw->decor = 0;
		}
	    }
	}

	ds->dmWin = dmWin;

	if (updateWindows)
	{
	    for (w = s->windows; w; w = w->next)
		decorWindowUpdate (w, TRUE);
	}
    }
}

static void
decorHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    Window     activeWindow = d->activeWindow;
    CompWindow *w;

    DECOR_DISPLAY (d);

    switch (event->type) {
    case DestroyNotify:
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	if (w)
	{
	    DECOR_SCREEN (w->screen);

	    if (w->id == ds->dmWin)
		decorCheckForDmOnScreen (w->screen, TRUE);
	}
    default:
	if (event->type == d->damageEvent + XDamageNotify)
	{
	    XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;
	    DecorTexture       *t;

	    for (t = dd->textures; t; t = t->next)
	    {
		if (t->pixmap == de->drawable)
		{
		    DecorWindow *dw;
		    DecorScreen *ds;
		    CompScreen  *s;

		    t->texture.oldMipmaps = TRUE;

		    for (s = d->screens; s; s = s->next)
		    {
			ds = GET_DECOR_SCREEN (s, dd);

			for (w = s->windows; w; w = w->next)
			{
			    if (w->shaded || w->mapNum)
			    {
				dw = GET_DECOR_WINDOW (w, ds);

				if (dw->wd && dw->wd->decor->texture == t)
				    damageWindowOutputExtents (w);
			    }
			}
		    }
		    return;
		}
	    }
	}
	break;
    }

    UNWRAP (dd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (dd, d, handleEvent, decorHandleEvent);

    if (d->activeWindow != activeWindow)
    {
	w = findWindowAtDisplay (d, activeWindow);
	if (w)
	    decorWindowUpdate (w, FALSE);

	w = findWindowAtDisplay (d, d->activeWindow);
	if (w)
	    decorWindowUpdate (w, FALSE);
    }

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == dd->winDecorAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		decorWindowUpdateDecoration (w);
		decorWindowUpdate (w, TRUE);
	    }
	}
	else if (event->xproperty.atom == d->mwmHintsAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		decorWindowUpdate (w, TRUE);
	}
	else
	{
	    CompScreen *s;

	    s = findScreenAtDisplay (d, event->xproperty.window);
	    if (s)
	    {
		if (event->xproperty.atom == dd->supportingDmCheckAtom)
		{
		    decorCheckForDmOnScreen (s, TRUE);
		}
		else
		{
		    int i;

		    for (i = 0; i < DECOR_NUM; i++)
		    {
			if (event->xproperty.atom == dd->decorAtom[i])
			{
			    DECOR_SCREEN (s);

			    if (ds->decor[i])
				decorReleaseDecoration (s, ds->decor[i]);

			    ds->decor[i] =
				decorCreateDecoration (s, s->root,
						       dd->decorAtom[i]);

			    for (w = s->windows; w; w = w->next)
				decorWindowUpdate (w, TRUE);
			}
		    }
		}
	    }
	}
	break;
    case MapRequest:
	w = findWindowAtDisplay (d, event->xmaprequest.window);
	if (w)
	    decorWindowUpdate (w, TRUE);
	break;
    default:
	if (d->shapeExtension && event->type == d->shapeEvent + ShapeNotify)
	{
	    w = findWindowAtDisplay (d, ((XShapeEvent *) event)->window);
	    if (w)
		decorWindowUpdate (w, TRUE);
	}
	break;
    }
}

static Bool
decorDamageWindowRect (CompWindow *w,
		       Bool	  initial,
		       BoxPtr     rect)
{
    Bool status;

    DECOR_SCREEN (w->screen);

    if (initial)
	decorWindowUpdate (w, FALSE);

    UNWRAP (ds, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ds, w->screen, damageWindowRect, decorDamageWindowRect);

    return status;
}

static void
decorGetOutputExtentsForWindow (CompWindow	  *w,
				CompWindowExtents *output)
{
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    UNWRAP (ds, w->screen, getOutputExtentsForWindow);
    (*w->screen->getOutputExtentsForWindow) (w, output);
    WRAP (ds, w->screen, getOutputExtentsForWindow,
	  decorGetOutputExtentsForWindow);

    if (dw->wd)
    {
	CompWindowExtents *e = &dw->wd->decor->output;

	if (e->left > output->left)
	    output->left = e->left;
	if (e->right > output->right)
	    output->right = e->right;
	if (e->top > output->top)
	    output->top = e->top;
	if (e->bottom > output->bottom)
	    output->bottom = e->bottom;
    }
}

static CompOption *
decorGetDisplayOptions (CompDisplay *display,
			int	    *count)
{
    DECOR_DISPLAY (display);

    *count = NUM_OPTIONS (dd);
    return dd->opt;
}

static Bool
decorSetDisplayOption (CompDisplay     *display,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    DECOR_DISPLAY (display);

    o = compFindOption (dd->opt, NUM_OPTIONS (dd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case DECOR_DISPLAY_OPTION_SHADOW_RADIUS:
    case DECOR_DISPLAY_OPTION_SHADOW_OPACITY:
	if (compSetFloatOption (o, value))
	    return TRUE;
	break;
    case DECOR_DISPLAY_OPTION_SHADOW_COLOR:
	if (compSetColorOption (o, value))
	    return TRUE;
	break;
    case DECOR_DISPLAY_OPTION_SHADOW_OFFSET_X:
    case DECOR_DISPLAY_OPTION_SHADOW_OFFSET_Y:
	if (compSetIntOption (o, value))
	    return TRUE;
	break;
    case DECOR_DISPLAY_OPTION_COMMAND:
	if (compSetStringOption (o, value))
	{
	    if (display->screens && *o->value.s != '\0')
	    {
		DECOR_SCREEN (display->screens);

		/* run decorator command if no decorator is present on
		   first screen */
		if (!ds->dmWin)
		{
		    if (fork () == 0)
		    {
			putenv (display->displayString);
			execl ("/bin/sh", "/bin/sh", "-c", o->value.s, NULL);
			exit (0);
		    }
		}
	    }

	    return TRUE;
	}
	break;
    case DECOR_DISPLAY_OPTION_MIPMAP:
	if (compSetBoolOption (o, value))
	    return TRUE;
	break;
    case DECOR_DISPLAY_OPTION_DECOR_MATCH:
    case DECOR_DISPLAY_OPTION_SHADOW_MATCH:
	if (compSetMatchOption (o, value))
	{
	    CompScreen *s;
	    CompWindow *w;

	    for (s = display->screens; s; s = s->next)
		for (w = s->windows; w; w = w->next)
		    decorWindowUpdate (w, TRUE);
	}
    default:
	break;
    }

    return FALSE;
}

static void
decorDisplayInitOptions (DecorDisplay *dd)
{
    CompOption *o;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_RADIUS];
    o->name		= "shadow_radius";
    o->shortDesc	= N_("Shadow Radius");
    o->longDesc		= N_("Drop shadow radius");
    o->type		= CompOptionTypeFloat;
    o->value.f		= DECOR_SHADOW_RADIUS_DEFAULT;
    o->rest.f.min	= DECOR_SHADOW_RADIUS_MIN;
    o->rest.f.max	= DECOR_SHADOW_RADIUS_MAX;
    o->rest.f.precision = DECOR_SHADOW_RADIUS_PRECISION;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_OPACITY];
    o->name		= "shadow_opacity";
    o->shortDesc	= N_("Shadow Opacity");
    o->longDesc		= N_("Drop shadow opacity");
    o->type		= CompOptionTypeFloat;
    o->value.f		= DECOR_SHADOW_OPACITY_DEFAULT;
    o->rest.f.min	= DECOR_SHADOW_OPACITY_MIN;
    o->rest.f.max	= DECOR_SHADOW_OPACITY_MAX;
    o->rest.f.precision = DECOR_SHADOW_OPACITY_PRECISION;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_COLOR];
    o->name		= "shadow_color";
    o->shortDesc	= "Shadow Color";
    o->longDesc		= "Drop shadow color";
    o->type		= CompOptionTypeColor;
    o->value.c[0]	= DECOR_SHADOW_COLOR_RED_DEFAULT;
    o->value.c[1]	= DECOR_SHADOW_COLOR_GREEN_DEFAULT;
    o->value.c[2]	= DECOR_SHADOW_COLOR_BLUE_DEFAULT;
    o->value.c[3]	= 0xffff;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_OFFSET_X];
    o->name		= "shadow_offset_x";
    o->shortDesc	= N_("Shadow Offset X");
    o->longDesc		= N_("Drop shadow X offset");
    o->type		= CompOptionTypeInt;
    o->value.i		= DECOR_SHADOW_OFFSET_DEFAULT;
    o->rest.i.min	= DECOR_SHADOW_OFFSET_MIN;
    o->rest.i.max	= DECOR_SHADOW_OFFSET_MAX;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_OFFSET_Y];
    o->name		= "shadow_offset_y";
    o->shortDesc	= N_("Shadow Offset Y");
    o->longDesc		= N_("Drop shadow Y offset");
    o->type		= CompOptionTypeInt;
    o->value.i		= DECOR_SHADOW_OFFSET_DEFAULT;
    o->rest.i.min	= DECOR_SHADOW_OFFSET_MIN;
    o->rest.i.max	= DECOR_SHADOW_OFFSET_MAX;

    o = &dd->opt[DECOR_DISPLAY_OPTION_COMMAND];
    o->name		= "command";
    o->shortDesc	= N_("Command");
    o->longDesc		= N_("Decorator command line that is executed if no "
			     "decorator is already running");
    o->type		= CompOptionTypeString;
    o->value.s		= strdup ("");
    o->rest.s.string	= NULL;
    o->rest.s.nString	= 0;

    o = &dd->opt[DECOR_DISPLAY_OPTION_MIPMAP];
    o->name	 = "mipmap";
    o->shortDesc = N_("Mipmap");
    o->longDesc	 = ("Allow mipmaps to be generated for decoration textures");
    o->type	 = CompOptionTypeBool;
    o->value.b   = DECOR_MIPMAP_DEFAULT;

    o = &dd->opt[DECOR_DISPLAY_OPTION_DECOR_MATCH];
    o->name	 = "decoration_match";
    o->shortDesc = N_("Decoration windows");
    o->longDesc	 = N_("Windows that should be decorated");
    o->type	 = CompOptionTypeMatch;

    matchInit (&o->value.match);
    matchAddFromString (&o->value.match, DECOR_DECOR_MATCH_DEFAULT);

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_MATCH];
    o->name	 = "shadow_match";
    o->shortDesc = N_("Shadow windows");
    o->longDesc	 = N_("Windows that should be have a shadow");
    o->type	 = CompOptionTypeMatch;

    matchInit (&o->value.match);
    matchAddFromString (&o->value.match, DECOR_SHADOW_MATCH_DEFAULT);
}

static void
decorWindowMoveNotify (CompWindow *w,
		       int	  dx,
		       int	  dy,
		       Bool	  immediate)
{
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    if (dw->wd)
    {
	WindowDecoration *wd = dw->wd;
	int		 i;

	for (i = 0; i < wd->nQuad; i++)
	{
	    wd->quad[i].box.x1 += dx;
	    wd->quad[i].box.y1 += dy;
	    wd->quad[i].box.x2 += dx;
	    wd->quad[i].box.y2 += dy;
	}

	setDecorationMatrices (w);
    }

    UNWRAP (ds, w->screen, windowMoveNotify);
    (*w->screen->windowMoveNotify) (w, dx, dy, immediate);
    WRAP (ds, w->screen, windowMoveNotify, decorWindowMoveNotify);
}

static void
decorWindowResizeNotify (CompWindow *w,
			 int	    dx,
			 int	    dy,
			 int	    dwidth,
			 int	    dheight)
{
    DECOR_SCREEN (w->screen);

    if (!decorWindowUpdate (w, FALSE))
	updateWindowDecorationScale (w);

    UNWRAP (ds, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (ds, w->screen, windowResizeNotify, decorWindowResizeNotify);
}

static void
decorWindowStateChangeNotify (CompWindow *w)
{
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    if (!decorWindowUpdate (w, FALSE))
    {
	if (dw->decor)
	{
	    if ((w->state & MAXIMIZE_STATE) == MAXIMIZE_STATE)
		setWindowFrameExtents (w, &dw->decor->maxInput);
	    else
		setWindowFrameExtents (w, &dw->decor->input);
	}
    }

    UNWRAP (ds, w->screen, windowStateChangeNotify);
    (*w->screen->windowStateChangeNotify) (w);
    WRAP (ds, w->screen, windowStateChangeNotify, decorWindowStateChangeNotify);
}

static void
decorMatchPropertyChanged (CompDisplay *d,
			   CompWindow  *w)
{
    DECOR_DISPLAY (d);

    decorWindowUpdate (w, FALSE);

    UNWRAP (dd, d, matchPropertyChanged);
    (*d->matchPropertyChanged) (d, w);
    WRAP (dd, d, matchPropertyChanged, decorMatchPropertyChanged);
}

static Bool
decorInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    DecorDisplay *dd;

    dd = malloc (sizeof (DecorDisplay));
    if (!dd)
	return FALSE;

    dd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (dd->screenPrivateIndex < 0)
    {
	free (dd);
	return FALSE;
    }

    dd->textures = 0;

    dd->supportingDmCheckAtom =
	XInternAtom (d->display, "_NET_SUPPORTING_DM_CHECK", 0);
    dd->winDecorAtom = XInternAtom (d->display, "_NET_WINDOW_DECOR", 0);
    dd->decorAtom[DECOR_BARE] =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_BARE", 0);
    dd->decorAtom[DECOR_NORMAL] =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_NORMAL", 0);
    dd->decorAtom[DECOR_ACTIVE] =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_ACTIVE", 0);

    decorDisplayInitOptions (dd);

    matchUpdate (d, &dd->opt[DECOR_DISPLAY_OPTION_DECOR_MATCH].value.match);
    matchUpdate (d, &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_MATCH].value.match);

    WRAP (dd, d, handleEvent, decorHandleEvent);
    WRAP (dd, d, matchPropertyChanged, decorMatchPropertyChanged);

    d->privates[displayPrivateIndex].ptr = dd;

    return TRUE;
}

static void
decorFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    DECOR_DISPLAY (d);

    matchFini (&dd->opt[DECOR_DISPLAY_OPTION_DECOR_MATCH].value.match);
    matchFini (&dd->opt[DECOR_DISPLAY_OPTION_SHADOW_MATCH].value.match);

    freeScreenPrivateIndex (d, dd->screenPrivateIndex);

    UNWRAP (dd, d, handleEvent);
    UNWRAP (dd, d, matchPropertyChanged);

    free (dd);
}

static Bool
decorInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    DecorScreen *ds;

    DECOR_DISPLAY (s->display);

    ds = malloc (sizeof (DecorScreen));
    if (!ds)
	return FALSE;

    ds->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ds->windowPrivateIndex < 0)
    {
	free (ds);
	return FALSE;
    }

    memset (ds->decor, 0, sizeof (ds->decor));

    ds->dmWin = None;

    WRAP (ds, s, drawWindow, decorDrawWindow);
    WRAP (ds, s, damageWindowRect, decorDamageWindowRect);
    WRAP (ds, s, getOutputExtentsForWindow, decorGetOutputExtentsForWindow);
    WRAP (ds, s, windowMoveNotify, decorWindowMoveNotify);
    WRAP (ds, s, windowResizeNotify, decorWindowResizeNotify);
    WRAP (ds, s, windowStateChangeNotify, decorWindowStateChangeNotify);

    s->privates[dd->screenPrivateIndex].ptr = ds;

    decorCheckForDmOnScreen (s, FALSE);

    return TRUE;
}

static void
decorFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    int i;

    DECOR_SCREEN (s);

    for (i = 0; i < DECOR_NUM; i++)
	if (ds->decor[i])
	    decorReleaseDecoration (s, ds->decor[i]);

    UNWRAP (ds, s, drawWindow);
    UNWRAP (ds, s, damageWindowRect);
    UNWRAP (ds, s, getOutputExtentsForWindow);
    UNWRAP (ds, s, windowMoveNotify);
    UNWRAP (ds, s, windowResizeNotify);
    UNWRAP (ds, s, windowStateChangeNotify);

    free (ds);
}

static Bool
decorInitWindow (CompPlugin *p,
		 CompWindow *w)
{
    DecorWindow *dw;

    DECOR_SCREEN (w->screen);

    dw = malloc (sizeof (DecorWindow));
    if (!dw)
	return FALSE;

    dw->wd    = NULL;
    dw->decor = NULL;

    w->privates[ds->windowPrivateIndex].ptr = dw;

    if (!w->attrib.override_redirect)
	decorWindowUpdateDecoration (w);

    if (w->shaded || w->attrib.map_state == IsViewable)
	decorWindowUpdate (w, FALSE);

    return TRUE;
}

static void
decorFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    DECOR_WINDOW (w);

    if (dw->wd)
	destroyWindowDecoration (w->screen, dw->wd);

    if (dw->decor)
	decorReleaseDecoration (w->screen, dw->decor);

    free (dw);
}

static Bool
decorInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
decorFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
decorGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

CompPluginDep decorDeps[] = {
    { CompPluginRuleBefore, "wobbly" },
    { CompPluginRuleBefore, "fade" },
    { CompPluginRuleBefore, "cube" },
    { CompPluginRuleBefore, "scale" }
};

CompPluginFeature decorFeatures[] = {
    { "decorations" }
};

static CompPluginVTable decorVTable = {
    "decoration",
    N_("Window Decoration"),
    N_("Window decorations"),
    decorGetVersion,
    decorInit,
    decorFini,
    decorInitDisplay,
    decorFiniDisplay,
    decorInitScreen,
    decorFiniScreen,
    decorInitWindow,
    decorFiniWindow,
    decorGetDisplayOptions,
    decorSetDisplayOption,
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    decorDeps,
    sizeof (decorDeps) / sizeof (decorDeps[0]),
    decorFeatures,
    sizeof (decorFeatures) / sizeof (decorFeatures[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &decorVTable;
}
