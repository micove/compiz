/*
 * Copyright © 2006 Novell, Inc.
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
#include <sys/time.h>

#include <compiz.h>

#define CLONE_INITIATE_BUTTON_DEFAULT           Button1
#define CLONE_INITIATE_BUTTON_MODIFIERS_DEFAULT (CompSuperMask | ShiftMask)

static int displayPrivateIndex;

#define CLONE_DISPLAY_OPTION_INITIATE 0
#define CLONE_DISPLAY_OPTION_NUM      1

typedef struct _CloneDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[CLONE_DISPLAY_OPTION_NUM];
} CloneDisplay;

typedef struct _CloneClone {
    int    src;
    int    dst;
    Region region;
    Window input;
} CloneClone;

typedef struct _CloneScreen {
    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc	   donePaintScreen;
    PaintScreenProc	   paintScreen;
    PaintWindowProc	   paintWindow;
    OutputChangeNotifyProc outputChangeNotify;

    int  grabIndex;
    Bool grab;

    float offset;

    Bool transformed;

    CloneClone *clone;
    int        nClone;

    int x, y;
    int grabbedOutput;
    int src, dst;
} CloneScreen;

#define GET_CLONE_DISPLAY(d)				      \
    ((CloneDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define CLONE_DISPLAY(d)		     \
    CloneDisplay *cd = GET_CLONE_DISPLAY (d)

#define GET_CLONE_SCREEN(s, cd)				          \
    ((CloneScreen *) (s)->privates[(cd)->screenPrivateIndex].ptr)

#define CLONE_SCREEN(s)						           \
    CloneScreen *cs = GET_CLONE_SCREEN (s, GET_CLONE_DISPLAY (s->display))

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static void
cloneRemove (CompScreen *s,
	     int	i)
{
    CloneClone *clone;

    CLONE_SCREEN (s);

    clone = malloc (sizeof (CloneClone) * (cs->nClone - 1));
    if (clone)
    {
	int j, k = 0;

	for (j = 0; j < cs->nClone; j++)
	    if (j != i)
		memcpy (&clone[k++], &cs->clone[j],
			sizeof (CloneClone));

	XDestroyRegion (cs->clone[i].region);
	XDestroyWindow (s->display->display, cs->clone[i].input);

	free (cs->clone);

	cs->clone = clone;
	cs->nClone--;
    }
}

static void
cloneFinish (CompScreen *s)
{
    CloneClone *clone;
    int        i;

    CLONE_SCREEN (s);

    cs->grab = FALSE;

    if (cs->src != cs->dst)
    {
	clone = NULL;

	/* check if we should replace current clone */
	for (i = 0; i < cs->nClone; i++)
	{
	    if (cs->clone[i].dst == cs->dst)
	    {
		clone = &cs->clone[i];
		break;
	    }
	}

	/* no existing clone for this destination, we must allocate one */
	if (!clone)
	{
	    Region region;

	    region = XCreateRegion ();
	    if (region)
	    {
		clone =
		    realloc (cs->clone,
			     sizeof (CloneClone) * (cs->nClone + 1));
		if (clone)
		{
		    XSetWindowAttributes attr;
		    int			 x, y;

		    cs->clone = clone;
		    clone = &cs->clone[cs->nClone++];
		    clone->region = region;

		    attr.override_redirect = TRUE;

		    x = s->outputDev[cs->dst].region.extents.x1;
		    y = s->outputDev[cs->dst].region.extents.y1;

		    clone->input =
			XCreateWindow (s->display->display,
				       s->root, x, y,
				       s->outputDev[cs->dst].width,
				       s->outputDev[cs->dst].height,
				       0, 0, InputOnly, CopyFromParent,
				       CWOverrideRedirect, &attr);
		    XMapRaised (s->display->display, clone->input);
		}
		else
		{
		    XDestroyRegion (region);
		}
	    }
	}

	if (clone)
	{
	    clone->src = cs->src;
	    clone->dst = cs->dst;
	}
    }

    if (cs->grabbedOutput != cs->dst)
    {
	/* remove clone */
	for (i = 0; i < cs->nClone; i++)
	{
	    if (cs->clone[i].dst == cs->grabbedOutput)
	    {
		cloneRemove (s, i);
		break;
	    }
	}
    }
}

static void
clonePreparePaintScreen (CompScreen *s,
			 int	    msSinceLastPaint)
{
    int i;

    CLONE_SCREEN (s);

    if (cs->grab)
    {
	if (cs->grabIndex)
	{
	    cs->offset -= msSinceLastPaint * 0.005f;
	    if (cs->offset < 0.0f)
		cs->offset = 0.0f;
	}
	else
	{
	    cs->offset += msSinceLastPaint * 0.005f;
	    if (cs->offset >= 1.0f)
	    {
		cs->offset = 1.0f;
		cloneFinish (s);
	    }
	}

	damageScreen (s);
    }

    UNWRAP (cs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (cs, s, preparePaintScreen, clonePreparePaintScreen);

    for (i = 0; i < cs->nClone; i++)
    {
	CompOutput *src = &s->outputDev[cs->clone[i].src];
	CompOutput *dst = &s->outputDev[cs->clone[i].dst];
	int	   dx, dy;

	dx = dst->region.extents.x1 - src->region.extents.x1;
	dy = dst->region.extents.y1 - src->region.extents.y1;

	if (s->damageMask & COMP_SCREEN_DAMAGE_REGION_MASK)
	{
	    if (src->width != dst->width || src->height != dst->height)
	    {
		XSubtractRegion (&dst->region, &emptyRegion,
				 cs->clone[i].region);
		XUnionRegion (s->damage, cs->clone[i].region, s->damage);
		XSubtractRegion (&src->region, &emptyRegion,
				 cs->clone[i].region);
	    }
	    else
	    {
		XSubtractRegion (s->damage, &dst->region, cs->clone[i].region);
		XOffsetRegion (cs->clone[i].region, dx, dy);
		XUnionRegion (s->damage, cs->clone[i].region, s->damage);
		XSubtractRegion (s->damage, &src->region, cs->clone[i].region);
		XOffsetRegion (cs->clone[i].region, -dx, -dy);
	    }
	}
	else
	{
	    XSubtractRegion (&src->region, &emptyRegion, cs->clone[i].region);
	}
    }
}

static void
cloneDonePaintScreen (CompScreen *s)
{
    CLONE_SCREEN (s);

    if (cs->grab && cs->offset)
	damageScreen (s);

    UNWRAP (cs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (cs, s, donePaintScreen, cloneDonePaintScreen);
}

static Bool
clonePaintScreen (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  Region		  region,
		  int			  output,
		  unsigned int		  mask)
{
    Bool status;
    int  i, dst = output;

    CLONE_SCREEN (s);

    if (!cs->grab || cs->grabbedOutput != output)
    {
	for (i = 0; i < cs->nClone; i++)
	{
	    if (cs->clone[i].dst == output)
	    {
		region = cs->clone[i].region;
		dst    = cs->clone[i].src;

		if (s->outputDev[dst].width  != s->outputDev[output].width ||
		    s->outputDev[dst].height != s->outputDev[output].height )
		    cs->transformed = TRUE;
		else
		    cs->transformed = FALSE;

		break;
	    }
	}
    }

    UNWRAP (cs, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, region, dst, mask);
    WRAP (cs, s, paintScreen, clonePaintScreen);

    if (cs->grab)
    {
	CompWindow *w;
	GLenum	   filter;
	float      zoom1, zoom2x, zoom2y, x1, y1, x2, y2;
	float      zoomX, zoomY;
	int        dx, dy;

	zoom1 = 160.0f / s->outputDev[cs->src].height;

	x1 = cs->x - (s->outputDev[cs->src].region.extents.x1 * zoom1);
	y1 = cs->y - (s->outputDev[cs->src].region.extents.y1 * zoom1);

	x1 -= (s->outputDev[cs->src].width  * zoom1) / 2;
	y1 -= (s->outputDev[cs->src].height * zoom1) / 2;

	if (cs->grabIndex)
	{
	    x2 = s->outputDev[cs->grabbedOutput].region.extents.x1 -
		s->outputDev[cs->src].region.extents.x1;
	    y2 = s->outputDev[cs->grabbedOutput].region.extents.y1 -
		s->outputDev[cs->src].region.extents.y1;

	    zoom2x = (float) s->outputDev[cs->grabbedOutput].width /
		s->outputDev[cs->src].width;
	    zoom2y = (float) s->outputDev[cs->grabbedOutput].height /
		s->outputDev[cs->src].height;
	}
	else
	{
	    x2 = s->outputDev[cs->dst].region.extents.x1 -
		s->outputDev[cs->src].region.extents.x1;
	    y2 = s->outputDev[cs->dst].region.extents.y1 -
		s->outputDev[cs->src].region.extents.y1;

	    zoom2x = (float) s->outputDev[cs->dst].width /
		s->outputDev[cs->src].width;
	    zoom2y = (float) s->outputDev[cs->dst].height /
		s->outputDev[cs->src].height;
	}

	/* XXX: hmm.. why do I need this.. */
	if (x2 < 0.0f)
	    x2 *= zoom2x;
	if (y2 < 0.0f)
	    y2 *= zoom2y;

	dx = x1 * (1.0f - cs->offset) + x2 * cs->offset;
	dy = y1 * (1.0f - cs->offset) + y2 * cs->offset;

	zoomX = zoom1 * (1.0f - cs->offset) + zoom2x * cs->offset;
	zoomY = zoom1 * (1.0f - cs->offset) + zoom2y * cs->offset;

	glPushMatrix ();

	glTranslatef (-0.5f, -0.5f, -DEFAULT_Z_CAMERA);
	glScalef (1.0f  / s->outputDev[output].width,
		  -1.0f / s->outputDev[output].height,
		  1.0f);
	glTranslatef (dx - s->outputDev[output].region.extents.x1,
		      dy - s->outputDev[output].region.extents.y2,
		      0.0f);
	glScalef (zoomX, zoomY, 1.0f);

	filter = s->display->textureFilter;

	if (cs->offset == 0.0f)
	    s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

	for (w = s->windows; w; w = w->next)
	{
	    if (w->destroyed)
		continue;

	    if (!w->shaded)
	    {
		if (w->attrib.map_state != IsViewable || !w->damaged)
		    continue;
	    }

	    (*s->paintWindow) (w, &w->paint, &s->outputDev[cs->src].region,
			       PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK);
	}

	s->display->textureFilter = filter;

	glPopMatrix ();
    }

    return status;
}

static Bool
clonePaintWindow (CompWindow		  *w,
		  const WindowPaintAttrib *attrib,
		  Region		  region,
		  unsigned int		  mask)
{
    Bool status;

    CLONE_SCREEN (w->screen);

    if (cs->nClone && cs->transformed)
	mask |= PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;

    UNWRAP (cs, w->screen, paintWindow);
    status = (*w->screen->paintWindow) (w, attrib, region, mask);
    WRAP (cs, w->screen, paintWindow, clonePaintWindow);

    return status;
}

static Bool
cloneInitiate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	int i;

	CLONE_SCREEN (s);

	if (cs->grab || otherScreenGrabExist (s, "clone", 0))
	    return FALSE;

	if (!cs->grabIndex)
	    cs->grabIndex = pushScreenGrab (s, None, "clone");

	cs->grab = TRUE;

	cs->x = getIntOptionNamed (option, nOption, "x", 0);
	cs->y = getIntOptionNamed (option, nOption, "y", 0);

	cs->src = cs->grabbedOutput = outputDeviceForPoint (s, cs->x, cs->y);

	/* trace source */
	i = 0;
	while (i < cs->nClone)
	{
	    if (cs->clone[i].dst == cs->src)
	    {
		cs->src = cs->clone[i].src;
		i = 0;
	    }
	    else
	    {
		i++;
	    }
	}

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;
    }

    return FALSE;
}

static Bool
cloneTerminate (CompDisplay     *d,
		CompAction      *action,
		CompActionState state,
		CompOption      *option,
		int	        nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	CLONE_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (cs->grabIndex)
	{
	    int	x, y;

	    removeScreenGrab (s, cs->grabIndex, NULL);
	    cs->grabIndex = 0;

	    x = getIntOptionNamed (option, nOption, "x", 0);
	    y = getIntOptionNamed (option, nOption, "y", 0);

	    cs->dst = outputDeviceForPoint (s, x, y);

	    damageScreen (s);
	}
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static void
cloneSetStrutsForCloneWindow (CompScreen *s,
			      CloneClone *clone)
{
    CompOutput *output = &s->outputDev[clone->dst];
    XRectangle *rect = NULL;
    CompStruts *struts;
    CompWindow *w;

    w = findWindowAtScreen (s, clone->input);
    if (!w)
	return;

    struts = malloc (sizeof (CompStruts));
    if (!struts)
	return;

    if (w->struts)
	free (w->struts);

    struts->left.x	= 0;
    struts->left.y	= 0;
    struts->left.width  = 0;
    struts->left.height = s->height;

    struts->right.x      = s->width;
    struts->right.y      = 0;
    struts->right.width  = 0;
    struts->right.height = s->height;

    struts->top.x      = 0;
    struts->top.y      = 0;
    struts->top.width  = s->width;
    struts->top.height = 0;

    struts->bottom.x      = 0;
    struts->bottom.y      = s->height;
    struts->bottom.width  = s->width;
    struts->bottom.height = 0;

    /* create struts relative to a screen edge that this output is next to */
    if (output->region.extents.x1 == 0)
	rect = &struts->left;
    else if (output->region.extents.x2 == s->width)
	rect = &struts->right;
    else if (output->region.extents.y1 == 0)
	rect = &struts->top;
    else if (output->region.extents.y2 == s->height)
	rect = &struts->bottom;

    if (rect)
    {
	rect->x	     = output->region.extents.x1;
	rect->y	     = output->region.extents.y1;
	rect->width  = output->width;
	rect->height = output->height;
    }

    w->struts = struts;
}

static void
cloneHandleMotionEvent (CompScreen *s,
			int	  xRoot,
			int	  yRoot)
{
    CLONE_SCREEN (s);

    if (cs->grabIndex)
    {
	cs->x = xRoot;
	cs->y = yRoot;

	damageScreen (s);
    }
}

static void
cloneHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    CompScreen *s;

    CLONE_DISPLAY (d);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    cloneHandleMotionEvent (s, pointerX, pointerY);
	break;
    case EnterNotify:
    case LeaveNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    cloneHandleMotionEvent (s, pointerX, pointerY);
    default:
	break;
    }

    UNWRAP (cd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (cd, d, handleEvent, cloneHandleEvent);

    switch (event->type) {
    case CreateNotify:
	s = findScreenAtDisplay (d, event->xcreatewindow.parent);
	if (s)
	{
	    int	i;

	    CLONE_SCREEN (s);

	    for (i = 0; i < cs->nClone; i++)
		if (event->xcreatewindow.window == cs->clone[i].input)
		    cloneSetStrutsForCloneWindow (s, &cs->clone[i]);
	}
    default:
	break;
    }
}

static void
cloneOutputChangeNotify (CompScreen *s)
{
    int i;

    CLONE_SCREEN (s);

    /* remove clones with destination or source that doesn't exist */
    for (i = 0; i < cs->nClone; i++)
    {
	if (cs->clone[i].dst >= s->nOutputDev ||
	    cs->clone[i].src >= s->nOutputDev)
	{
	    cloneRemove (s, i);
	    i = 0;
	    continue;
	}
    }

    UNWRAP (cs, s, outputChangeNotify);
    (*s->outputChangeNotify) (s);
    WRAP (cs, s, outputChangeNotify, cloneOutputChangeNotify);
}

static CompOption *
cloneGetDisplayOptions (CompDisplay *display,
			int	    *count)
{
    CLONE_DISPLAY (display);

    *count = NUM_OPTIONS (cd);
    return cd->opt;
}

static Bool
cloneSetDisplayOption (CompDisplay     *display,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    CLONE_DISPLAY (display);

    o = compFindOption (cd->opt, NUM_OPTIONS (cd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case CLONE_DISPLAY_OPTION_INITIATE:
	if (setDisplayAction (display, o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
cloneDisplayInitOptions (CloneDisplay *cd)
{
    CompOption *o;

    o = &cd->opt[CLONE_DISPLAY_OPTION_INITIATE];
    o->name			     = "initiate";
    o->shortDesc		     = N_("Initiate");
    o->longDesc			     = N_("Initiate clone selection");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = cloneInitiate;
    o->value.action.terminate	     = cloneTerminate;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.state	     = CompActionStateInitButton;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.button.modifiers = CLONE_INITIATE_BUTTON_MODIFIERS_DEFAULT;
    o->value.action.button.button    = CLONE_INITIATE_BUTTON_DEFAULT;
}

static Bool
cloneInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    CloneDisplay *cd;

    cd = malloc (sizeof (CloneDisplay));
    if (!cd)
	return FALSE;

    cd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (cd->screenPrivateIndex < 0)
    {
	free (cd);
	return FALSE;
    }

    cloneDisplayInitOptions (cd);

    WRAP (cd, d, handleEvent, cloneHandleEvent);

    d->privates[displayPrivateIndex].ptr = cd;

    return TRUE;
}

static void
cloneFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    CLONE_DISPLAY (d);

    freeScreenPrivateIndex (d, cd->screenPrivateIndex);

    UNWRAP (cd, d, handleEvent);

    free (cd);
}

static Bool
cloneInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    CloneScreen *cs;

    CLONE_DISPLAY (s->display);

    cs = malloc (sizeof (CloneScreen));
    if (!cs)
	return FALSE;

    cs->grabIndex = 0;
    cs->grab      = FALSE;

    cs->offset = 1.0f;

    cs->transformed = FALSE;

    cs->nClone = 0;
    cs->clone  = NULL;

    cs->src = 0;

    addScreenAction (s, &cd->opt[CLONE_DISPLAY_OPTION_INITIATE].value.action);

    WRAP (cs, s, preparePaintScreen, clonePreparePaintScreen);
    WRAP (cs, s, donePaintScreen, cloneDonePaintScreen);
    WRAP (cs, s, paintScreen, clonePaintScreen);
    WRAP (cs, s, paintWindow, clonePaintWindow);
    WRAP (cs, s, outputChangeNotify, cloneOutputChangeNotify);

    s->privates[cd->screenPrivateIndex].ptr = cs;

    return TRUE;
}

static void
cloneFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    CLONE_SCREEN (s);

    UNWRAP (cs, s, preparePaintScreen);
    UNWRAP (cs, s, donePaintScreen);
    UNWRAP (cs, s, paintScreen);
    UNWRAP (cs, s, paintWindow);
    UNWRAP (cs, s, outputChangeNotify);

    free (cs);
}

static Bool
cloneInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
cloneFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
cloneGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

CompPluginVTable cloneVTable = {
    "clone",
    N_("Clone Output"),
    N_("Output clone handler"),
    cloneGetVersion,
    cloneInit,
    cloneFini,
    cloneInitDisplay,
    cloneFiniDisplay,
    cloneInitScreen,
    cloneFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    cloneGetDisplayOptions,
    cloneSetDisplayOption,
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &cloneVTable;
}