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

#include <stdlib.h>
#include <string.h>

#include <compiz.h>

static CompMetadata fadeMetadata;

static int displayPrivateIndex;

typedef struct _FadeDisplay {
    int			       screenPrivateIndex;
    HandleEventProc	       handleEvent;
    MatchExpHandlerChangedProc matchExpHandlerChanged;
    int			       displayModals;
} FadeDisplay;

#define FADE_SCREEN_OPTION_FADE_SPEED		  0
#define FADE_SCREEN_OPTION_WINDOW_MATCH		  1
#define FADE_SCREEN_OPTION_VISUAL_BELL		  2
#define FADE_SCREEN_OPTION_FULLSCREEN_VISUAL_BELL 3
#define FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE	  4
#define FADE_SCREEN_OPTION_NUM			  5

typedef struct _FadeScreen {
    int			   windowPrivateIndex;
    int			   fadeTime;

    CompOption opt[FADE_SCREEN_OPTION_NUM];

    PreparePaintScreenProc preparePaintScreen;
    PaintWindowProc	   paintWindow;
    DamageWindowRectProc   damageWindowRect;
    FocusWindowProc	   focusWindow;
    WindowResizeNotifyProc windowResizeNotify;

    CompMatch match;
} FadeScreen;

typedef struct _FadeWindow {
    GLushort opacity;
    GLushort brightness;
    GLushort saturation;

    int dModal;

    int destroyCnt;
    int unmapCnt;

    Bool shaded;
    Bool fadeOut;

    int steps;
} FadeWindow;

#define GET_FADE_DISPLAY(d)				     \
    ((FadeDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define FADE_DISPLAY(d)			   \
    FadeDisplay *fd = GET_FADE_DISPLAY (d)

#define GET_FADE_SCREEN(s, fd)					 \
    ((FadeScreen *) (s)->privates[(fd)->screenPrivateIndex].ptr)

#define FADE_SCREEN(s)							\
    FadeScreen *fs = GET_FADE_SCREEN (s, GET_FADE_DISPLAY (s->display))

#define GET_FADE_WINDOW(w, fs)				         \
    ((FadeWindow *) (w)->privates[(fs)->windowPrivateIndex].ptr)

#define FADE_WINDOW(w)					     \
    FadeWindow *fw = GET_FADE_WINDOW  (w,		     \
		     GET_FADE_SCREEN  (w->screen,	     \
		     GET_FADE_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static void
fadeUpdateWindowFadeMatch (CompDisplay     *display,
			   CompOptionValue *value,
			   CompMatch       *match)
{
    matchFini (match);
    matchInit (match);
    matchAddFromString (match, "!type=desktop");
    matchAddGroup (match, MATCH_OP_AND_MASK, &value->match);
    matchUpdate (display, match);
}

static CompOption *
fadeGetScreenOptions (CompPlugin *plugin,
		      CompScreen *screen,
		      int	 *count)
{
    FADE_SCREEN (screen);

    *count = NUM_OPTIONS (fs);
    return fs->opt;
}

static Bool
fadeSetScreenOption (CompPlugin      *plugin,
		     CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    FADE_SCREEN (screen);

    o = compFindOption (fs->opt, NUM_OPTIONS (fs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case FADE_SCREEN_OPTION_FADE_SPEED:
	if (compSetFloatOption (o, value))
	{
	    fs->fadeTime = 1000.0f / o->value.f;
	    return TRUE;
	}
	break;
    case FADE_SCREEN_OPTION_WINDOW_MATCH:
	if (compSetMatchOption (o, value))
	{
	    fadeUpdateWindowFadeMatch (screen->display, &o->value, &fs->match);
	    return TRUE;
	}
	break;
    default:
	if (compSetOption (o, value))
	    return TRUE;
	break;
    }

    return FALSE;
}

static void
fadePreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    CompWindow *w;
    int	       steps;

    FADE_SCREEN (s);

    steps = (msSinceLastPaint * OPAQUE) / fs->fadeTime;
    if (steps < 12)
	steps = 12;

    for (w = s->windows; w; w = w->next)
	GET_FADE_WINDOW (w, fs)->steps = steps;

    UNWRAP (fs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (fs, s, preparePaintScreen, fadePreparePaintScreen);
}

static void
fadeWindowStop (CompWindow *w)
{
    FADE_WINDOW (w);

    while (fw->unmapCnt)
    {
	unmapWindow (w);
	fw->unmapCnt--;
    }

    while (fw->destroyCnt)
    {
	destroyWindow (w);
	fw->destroyCnt--;
    }
}

static Bool
fadePaintWindow (CompWindow		 *w,
		 const WindowPaintAttrib *attrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 unsigned int		 mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    FADE_SCREEN (s);
    FADE_WINDOW (w);

    if (!w->screen->canDoSlightlySaturated)
	fw->saturation = attrib->saturation;

    if (fw->destroyCnt			     ||
	fw->unmapCnt			     ||
	fw->opacity    != attrib->opacity    ||
	fw->brightness != attrib->brightness ||
	fw->saturation != attrib->saturation)
    {
	WindowPaintAttrib fAttrib = *attrib;

	if (fw->fadeOut)
	    fAttrib.opacity = 0;

	if (fw->steps)
	{
	    GLint opacity;
	    GLint brightness;
	    GLint saturation;

	    opacity = fw->opacity;
	    if (fAttrib.opacity > fw->opacity)
	    {
		opacity = fw->opacity + fw->steps;
		if (opacity > fAttrib.opacity)
		    opacity = fAttrib.opacity;
	    }
	    else if (fAttrib.opacity < fw->opacity)
	    {
		if (w->type & CompWindowTypeUnknownMask)
		    opacity = fw->opacity - (fw->steps >> 1);
		else
		    opacity = fw->opacity - fw->steps;

		if (opacity < fAttrib.opacity)
		    opacity = fAttrib.opacity;
	    }

	    brightness = fw->brightness;
	    if (attrib->brightness > fw->brightness)
	    {
		brightness = fw->brightness + (fw->steps / 12);
		if (brightness > attrib->brightness)
		    brightness = attrib->brightness;
	    }
	    else if (attrib->brightness < fw->brightness)
	    {
		brightness = fw->brightness - (fw->steps / 12);
		if (brightness < attrib->brightness)
		    brightness = attrib->brightness;
	    }

	    saturation = fw->saturation;
	    if (attrib->saturation > fw->saturation)
	    {
		saturation = fw->saturation + (fw->steps / 6);
		if (saturation > attrib->saturation)
		    saturation = attrib->saturation;
	    }
	    else if (attrib->saturation < fw->saturation)
	    {
		saturation = fw->saturation - (fw->steps / 6);
		if (saturation < attrib->saturation)
		    saturation = attrib->saturation;
	    }

	    fw->steps = 0;

	    if (opacity > 0)
	    {
		fw->opacity    = opacity;
		fw->brightness = brightness;
		fw->saturation = saturation;

		if (opacity    != attrib->opacity    ||
		    brightness != attrib->brightness ||
		    saturation != attrib->saturation)
		    addWindowDamage (w);
	    }
	    else
	    {
		fw->opacity = 0;

		fadeWindowStop (w);
	    }
	}

	fAttrib.opacity	   = fw->opacity;
	fAttrib.brightness = fw->brightness;
	fAttrib.saturation = fw->saturation;

	UNWRAP (fs, s, paintWindow);
	status = (*s->paintWindow) (w, &fAttrib, transform, region, mask);
	WRAP (fs, s, paintWindow, fadePaintWindow);
    }
    else
    {
	UNWRAP (fs, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
	WRAP (fs, s, paintWindow, fadePaintWindow);
    }

    return status;
}

static void
fadeAddDisplayModal (CompDisplay *d,
		     CompWindow  *w)
{
    FADE_DISPLAY (d);
    FADE_WINDOW (w);

    if (!(w->state & CompWindowStateDisplayModalMask))
	return;

    if (fw->dModal)
	return;

    fw->dModal = 1;

    fd->displayModals++;
    if (fd->displayModals == 1)
    {
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	{
	    for (w = s->windows; w; w = w->next)
	    {
		FADE_WINDOW (w);

		if (fw->dModal)
		    continue;

		w->paint.brightness = 0xa8a8;
		w->paint.saturation = 0;
	    }

	    damageScreen (s);
	}
    }
}

static void
fadeRemoveDisplayModal (CompDisplay *d,
			CompWindow  *w)
{
    FADE_DISPLAY (d);
    FADE_WINDOW (w);

    if (!fw->dModal)
	return;

    fw->dModal = 0;

    fd->displayModals--;
    if (fd->displayModals == 0)
    {
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	{
	    for (w = s->windows; w; w = w->next)
	    {
		FADE_WINDOW (w);

		if (fw->dModal)
		    continue;

		if (w->alive)
		{
		    w->paint.brightness = w->brightness;
		    w->paint.saturation = w->saturation;
		}
	    }

	    damageScreen (s);
	}
    }
}

static void
fadeHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompWindow *w;

    FADE_DISPLAY (d);

    switch (event->type) {
    case DestroyNotify:
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	if (w)
	{
	    FADE_SCREEN (w->screen);

	    if (!fs->opt[FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE].value.b)
		break;

	    if (w->texture->pixmap && matchEval (&fs->match, w))
	    {
		FADE_WINDOW (w);

		if (fw->opacity == 0xffff)
		    fw->opacity = 0xfffe;

		fw->destroyCnt++;
		w->destroyRefCnt++;

		fw->fadeOut = TRUE;

		addWindowDamage (w);
	    }

	    fadeRemoveDisplayModal (d, w);
	}
	break;
    case UnmapNotify:
	w = findWindowAtDisplay (d, event->xunmap.window);
	if (w)
	{
	    FADE_SCREEN (w->screen);
	    FADE_WINDOW (w);

	    fw->shaded = w->shaded;

	    if (!fs->opt[FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE].value.b)
		break;

	    if (!fw->shaded && w->texture->pixmap && matchEval (&fs->match, w))
	    {
		if (fw->opacity == 0xffff)
		    fw->opacity = 0xfffe;

		fw->unmapCnt++;
		w->unmapRefCnt++;

		fw->fadeOut = TRUE;

		addWindowDamage (w);
	    }

	    fadeRemoveDisplayModal (d, w);
	}
	break;
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    FADE_SCREEN(w->screen);

	    if (!fs->opt[FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE].value.b)
		break;

	    fadeWindowStop (w);

	    if (w->state & CompWindowStateDisplayModalMask)
		fadeAddDisplayModal (d, w);
	}
	break;
    default:
	if (event->type == d->xkbEvent)
	{
	    XkbAnyEvent *xkbEvent = (XkbAnyEvent *) event;

	    if (xkbEvent->xkb_type == XkbBellNotify)
	    {
		XkbBellNotifyEvent *xkbBellEvent = (XkbBellNotifyEvent *)
		    xkbEvent;

		w = findWindowAtDisplay (d, xkbBellEvent->window);
		if (!w)
		    w = findWindowAtDisplay (d, d->activeWindow);

		if (w)
		{
		    CompScreen *s = w->screen;

		    FADE_SCREEN (s);

		    if (fs->opt[FADE_SCREEN_OPTION_VISUAL_BELL].value.b)
		    {
			int option;

			option = FADE_SCREEN_OPTION_FULLSCREEN_VISUAL_BELL;
			if (fs->opt[option].value.b)
			{
			    for (w = s->windows; w; w = w->next)
			    {
				if (w->destroyed)
				    continue;

				if (w->attrib.map_state != IsViewable)
				    continue;

				if (w->damaged)
				{
				    FADE_WINDOW (w);

				    fw->brightness = w->paint.brightness / 2;
				}
			    }

			    damageScreen (s);
			}
			else
			{
			    FADE_WINDOW (w);

			    fw->brightness = w->paint.brightness / 2;

			    addWindowDamage (w);
			}
		    }
		}
	    }
	}
	break;
    }

    UNWRAP (fd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (fd, d, handleEvent, fadeHandleEvent);

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == d->winStateAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w && w->attrib.map_state == IsViewable)
	    {
		if (w->state & CompWindowStateDisplayModalMask)
		    fadeAddDisplayModal (d, w);
		else
		    fadeRemoveDisplayModal (d, w);
	    }
	}
	break;
    }
}

static Bool
fadeDamageWindowRect (CompWindow *w,
		      Bool	 initial,
		      BoxPtr     rect)
{
    Bool status;

    FADE_SCREEN (w->screen);

    if (initial)
    {
	FADE_WINDOW (w);

	fw->fadeOut = FALSE;

	if (fw->shaded)
	{
	    fw->shaded = w->shaded;
	}
	else if (matchEval (&fs->match, w))
	{
	    if (fs->opt[FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE].value.b)
	    {
		fw->opacity = 0;
	    }
	}
    }

    UNWRAP (fs, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (fs, w->screen, damageWindowRect, fadeDamageWindowRect);

    return status;
}

static Bool
fadeFocusWindow (CompWindow *w)
{
    Bool status;

    FADE_SCREEN (w->screen);
    FADE_WINDOW (w);

    if (fw->destroyCnt || fw->unmapCnt)
	return FALSE;

    UNWRAP (fs, w->screen, focusWindow);
    status = (*w->screen->focusWindow) (w);
    WRAP (fs, w->screen, focusWindow, fadeFocusWindow);

    return status;
}

static void
fadeWindowResizeNotify (CompWindow *w,
			int	   dx,
			int	   dy,
			int	   dwidth,
			int	   dheight)
{
    FADE_SCREEN (w->screen);

    if (!w->mapNum)
	fadeWindowStop (w);

    UNWRAP (fs, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (fs, w->screen, windowResizeNotify, fadeWindowResizeNotify);
}

static void
fadeMatchExpHandlerChanged (CompDisplay *d)
{
    CompScreen *s;

    FADE_DISPLAY (d);

    for (s = d->screens; s; s = s->next)
	matchUpdate (d, &GET_FADE_SCREEN (s,fd)->match);

    UNWRAP (fd, d, matchExpHandlerChanged);
    (*d->matchExpHandlerChanged) (d);
    WRAP (fd, d, matchExpHandlerChanged, fadeMatchExpHandlerChanged);
}

static Bool
fadeInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    FadeDisplay *fd;

    fd = malloc (sizeof (FadeDisplay));
    if (!fd)
	return FALSE;

    fd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (fd->screenPrivateIndex < 0)
    {
	free (fd);
	return FALSE;
    }

    fd->displayModals = 0;

    WRAP (fd, d, handleEvent, fadeHandleEvent);
    WRAP (fd, d, matchExpHandlerChanged, fadeMatchExpHandlerChanged);

    d->privates[displayPrivateIndex].ptr = fd;

    return TRUE;
}

static void
fadeFiniDisplay (CompPlugin *p,
		 CompDisplay *d)
{
    FADE_DISPLAY (d);

    freeScreenPrivateIndex (d, fd->screenPrivateIndex);

    UNWRAP (fd, d, handleEvent);
    UNWRAP (fd, d, matchExpHandlerChanged);

    free (fd);
}

static const CompMetadataOptionInfo fadeScreenOptionInfo[] = {
    { "fade_speed", "float", "<min>0.1</min>", 0, 0 },
    { "window_match", "match", "<helper>true</helper>", 0, 0 },
    { "visual_bell", "bool", 0, 0, 0 },
    { "fullscreen_visual_bell", "bool", 0, 0, 0 },
    { "minimize_open_close", "bool", 0, 0, 0 }
};

static Bool
fadeInitScreen (CompPlugin *p,
		CompScreen *s)
{
    FadeScreen *fs;

    FADE_DISPLAY (s->display);

    fs = malloc (sizeof (FadeScreen));
    if (!fs)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &fadeMetadata,
					    fadeScreenOptionInfo,
					    fs->opt,
					    FADE_SCREEN_OPTION_NUM))
    {
	free (fs);
	return FALSE;
    }

    fs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (fs->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, fs->opt, FADE_SCREEN_OPTION_NUM);
	free (fs);
	return FALSE;
    }

    fs->fadeTime = 1000.0f / fs->opt[FADE_SCREEN_OPTION_FADE_SPEED].value.f;

    matchInit (&fs->match);

    fadeUpdateWindowFadeMatch (s->display,
			       &fs->opt[FADE_SCREEN_OPTION_WINDOW_MATCH].value,
			       &fs->match);

    WRAP (fs, s, preparePaintScreen, fadePreparePaintScreen);
    WRAP (fs, s, paintWindow, fadePaintWindow);
    WRAP (fs, s, damageWindowRect, fadeDamageWindowRect);
    WRAP (fs, s, focusWindow, fadeFocusWindow);
    WRAP (fs, s, windowResizeNotify, fadeWindowResizeNotify);

    s->privates[fd->screenPrivateIndex].ptr = fs;

    return TRUE;
}

static void
fadeFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    FADE_SCREEN (s);

    matchFini (&fs->match);

    freeWindowPrivateIndex (s, fs->windowPrivateIndex);

    UNWRAP (fs, s, preparePaintScreen);
    UNWRAP (fs, s, paintWindow);
    UNWRAP (fs, s, damageWindowRect);
    UNWRAP (fs, s, focusWindow);
    UNWRAP (fs, s, windowResizeNotify);

    compFiniScreenOptions (s, fs->opt, FADE_SCREEN_OPTION_NUM);

    free (fs);
}

static Bool
fadeInitWindow (CompPlugin *p,
		CompWindow *w)
{
    FadeWindow *fw;

    FADE_SCREEN (w->screen);

    fw = malloc (sizeof (FadeWindow));
    if (!fw)
	return FALSE;

    fw->opacity	   = w->paint.opacity;
    fw->brightness = w->paint.brightness;
    fw->saturation = w->paint.saturation;

    fw->dModal = 0;

    fw->destroyCnt = 0;
    fw->unmapCnt   = 0;
    fw->shaded     = w->shaded;
    fw->fadeOut    = FALSE;

    w->privates[fs->windowPrivateIndex].ptr = fw;

    if (w->attrib.map_state == IsViewable)
    {
	if (w->state & CompWindowStateDisplayModalMask)
	    fadeAddDisplayModal (w->screen->display, w);
    }

    return TRUE;
}

static void
fadeFiniWindow (CompPlugin *p,
		CompWindow *w)
{
    FADE_WINDOW (w);

    fadeRemoveDisplayModal (w->screen->display, w);
    fadeWindowStop (w);

    w->paint.opacity    = w->opacity;
    w->paint.brightness = w->brightness;
    w->paint.saturation = w->saturation;

    free (fw);
}

static Bool
fadeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&fadeMetadata, p->vTable->name, 0, 0,
					 fadeScreenOptionInfo,
					 FADE_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&fadeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&fadeMetadata, p->vTable->name);

    return TRUE;
}

static void
fadeFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&fadeMetadata);
}

static int
fadeGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompMetadata *
fadeGetMetadata (CompPlugin *plugin)
{
    return &fadeMetadata;
}

static CompPluginVTable fadeVTable = {
    "fade",
    fadeGetVersion,
    fadeGetMetadata,
    fadeInit,
    fadeFini,
    fadeInitDisplay,
    fadeFiniDisplay,
    fadeInitScreen,
    fadeFiniScreen,
    fadeInitWindow,
    fadeFiniWindow,
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    fadeGetScreenOptions,
    fadeSetScreenOption
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &fadeVTable;
}
