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

/*
 * Spring model implemented by Kristian Hogsberg.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <compiz.h>

#define WIN_X(w) ((w)->attrib.x - (w)->output.left)
#define WIN_Y(w) ((w)->attrib.y - (w)->output.top)
#define WIN_W(w) ((w)->width + (w)->output.left + (w)->output.right)
#define WIN_H(w) ((w)->height + (w)->output.top + (w)->output.bottom)

#define GRID_WIDTH  4
#define GRID_HEIGHT 4

#define MODEL_MAX_SPRINGS (GRID_WIDTH * GRID_HEIGHT * 2)

#define MASS 15.0f

typedef struct _xy_pair {
    float x, y;
} Point, Vector;

#define NorthEdgeMask (1L << 0)
#define SouthEdgeMask (1L << 1)
#define WestEdgeMask  (1L << 2)
#define EastEdgeMask  (1L << 3)

#define EDGE_DISTANCE 25.0f
#define EDGE_VELOCITY 13.0f

typedef struct _Edge {
    float next, prev;

    float start;
    float end;

    float attract;
    float velocity;

    Bool  snapped;
} Edge;

typedef struct _Object {
    Vector	 force;
    Point	 position;
    Vector	 velocity;
    float	 theta;
    Bool	 immobile;
    unsigned int edgeMask;
    Edge	 vertEdge;
    Edge	 horzEdge;
} Object;

typedef struct _Spring {
    Object *a;
    Object *b;
    Vector offset;
} Spring;

#define NORTH 0
#define SOUTH 1
#define WEST  2
#define EAST  3

typedef struct _Model {
    Object	 *objects;
    int		 numObjects;
    Spring	 springs[MODEL_MAX_SPRINGS];
    int		 numSprings;
    Object	 *anchorObject;
    float	 steps;
    Vector	 scale;
    Point	 scaleOrigin;
    Bool	 transformed;
    Point	 topLeft;
    Point	 bottomRight;
    unsigned int edgeMask;
    unsigned int snapCnt[4];
} Model;

#define WOBBLY_FRICTION_DEFAULT    3.0f
#define WOBBLY_FRICTION_MIN        0.1f
#define WOBBLY_FRICTION_MAX       10.0f
#define WOBBLY_FRICTION_PRECISION  0.1f

#define WOBBLY_SPRING_K_DEFAULT    8.0f
#define WOBBLY_SPRING_K_MIN        0.1f
#define WOBBLY_SPRING_K_MAX       10.0f
#define WOBBLY_SPRING_K_PRECISION  0.1f

#define WOBBLY_GRID_RESOLUTION_DEFAULT  8
#define WOBBLY_GRID_RESOLUTION_MIN      1
#define WOBBLY_GRID_RESOLUTION_MAX      64

#define WOBBLY_MIN_GRID_SIZE_DEFAULT  8
#define WOBBLY_MIN_GRID_SIZE_MIN      4
#define WOBBLY_MIN_GRID_SIZE_MAX      128

typedef enum {
    WobblyEffectNone = 0,
    WobblyEffectShiver
} WobblyEffect;

static char *effectName[] = {
    N_("None"),
    N_("Shiver")
};

static WobblyEffect effectType[] = {
    WobblyEffectNone,
    WobblyEffectShiver
};

#define NUM_EFFECT (sizeof (effectType) / sizeof (effectType[0]))

#define WOBBLY_MAP_DEFAULT   (effectName[1])
#define WOBBLY_FOCUS_DEFAULT (effectName[0])

static char *mapWinType[] = {
    N_("Splash"),
    N_("DropdownMenu"),
    N_("PopupMenu"),
    N_("Tooltip"),
    N_("Notification"),
    N_("Combo"),
    N_("Dnd"),
    N_("Unknown")
};
#define N_MAP_WIN_TYPE (sizeof (mapWinType) / sizeof (mapWinType[0]))
#define N_FOCUS_WIN_TYPE (0)

static char *moveWinType[] = {
    N_("Toolbar"),
    N_("Menu"),
    N_("Utility"),
    N_("Dialog"),
    N_("ModalDialog"),
    N_("Normal")
};
#define N_MOVE_WIN_TYPE (sizeof (moveWinType) / sizeof (moveWinType[0]))
#define N_GRAB_WIN_TYPE (0)

#define WOBBLY_SNAP_MODIFIERS_DEFAULT ShiftMask

#define WOBBLY_MAXIMIZE_EFFECT_DEFAULT TRUE

static int displayPrivateIndex;

#define WOBBLY_DISPLAY_OPTION_SNAP   0
#define WOBBLY_DISPLAY_OPTION_SHIVER 1
#define WOBBLY_DISPLAY_OPTION_NUM    2

typedef struct _WobblyDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[WOBBLY_DISPLAY_OPTION_NUM];

    Bool snapping;
} WobblyDisplay;

#define WOBBLY_SCREEN_OPTION_FRICTION	       0
#define WOBBLY_SCREEN_OPTION_SPRING_K	       1
#define WOBBLY_SCREEN_OPTION_GRID_RESOLUTION   2
#define WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE     3
#define WOBBLY_SCREEN_OPTION_MAP_EFFECT	       4
#define WOBBLY_SCREEN_OPTION_FOCUS_EFFECT      5
#define WOBBLY_SCREEN_OPTION_MAP_WINDOW_TYPE   6
#define WOBBLY_SCREEN_OPTION_FOCUS_WINDOW_TYPE 7
#define WOBBLY_SCREEN_OPTION_GRAB_WINDOW_TYPE  8
#define WOBBLY_SCREEN_OPTION_MOVE_WINDOW_TYPE  9
#define WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT   10
#define WOBBLY_SCREEN_OPTION_NUM	       11

typedef struct _WobblyScreen {
    int	windowPrivateIndex;

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc	   donePaintScreen;
    PaintScreenProc	   paintScreen;
    PaintWindowProc	   paintWindow;
    DamageWindowRectProc   damageWindowRect;
    AddWindowGeometryProc  addWindowGeometry;
    DrawWindowGeometryProc drawWindowGeometry;
    SetWindowScaleProc     setWindowScale;

    WindowResizeNotifyProc windowResizeNotify;
    WindowMoveNotifyProc   windowMoveNotify;
    WindowGrabNotifyProc   windowGrabNotify;
    WindowUngrabNotifyProc windowUngrabNotify;

    CompOption opt[WOBBLY_SCREEN_OPTION_NUM];

    Bool wobblyWindows;

    WobblyEffect mapEffect;
    WobblyEffect focusEffect;

    unsigned int mapWMask;
    unsigned int focusWMask;
    unsigned int moveWMask;
    unsigned int grabWMask;

    unsigned int grabMask;
    CompWindow	 *grabWindow;
} WobblyScreen;

#define WobblyInitial  (1L << 0)
#define WobblyForce    (1L << 1)
#define WobblyVelocity (1L << 2)

typedef struct _WobblyWindow {
    Model	 *model;
    int		 wobbly;
    Bool	 grabbed;
    Bool	 velocity;
    unsigned int state;
} WobblyWindow;

#define GET_WOBBLY_DISPLAY(d)				       \
    ((WobblyDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define WOBBLY_DISPLAY(d)		       \
    WobblyDisplay *wd = GET_WOBBLY_DISPLAY (d)

#define GET_WOBBLY_SCREEN(s, wd)				   \
    ((WobblyScreen *) (s)->privates[(wd)->screenPrivateIndex].ptr)

#define WOBBLY_SCREEN(s)						      \
    WobblyScreen *ws = GET_WOBBLY_SCREEN (s, GET_WOBBLY_DISPLAY (s->display))

#define GET_WOBBLY_WINDOW(w, ws)				   \
    ((WobblyWindow *) (w)->privates[(ws)->windowPrivateIndex].ptr)

#define WOBBLY_WINDOW(w)				         \
    WobblyWindow *ww = GET_WOBBLY_WINDOW  (w,		         \
		       GET_WOBBLY_SCREEN  (w->screen,	         \
		       GET_WOBBLY_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
wobblyGetScreenOptions (CompScreen *screen,
			int	   *count)
{
    WOBBLY_SCREEN (screen);

    *count = NUM_OPTIONS (ws);
    return ws->opt;
}

static Bool
wobblySetScreenOption (CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    WOBBLY_SCREEN (screen);

    o = compFindOption (ws->opt, NUM_OPTIONS (ws), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case WOBBLY_SCREEN_OPTION_FRICTION:
    case WOBBLY_SCREEN_OPTION_SPRING_K:
	if (compSetFloatOption (o, value))
	    return TRUE;
	break;
    case WOBBLY_SCREEN_OPTION_GRID_RESOLUTION:
	if (compSetIntOption (o, value))
	    return TRUE;
	break;
    case WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE:
	if (compSetIntOption (o, value))
	    return TRUE;
	break;
    case WOBBLY_SCREEN_OPTION_MAP_EFFECT:
	if (compSetStringOption (o, value))
	{
	    int i;

	    for (i = 0; i < NUM_EFFECT; i++)
	    {
		if (strcmp (o->value.s, effectName[i]) == 0)
		{
		    ws->mapEffect = effectType[i];
		    return TRUE;
		}
	    }
	}
	break;
    case WOBBLY_SCREEN_OPTION_FOCUS_EFFECT:
	if (compSetStringOption (o, value))
	{
	    int i;

	    for (i = 0; i < NUM_EFFECT; i++)
	    {
		if (strcmp (o->value.s, effectName[i]) == 0)
		{
		    ws->focusEffect = effectType[i];
		    return TRUE;
		}
	    }
	}
	break;
    case WOBBLY_SCREEN_OPTION_MAP_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    ws->mapWMask = compWindowTypeMaskFromStringList (&o->value);
	    return TRUE;
	}
	break;
    case WOBBLY_SCREEN_OPTION_FOCUS_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    ws->focusWMask = compWindowTypeMaskFromStringList (&o->value);
	    return TRUE;
	}
	break;
    case WOBBLY_SCREEN_OPTION_MOVE_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    ws->moveWMask = compWindowTypeMaskFromStringList (&o->value);
	    return TRUE;
	}
	break;
    case WOBBLY_SCREEN_OPTION_GRAB_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    ws->grabWMask = compWindowTypeMaskFromStringList (&o->value);
	    return TRUE;
	}
	break;
    case WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT:
	if (compSetBoolOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
wobblyScreenInitOptions (WobblyScreen *ws,
			 Display      *display)
{
    CompOption *o;
    int	       i;

    o = &ws->opt[WOBBLY_SCREEN_OPTION_FRICTION];
    o->name		= "friction";
    o->shortDesc	= N_("Friction");
    o->longDesc		= N_("Spring Friction");
    o->type		= CompOptionTypeFloat;
    o->value.f		= WOBBLY_FRICTION_DEFAULT;
    o->rest.f.min	= WOBBLY_FRICTION_MIN;
    o->rest.f.max	= WOBBLY_FRICTION_MAX;
    o->rest.f.precision = WOBBLY_FRICTION_PRECISION;

    o = &ws->opt[WOBBLY_SCREEN_OPTION_SPRING_K];
    o->name		= "spring_k";
    o->shortDesc	= N_("Spring K");
    o->longDesc		= N_("Spring Konstant");
    o->type		= CompOptionTypeFloat;
    o->value.f		= WOBBLY_SPRING_K_DEFAULT;
    o->rest.f.min	= WOBBLY_SPRING_K_MIN;
    o->rest.f.max	= WOBBLY_SPRING_K_MAX;
    o->rest.f.precision = WOBBLY_SPRING_K_PRECISION;

    o = &ws->opt[WOBBLY_SCREEN_OPTION_GRID_RESOLUTION];
    o->name	  = "grid_resolution";
    o->shortDesc  = N_("Grid Resolution");
    o->longDesc	  = N_("Vertex Grid Resolution");
    o->type	  = CompOptionTypeInt;
    o->value.i	  = WOBBLY_GRID_RESOLUTION_DEFAULT;
    o->rest.i.min = WOBBLY_GRID_RESOLUTION_MIN;
    o->rest.i.max = WOBBLY_GRID_RESOLUTION_MAX;

    o = &ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE];
    o->name	  = "min_grid_size";
    o->shortDesc  = N_("Minimum Grid Size");
    o->longDesc	  = N_("Minimum Vertex Grid Size");
    o->type	  = CompOptionTypeInt;
    o->value.i	  = WOBBLY_MIN_GRID_SIZE_DEFAULT;
    o->rest.i.min = WOBBLY_MIN_GRID_SIZE_MIN;
    o->rest.i.max = WOBBLY_MIN_GRID_SIZE_MAX;

    o = &ws->opt[WOBBLY_SCREEN_OPTION_MAP_EFFECT];
    o->name	      = "map_effect";
    o->shortDesc      = N_("Map Effect");
    o->longDesc	      = N_("Map Window Effect");
    o->type	      = CompOptionTypeString;
    o->value.s	      = strdup (WOBBLY_MAP_DEFAULT);
    o->rest.s.string  = effectName;
    o->rest.s.nString = NUM_EFFECT;

    o = &ws->opt[WOBBLY_SCREEN_OPTION_FOCUS_EFFECT];
    o->name	      = "focus_effect";
    o->shortDesc      = N_("Focus Effect");
    o->longDesc	      = N_("Focus Window Effect");
    o->type	      = CompOptionTypeString;
    o->value.s	      = strdup (WOBBLY_FOCUS_DEFAULT);
    o->rest.s.string  = effectName;
    o->rest.s.nString = NUM_EFFECT;

    o = &ws->opt[WOBBLY_SCREEN_OPTION_MAP_WINDOW_TYPE];
    o->name	         = "map_window_types";
    o->shortDesc         = N_("Map Window Types");
    o->longDesc	         = N_("Window types that should wobble when mapped");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_MAP_WIN_TYPE;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_MAP_WIN_TYPE);
    for (i = 0; i < N_MAP_WIN_TYPE; i++)
	o->value.list.value[i].s = strdup (mapWinType[i]);
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    ws->mapWMask = compWindowTypeMaskFromStringList (&o->value);

    o = &ws->opt[WOBBLY_SCREEN_OPTION_FOCUS_WINDOW_TYPE];
    o->name	         = "focus_window_types";
    o->shortDesc         = N_("Focus Window Types");
    o->longDesc	         = N_("Window types that should wobble when focused");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_FOCUS_WIN_TYPE;
    o->value.list.value  = NULL;
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    ws->focusWMask = compWindowTypeMaskFromStringList (&o->value);

    o = &ws->opt[WOBBLY_SCREEN_OPTION_MOVE_WINDOW_TYPE];
    o->name	         = "move_window_types";
    o->shortDesc         = N_("Move Window Types");
    o->longDesc	         = N_("Window types that should wobble when moved");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_MOVE_WIN_TYPE;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_MOVE_WIN_TYPE);
    for (i = 0; i < N_MOVE_WIN_TYPE; i++)
	o->value.list.value[i].s = strdup (moveWinType[i]);
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    ws->moveWMask = compWindowTypeMaskFromStringList (&o->value);

    o = &ws->opt[WOBBLY_SCREEN_OPTION_GRAB_WINDOW_TYPE];
    o->name	         = "grab_window_types";
    o->shortDesc         = N_("Grab Window Types");
    o->longDesc	         = N_("Window types that should wobble when grabbed");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_GRAB_WIN_TYPE;
    o->value.list.value  = NULL;
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    ws->grabWMask = compWindowTypeMaskFromStringList (&o->value);

    o = &ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT];
    o->name	  = "maximize_effect";
    o->shortDesc  = N_("Maximize Effect");
    o->longDesc	  = N_("Wobble effect when maximizing and unmaximizing "
		       "windows");
    o->type	  = CompOptionTypeBool;
    o->value.b    = WOBBLY_MAXIMIZE_EFFECT_DEFAULT;
}

static void
findNextWestEdge (CompWindow *w,
		  Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int x;

    start = -65535.0f;
    end   =  65535.0f;

    v1 = -65535.0f;
    v2 =  65535.0f;

    x = object->position.x + w->output.left - w->input.left;

    if (x >= w->screen->workArea.x)
    {
	CompWindow *p;

	v1 = w->screen->workArea.x;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (p->invisible || w == p || p->type != CompWindowTypeNormalMask)
		continue;

	    s = p->attrib.y - p->output.top;
	    e = p->attrib.y + p->height + p->output.bottom;

	    if (s > object->position.y)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.y)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		v = p->attrib.x + p->width + p->input.right;
		if (v <= x)
		{
		    if (v > v1)
			v1 = v;
		}
		else
		{
		    if (v < v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = w->screen->workArea.x;
    }

    v1 = v1 - w->output.left + w->input.left;
    v2 = v2 - w->output.left + w->input.left;

    if (v1 != (int) object->vertEdge.next)
	object->vertEdge.snapped = FALSE;

    object->vertEdge.start = start;
    object->vertEdge.end   = end;

    object->vertEdge.next = v1;
    object->vertEdge.prev = v2;

    object->vertEdge.attract  = v1 + EDGE_DISTANCE;
    object->vertEdge.velocity = EDGE_VELOCITY;
}

static void
findNextEastEdge (CompWindow *w,
		  Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int x;

    start = -65535.0f;
    end   =  65535.0f;

    v1 =  65535.0f;
    v2 = -65535.0f;

    x = object->position.x - w->output.right + w->input.right;

    if (x <= w->screen->workArea.x + w->screen->workArea.width)
    {
	CompWindow *p;

	v1 = w->screen->workArea.x + w->screen->workArea.width;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (p->invisible || w == p || p->type != CompWindowTypeNormalMask)
		continue;

	    s = p->attrib.y - p->output.top;
	    e = p->attrib.y + p->height + p->output.bottom;

	    if (s > object->position.y)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.y)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		v = p->attrib.x - p->input.left;
		if (v >= x)
		{
		    if (v < v1)
			v1 = v;
		}
		else
		{
		    if (v > v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = w->screen->workArea.x + w->screen->workArea.width;
    }

    v1 = v1 + w->output.right - w->input.right;
    v2 = v2 + w->output.right - w->input.right;

    if (v1 != (int) object->vertEdge.next)
	object->vertEdge.snapped = FALSE;

    object->vertEdge.start = start;
    object->vertEdge.end   = end;

    object->vertEdge.next = v1;
    object->vertEdge.prev = v2;

    object->vertEdge.attract  = v1 - EDGE_DISTANCE;
    object->vertEdge.velocity = EDGE_VELOCITY;
}

static void
findNextNorthEdge (CompWindow *w,
		   Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int y;

    start = -65535.0f;
    end   =  65535.0f;

    v1 = -65535.0f;
    v2 =  65535.0f;

    y = object->position.y + w->output.top - w->input.top;

    if (y >= w->screen->workArea.y)
    {
	CompWindow *p;

	v1 = w->screen->workArea.y;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (p->invisible || w == p || p->type != CompWindowTypeNormalMask)
		continue;

	    s = p->attrib.x - p->output.left;
	    e = p->attrib.x + p->width + p->output.right;

	    if (s > object->position.x)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.x)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		v = p->attrib.y + p->height + p->input.bottom;
		if (v <= y)
		{
		    if (v > v1)
			v1 = v;
		}
		else
		{
		    if (v < v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = w->screen->workArea.y;
    }

    v1 = v1 - w->output.top + w->input.top;
    v2 = v2 - w->output.top + w->input.top;

    if (v1 != (int) object->horzEdge.next)
	object->horzEdge.snapped = FALSE;

    object->horzEdge.start = start;
    object->horzEdge.end   = end;

    object->horzEdge.next = v1;
    object->horzEdge.prev = v2;

    object->horzEdge.attract  = v1 + EDGE_DISTANCE;
    object->horzEdge.velocity = EDGE_VELOCITY;
}

static void
findNextSouthEdge (CompWindow *w,
		   Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int y;

    start = -65535.0f;
    end   =  65535.0f;

    v1 =  65535.0f;
    v2 = -65535.0f;

    y = object->position.y - w->output.bottom + w->input.bottom;

    if (y <= w->screen->workArea.y + w->screen->workArea.height)
    {
	CompWindow *p;

	v1 = w->screen->workArea.y + w->screen->workArea.height;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (p->invisible || w == p || p->type != CompWindowTypeNormalMask)
		continue;

	    s = p->attrib.x - p->output.left;
	    e = p->attrib.x + p->width + p->output.right;

	    if (s > object->position.x)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.x)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		v = p->attrib.y - p->input.top;
		if (v >= y)
		{
		    if (v < v1)
			v1 = v;
		}
		else
		{
		    if (v > v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = w->screen->workArea.y + w->screen->workArea.height;
    }

    v1 = v1 + w->output.bottom - w->input.bottom;
    v2 = v2 + w->output.bottom - w->input.bottom;

    if (v1 != (int) object->horzEdge.next)
	object->horzEdge.snapped = FALSE;

    object->horzEdge.start = start;
    object->horzEdge.end   = end;

    object->horzEdge.next = v1;
    object->horzEdge.prev = v2;

    object->horzEdge.attract  = v1 - EDGE_DISTANCE;
    object->horzEdge.velocity = EDGE_VELOCITY;
}

static void
objectInit (Object *object,
	    float  positionX,
	    float  positionY,
	    float  velocityX,
	    float  velocityY)
{
    object->force.x = 0;
    object->force.y = 0;

    object->position.x = positionX;
    object->position.y = positionY;

    object->velocity.x = velocityX;
    object->velocity.y = velocityY;

    object->theta    = 0;
    object->immobile = FALSE;

    object->edgeMask = 0;

    object->vertEdge.snapped = FALSE;
    object->horzEdge.snapped = FALSE;

    object->vertEdge.next = 0.0f;
    object->horzEdge.next = 0.0f;
}

static void
springInit (Spring *spring,
	    Object *a,
	    Object *b,
	    float  offsetX,
	    float  offsetY)
{
    spring->a	     = a;
    spring->b	     = b;
    spring->offset.x = offsetX;
    spring->offset.y = offsetY;
}

static void
modelCalcBounds (Model *model)
{
    int i;

    model->topLeft.x	 = MAXSHORT;
    model->topLeft.y	 = MAXSHORT;
    model->bottomRight.x = MINSHORT;
    model->bottomRight.y = MINSHORT;

    for (i = 0; i < model->numObjects; i++)
    {
	if (model->objects[i].position.x < model->topLeft.x)
	    model->topLeft.x = model->objects[i].position.x;
	else if (model->objects[i].position.x > model->bottomRight.x)
	    model->bottomRight.x = model->objects[i].position.x;

	if (model->objects[i].position.y < model->topLeft.y)
	    model->topLeft.y = model->objects[i].position.y;
	else if (model->objects[i].position.y > model->bottomRight.y)
	    model->bottomRight.y = model->objects[i].position.y;
    }
}

static void
modelAddSpring (Model  *model,
		Object *a,
		Object *b,
		float  offsetX,
		float  offsetY)
{
    Spring *spring;

    spring = &model->springs[model->numSprings];
    model->numSprings++;

    springInit (spring, a, b, offsetX, offsetY);
}

static void
modelSetMiddleAnchor (Model *model,
		      int   x,
		      int   y,
		      int   width,
		      int   height)
{
    float gx, gy, x0, y0;

    x0 = model->scaleOrigin.x;
    y0 = model->scaleOrigin.y;

    gx = ((GRID_WIDTH  - 1) / 2 * width)  / (float) (GRID_WIDTH  - 1);
    gy = ((GRID_HEIGHT - 1) / 2 * height) / (float) (GRID_HEIGHT - 1);

    if (model->anchorObject)
	model->anchorObject->immobile = FALSE;

    model->anchorObject = &model->objects[GRID_WIDTH *
					  ((GRID_HEIGHT - 1) / 2) +
					  (GRID_WIDTH - 1) / 2];
    model->anchorObject->position.x = x + (gx - x0) * model->scale.x + x0;
    model->anchorObject->position.y = y + (gy - y0) * model->scale.y + y0;

    model->anchorObject->immobile = TRUE;
}

static void
modelAddEdgeAnchors (Model *model,
		     int   x,
		     int   y,
		     int   width,
		     int   height)
{
    Object *o;
    float  x0, y0;

    x0 = model->scaleOrigin.x;
    y0 = model->scaleOrigin.y;

    o = &model->objects[0];
    o->position.x = x + (0      - x0) * model->scale.x + x0;
    o->position.y = y + (0      - y0) * model->scale.y + y0;
    o->immobile = TRUE;

    o = &model->objects[GRID_WIDTH - 1];
    o->position.x = x + (width  - x0) * model->scale.x + x0;
    o->position.y = y + (0      - y0) * model->scale.y + y0;
    o->immobile = TRUE;

    o = &model->objects[GRID_WIDTH * (GRID_HEIGHT - 1)];
    o->position.x = x + (0      - x0) * model->scale.x + x0;
    o->position.y = y + (height - y0) * model->scale.y + y0;
    o->immobile = TRUE;

    o = &model->objects[model->numObjects - 1];
    o->position.x = x + (width  - x0) * model->scale.x + x0;
    o->position.y = y + (height - y0) * model->scale.y + y0;
    o->immobile = TRUE;

    if (!model->anchorObject)
	model->anchorObject = &model->objects[0];
}

static void
modelRemoveEdgeAnchors (Model *model,
			int   x,
			int   y,
			int   width,
			int   height)
{
    Object *o;
    float  x0, y0;

    x0 = model->scaleOrigin.x;
    y0 = model->scaleOrigin.y;

    o = &model->objects[0];
    o->position.x = x + (0      - x0) * model->scale.x + x0;
    o->position.y = y + (0      - y0) * model->scale.y + y0;
    if (o != model->anchorObject)
	o->immobile = FALSE;

    o = &model->objects[GRID_WIDTH - 1];
    o->position.x = x + (width  - x0) * model->scale.x + x0;
    o->position.y = y + (0      - y0) * model->scale.y + y0;
    if (o != model->anchorObject)
	o->immobile = FALSE;

    o = &model->objects[GRID_WIDTH * (GRID_HEIGHT - 1)];
    o->position.x = x + (0      - x0) * model->scale.x + x0;
    o->position.y = y + (height - y0) * model->scale.y + y0;
    if (o != model->anchorObject)
	o->immobile = FALSE;

    o = &model->objects[model->numObjects - 1];
    o->position.x = x + (width  - x0) * model->scale.x + x0;
    o->position.y = y + (height - y0) * model->scale.y + y0;
    if (o != model->anchorObject)
	o->immobile = FALSE;
}

static void
modelAdjustObjectPosition (Model  *model,
			   Object *object,
			   int    x,
			   int    y,
			   int    width,
			   int    height)
{
    Object *o;
    float  x0, y0;
    int	   gridX, gridY, i = 0;

    x0 = model->scaleOrigin.x;
    y0 = model->scaleOrigin.y;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    o = &model->objects[i];
	    if (o == object)
	    {
		o->position.x = x +
		    (((gridX * width) / (GRID_WIDTH - 1)) - x0) *
		    model->scale.x + x0;
		o->position.y = y +
		    (((gridY * height) / (GRID_HEIGHT - 1)) - y0) *
		    model->scale.y + y0;

		return;
	    }

	    i++;
	}
    }
}

static void
modelInitObjects (Model *model,
		  int	x,
		  int   y,
		  int	width,
		  int	height)
{
    int	  gridX, gridY, i = 0;
    float gw, gh, x0, y0;

    x0 = model->scaleOrigin.x;
    y0 = model->scaleOrigin.y;

    gw = GRID_WIDTH  - 1;
    gh = GRID_HEIGHT - 1;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    objectInit (&model->objects[i],
			x + (((gridX * width)  / gw) - x0) *
			model->scale.x + x0,
			y + (((gridY * height) / gh) - y0) *
			model->scale.y + y0,
			0, 0);
	    i++;
	}
    }

    modelSetMiddleAnchor (model, x, y, width, height);
}

static void
modelUpdateSnapping (CompWindow *window,
		     Model      *model)
{
    unsigned int edgeMask, gridMask, mask;
    int		 gridX, gridY, i = 0;

    edgeMask = model->edgeMask;

    if (model->snapCnt[NORTH])
	edgeMask &= ~SouthEdgeMask;
    else if (model->snapCnt[SOUTH])
	edgeMask &= ~NorthEdgeMask;

    if (model->snapCnt[WEST])
	edgeMask &= ~EastEdgeMask;
    else if (model->snapCnt[EAST])
	edgeMask &= ~WestEdgeMask;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	if (gridY == 0)
	    gridMask = edgeMask & NorthEdgeMask;
	else if (gridY == GRID_HEIGHT - 1)
	    gridMask = edgeMask & SouthEdgeMask;
	else
	    gridMask = 0;

	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    mask = gridMask;

	    if (gridX == 0)
		mask |= edgeMask & WestEdgeMask;
	    else if (gridX == GRID_WIDTH - 1)
		mask |= edgeMask & EastEdgeMask;

	    if (mask != model->objects[i].edgeMask)
	    {
		model->objects[i].edgeMask = mask;

		if (mask & WestEdgeMask)
		{
		    if (!model->objects[i].vertEdge.snapped)
			findNextWestEdge (window, &model->objects[i]);
		}
		else if (mask & EastEdgeMask)
		{
		    if (!model->objects[i].vertEdge.snapped)
			findNextEastEdge (window, &model->objects[i]);
		}
		else
		    model->objects[i].vertEdge.snapped = FALSE;

		if (mask & NorthEdgeMask)
		{
		    if (!model->objects[i].horzEdge.snapped)
			findNextNorthEdge (window, &model->objects[i]);
		}
		else if (mask & SouthEdgeMask)
		{
		    if (!model->objects[i].horzEdge.snapped)
			findNextSouthEdge (window, &model->objects[i]);
		}
		else
		    model->objects[i].horzEdge.snapped = FALSE;
	    }

	    i++;
	}
    }
}

static void
modelReduceEdgeEscapeVelocity (Model *model)
{
    int	gridX, gridY, i = 0;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (model->objects[i].vertEdge.snapped)
		model->objects[i].vertEdge.velocity *= drand48 () * 0.25f;

	    if (model->objects[i].horzEdge.snapped)
		model->objects[i].horzEdge.velocity *= drand48 () * 0.25f;

	    i++;
	}
    }
}

static Bool
modelDisableSnapping (CompWindow *window,
		      Model      *model)
{
    int	 gridX, gridY, i = 0;
    Bool snapped = FALSE;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (model->objects[i].vertEdge.snapped ||
		model->objects[i].horzEdge.snapped)
		snapped = TRUE;

	    model->objects[i].vertEdge.snapped = FALSE;
	    model->objects[i].horzEdge.snapped = FALSE;

	    model->objects[i].edgeMask = 0;

	    i++;
	}
    }

    memset (model->snapCnt, 0, sizeof (model->snapCnt));

    return snapped;
}

static void
modelAdjustObjectsForShiver (Model *model,
			     int   x,
			     int   y,
			     int   width,
			     int   height)
{
    int   gridX, gridY, i = 0;
    float vX, vY;
    float scale;
    float w, h;

    w = (float) width  * model->scale.x;
    h = (float) height * model->scale.y;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (!model->objects[i].immobile)
	    {
		vX = model->objects[i].position.x - (x + w / 2);
		vY = model->objects[i].position.y - (y + h / 2);

		vX /= w;
		vY /= h;

		scale = ((float) rand () * 7.5f) / RAND_MAX;

		model->objects[i].velocity.x += vX * scale;
		model->objects[i].velocity.y += vY * scale;
	    }

	    i++;
	}
    }
}

static void
modelInitSprings (Model *model,
		  int   x,
		  int   y,
		  int   width,
		  int   height)
{
    int   gridX, gridY, i = 0;
    float hpad, vpad;

    model->numSprings = 0;

    hpad = ((float) width  * model->scale.x) / (GRID_WIDTH  - 1);
    vpad = ((float) height * model->scale.y) / (GRID_HEIGHT - 1);

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (gridX > 0)
		modelAddSpring (model,
				&model->objects[i - 1],
				&model->objects[i],
				hpad, 0);

	    if (gridY > 0)
		modelAddSpring (model,
				&model->objects[i - GRID_WIDTH],
				&model->objects[i],
				0, vpad);

	    i++;
	}
    }
}

static void
modelMove (Model *model,
	   float tx,
	   float ty)
{
    int i;

    for (i = 0; i < model->numObjects; i++)
    {
	model->objects[i].position.x += tx;
	model->objects[i].position.y += ty;
    }
}

static Model *
createModel (int	  x,
	     int	  y,
	     int	  width,
	     int	  height,
	     unsigned int edgeMask)
{
    Model *model;

    model = malloc (sizeof (Model));
    if (!model)
	return 0;

    model->numObjects = GRID_WIDTH * GRID_HEIGHT;
    model->objects = malloc (sizeof (Object) * model->numObjects);
    if (!model->objects)
	return 0;

    model->anchorObject = 0;
    model->numSprings = 0;

    model->steps = 0;

    model->scale.x = 1.0f;
    model->scale.y = 1.0f;

    model->scaleOrigin.x = 0.0f;
    model->scaleOrigin.y = 0.0f;

    model->transformed = FALSE;

    memset (model->snapCnt, 0, sizeof (model->snapCnt));

    model->edgeMask = edgeMask;

    modelInitObjects (model, x, y, width, height);
    modelInitSprings (model, x, y, width, height);

    modelCalcBounds (model);

    return model;
}

static void
objectApplyForce (Object *object,
		  float  fx,
		  float  fy)
{
    object->force.x += fx;
    object->force.y += fy;
}

static void
springExertForces (Spring *spring,
		   float  k)
{
    Vector da, db;
    Vector a, b;

    a = spring->a->position;
    b = spring->b->position;

    da.x = 0.5f * (b.x - a.x - spring->offset.x);
    da.y = 0.5f * (b.y - a.y - spring->offset.y);

    db.x = 0.5f * (a.x - b.x + spring->offset.x);
    db.y = 0.5f * (a.y - b.y + spring->offset.y);

    objectApplyForce (spring->a, k * da.x, k * da.y);
    objectApplyForce (spring->b, k * db.x, k * db.y);
}

static Bool
objectReleaseWestEdge (CompWindow *w,
		       Model	  *model,
		       Object	  *object)
{
    if (fabs (object->velocity.x) > object->vertEdge.velocity)
    {
	object->position.x += object->velocity.x * 2.0f;

	model->snapCnt[WEST]--;

	object->vertEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.x = 0.0f;

    return FALSE;
}

static Bool
objectReleaseEastEdge (CompWindow *w,
		       Model	  *model,
		       Object	  *object)
{
    if (fabs (object->velocity.x) > object->vertEdge.velocity)
    {
	object->position.x += object->velocity.x * 2.0f;

	model->snapCnt[EAST]--;

	object->vertEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.x = 0.0f;

    return FALSE;
}

static Bool
objectReleaseNorthEdge (CompWindow *w,
			Model	   *model,
			Object	   *object)
{
    if (fabs (object->velocity.y) > object->horzEdge.velocity)
    {
	object->position.y += object->velocity.y * 2.0f;

	model->snapCnt[NORTH]--;

	object->horzEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.y = 0.0f;

    return FALSE;
}

static Bool
objectReleaseSouthEdge (CompWindow *w,
			Model	   *model,
			Object	   *object)
{
    if (fabs (object->velocity.y) > object->horzEdge.velocity)
    {
	object->position.y += object->velocity.y * 2.0f;

	model->snapCnt[SOUTH]--;

	object->horzEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.y = 0.0f;

    return FALSE;
}

static float
modelStepObject (CompWindow *window,
		 Model	    *model,
		 Object	    *object,
		 float	    friction,
		 float	    *force)
{
    object->theta += 0.05f;

    if (object->immobile)
    {
	object->velocity.x = 0.0f;
	object->velocity.y = 0.0f;

	object->force.x = 0.0f;
	object->force.y = 0.0f;

	*force = 0.0f;

	return 0.0f;
    }
    else
    {
	object->force.x -= friction * object->velocity.x;
	object->force.y -= friction * object->velocity.y;

	object->velocity.x += object->force.x / MASS;
	object->velocity.y += object->force.y / MASS;

	if (object->edgeMask)
	{
	    if (object->edgeMask & WestEdgeMask)
	    {
		if (object->position.y < object->vertEdge.start ||
		    object->position.y > object->vertEdge.end)
		    findNextWestEdge (window, object);

		if (object->vertEdge.snapped == FALSE ||
		    objectReleaseWestEdge (window, model, object))
		{
		    object->position.x += object->velocity.x;

		    if (object->velocity.x < 0.0f &&
			object->position.x < object->vertEdge.attract)
		    {
			if (object->position.x < object->vertEdge.next)
			{
			    object->vertEdge.snapped = TRUE;
			    object->position.x = object->vertEdge.next;
			    object->velocity.x = 0.0f;

			    model->snapCnt[WEST]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.x -=
				object->vertEdge.attract - object->position.x;
			}
		    }

		    if (object->position.x > object->vertEdge.prev)
			findNextWestEdge (window, object);
		}
	    }
	    else if (object->edgeMask & EastEdgeMask)
	    {
		if (object->position.y < object->vertEdge.start ||
		    object->position.y > object->vertEdge.end)
		    findNextEastEdge (window, object);

		if (object->vertEdge.snapped == FALSE ||
		    objectReleaseEastEdge (window, model, object))
		{
		    object->position.x += object->velocity.x;

		    if (object->velocity.x > 0.0f &&
			object->position.x > object->vertEdge.attract)
		    {
			if (object->position.x > object->vertEdge.next)
			{
			    object->vertEdge.snapped = TRUE;
			    object->position.x = object->vertEdge.next;
			    object->velocity.x = 0.0f;

			    model->snapCnt[EAST]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.x =
				object->position.x - object->vertEdge.attract;
			}
		    }

		    if (object->position.x < object->vertEdge.prev)
			findNextEastEdge (window, object);
		}
	    }
	    else
		object->position.x += object->velocity.x;

	    if (object->edgeMask & NorthEdgeMask)
	    {
		if (object->position.x < object->horzEdge.start ||
		    object->position.x > object->horzEdge.end)
		    findNextNorthEdge (window, object);

		if (object->horzEdge.snapped == FALSE ||
		    objectReleaseNorthEdge (window, model, object))
		{
		    object->position.y += object->velocity.y;

		    if (object->velocity.y < 0.0f &&
			object->position.y < object->horzEdge.attract)
		    {
			if (object->position.y < object->horzEdge.next)
			{
			    object->horzEdge.snapped = TRUE;
			    object->position.y = object->horzEdge.next;
			    object->velocity.y = 0.0f;

			    model->snapCnt[NORTH]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.y -=
				object->horzEdge.attract - object->position.y;
			}
		    }

		    if (object->position.y > object->horzEdge.prev)
			findNextNorthEdge (window, object);
		}
	    }
	    else if (object->edgeMask & SouthEdgeMask)
	    {
		if (object->position.x < object->horzEdge.start ||
		    object->position.x > object->horzEdge.end)
		    findNextSouthEdge (window, object);

		if (object->horzEdge.snapped == FALSE ||
		    objectReleaseSouthEdge (window, model, object))
		{
		    object->position.y += object->velocity.y;

		    if (object->velocity.y > 0.0f &&
			object->position.y > object->horzEdge.attract)
		    {
			if (object->position.y > object->horzEdge.next)
			{
			    object->horzEdge.snapped = TRUE;
			    object->position.y = object->horzEdge.next;
			    object->velocity.y = 0.0f;

			    model->snapCnt[SOUTH]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.y =
				object->position.y - object->horzEdge.attract;
			}
		    }

		    if (object->position.y < object->horzEdge.prev)
			findNextSouthEdge (window, object);
		}
	    }
	    else
		object->position.y += object->velocity.y;
	}
	else
	{
	    object->position.x += object->velocity.x;
	    object->position.y += object->velocity.y;
	}

	*force = fabs (object->force.x) + fabs (object->force.y);

	object->force.x = 0.0f;
	object->force.y = 0.0f;

	return fabs (object->velocity.x) + fabs (object->velocity.y);
    }
}

static int
modelStep (CompWindow *window,
	   Model      *model,
	   float      friction,
	   float      k,
	   float      time)
{
    int   i, j, steps, wobbly = 0;
    float velocitySum = 0.0f;
    float force, forceSum = 0.0f;

    model->steps += time / 15.0f;
    steps = floor (model->steps);
    model->steps -= steps;

    if (!steps)
	return TRUE;

    for (j = 0; j < steps; j++)
    {
	for (i = 0; i < model->numSprings; i++)
	    springExertForces (&model->springs[i], k);

	for (i = 0; i < model->numObjects; i++)
	{
	    velocitySum += modelStepObject (window,
					    model,
					    &model->objects[i],
					    friction,
					    &force);
	    forceSum += force;
	}
    }

    modelCalcBounds (model);

    if (velocitySum > 0.5f)
	wobbly |= WobblyVelocity;

    if (forceSum > 20.0f)
	wobbly |= WobblyForce;

    return wobbly;
}

static void
bezierPatchEvaluate (Model *model,
		     float u,
		     float v,
		     float *patchX,
		     float *patchY)
{
    float coeffsU[4], coeffsV[4];
    float x, y;
    int   i, j;

    coeffsU[0] = (1 - u) * (1 - u) * (1 - u);
    coeffsU[1] = 3 * u * (1 - u) * (1 - u);
    coeffsU[2] = 3 * u * u * (1 - u);
    coeffsU[3] = u * u * u;

    coeffsV[0] = (1 - v) * (1 - v) * (1 - v);
    coeffsV[1] = 3 * v * (1 - v) * (1 - v);
    coeffsV[2] = 3 * v * v * (1 - v);
    coeffsV[3] = v * v * v;

    x = y = 0.0f;

    for (i = 0; i < 4; i++)
    {
	for (j = 0; j < 4; j++)
	{
	    x += coeffsU[i] * coeffsV[j] *
		model->objects[j * GRID_WIDTH + i].position.x;
	    y += coeffsU[i] * coeffsV[j] *
		model->objects[j * GRID_WIDTH + i].position.y;
	}
    }

    *patchX = x;
    *patchY = y;
}

static Bool
wobblyEnsureModel (CompWindow *w)
{
    WOBBLY_WINDOW (w);

    if (!ww->model)
    {
	unsigned int edgeMask = 0;

	if (w->type & CompWindowTypeNormalMask)
	    edgeMask = WestEdgeMask | EastEdgeMask | NorthEdgeMask |
		SouthEdgeMask;

	ww->model = createModel (WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w),
				 edgeMask);
	if (!ww->model)
	    return FALSE;
    }

    return TRUE;
}

static float
objectDistance (Object *object,
		float  x,
		float  y)
{
    float dx, dy;

    dx = object->position.x - x;
    dy = object->position.y - y;

    return sqrt (dx * dx + dy * dy);
}

static Object *
modelFindNearestObject (Model *model,
			float x,
			float y)
{
    Object *object = &model->objects[0];
    float  distance, minDistance = 0.0;
    int    i;

    for (i = 0; i < model->numObjects; i++)
    {
	distance = objectDistance (&model->objects[i], x, y);
	if (i == 0 || distance < minDistance)
	{
	    minDistance = distance;
	    object = &model->objects[i];
	}
    }

    return object;
}

static Bool
isWobblyWin (CompWindow *w)
{
    WOBBLY_WINDOW (w);

    if (ww->model)
	return TRUE;

    /* avoid tiny windows */
    if (w->width == 1 && w->height == 1)
	return FALSE;

    /* avoid fullscreen windows */
    if (w->attrib.x <= 0 &&
	w->attrib.y <= 0 &&
	w->attrib.x + w->width >= w->screen->width &&
	w->attrib.y + w->height >= w->screen->height)
	return FALSE;

    return TRUE;
}

static void
wobblyPreparePaintScreen (CompScreen *s,
			  int	     msSinceLastPaint)
{
    WobblyWindow *ww;
    CompWindow   *w;

    WOBBLY_SCREEN (s);

    if (ws->wobblyWindows & (WobblyInitial | WobblyVelocity))
    {
	REGION region;
	Point  topLeft, bottomRight;
	float  friction, springK;
	Model  *model;

	friction = ws->opt[WOBBLY_SCREEN_OPTION_FRICTION].value.f;
	springK  = ws->opt[WOBBLY_SCREEN_OPTION_SPRING_K].value.f;

	region.rects = &region.extents;
	region.numRects = region.size = 1;

	ws->wobblyWindows = 0;
	for (w = s->windows; w; w = w->next)
	{
	    ww = GET_WOBBLY_WINDOW (w, ws);

	    if (ww->wobbly)
	    {
		if (ww->wobbly & (WobblyInitial | WobblyVelocity))
		{
		    model = ww->model;

		    topLeft     = model->topLeft;
		    bottomRight = model->bottomRight;

		    ww->wobbly = modelStep (w, model, friction, springK,
					    (ww->wobbly & WobblyVelocity) ?
					    msSinceLastPaint :
					    s->redrawTime);

		    if ((ww->state & MAXIMIZE_STATE) && ww->grabbed)
			ww->wobbly |= WobblyForce;

		    if (ww->wobbly)
		    {
			/* snapped to more than one edge, we have to reduce
			   edge escape velocity until only one edge is snapped */
			if (ww->wobbly == WobblyForce && !ww->grabbed)
			{
			    modelReduceEdgeEscapeVelocity (ww->model);
			    ww->wobbly |= WobblyInitial;
			}
		    }
		    else if (!ww->model->transformed)
		    {
			ww->model = 0;

			moveWindow (w,
				    model->topLeft.x + w->output.left -
				    w->attrib.x,
				    model->topLeft.y + w->output.top -
				    w->attrib.y,
				    TRUE, TRUE);

			ww->model = model;

			syncWindowPosition (w);
		    }

		    if (!(s->damageMask & COMP_SCREEN_DAMAGE_ALL_MASK))
		    {
			if (ww->wobbly)
			{
			    if (ww->model->topLeft.x < topLeft.x)
				topLeft.x = ww->model->topLeft.x;
			    if (ww->model->topLeft.y < topLeft.y)
				topLeft.y = ww->model->topLeft.y;
			    if (ww->model->bottomRight.x > bottomRight.x)
				bottomRight.x = ww->model->bottomRight.x;
			    if (ww->model->bottomRight.y > bottomRight.y)
				bottomRight.y = ww->model->bottomRight.y;
			}
			else
			    addWindowDamage (w);

			region.extents.x1 = topLeft.x;
			region.extents.y1 = topLeft.y;
			region.extents.x2 = bottomRight.x + 0.5f;
			region.extents.y2 = bottomRight.y + 0.5f;

			damageScreenRegion (s, &region);
		    }
		}

		ws->wobblyWindows |= ww->wobbly;
	    }
	}
    }

    UNWRAP (ws, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ws, s, preparePaintScreen, wobblyPreparePaintScreen);
}

static void
wobblyDonePaintScreen (CompScreen *s)
{
    WOBBLY_SCREEN (s);

    if (ws->wobblyWindows & (WobblyVelocity | WobblyInitial))
	damagePendingOnScreen (s);

    UNWRAP (ws, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ws, s, donePaintScreen, wobblyDonePaintScreen);
}

static void
wobblyAddWindowGeometry (CompWindow *w,
			 CompMatrix *matrix,
			 int	    nMatrix,
			 Region     region,
			 Region     clip)
{
    WOBBLY_WINDOW (w);
    WOBBLY_SCREEN (w->screen);

    if (ww->wobbly)
    {
	BoxPtr   pClip;
	int      nClip, nVertices, nIndices;
	GLushort *i;
	GLfloat  *v;
	int      x1, y1, x2, y2;
	float    width, height;
	float    deformedX, deformedY;
	int      x, y, iw, ih, wx, wy;
	int      vSize, it;
	int      gridW, gridH;
	Bool     rect = TRUE;

	for (it = 0; it < nMatrix; it++)
	{
	    if (matrix[it].xy != 0.0f && matrix[it].yx != 0.0f)
	    {
		rect = FALSE;
		break;
	    }
	}

	wx     = WIN_X (w);
	wy     = WIN_Y (w);
	width  = WIN_W (w);
	height = WIN_H (w);

	gridW = width / ws->opt[WOBBLY_SCREEN_OPTION_GRID_RESOLUTION].value.i;
	if (gridW < ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i)
	    gridW = ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i;

	gridH = height / ws->opt[WOBBLY_SCREEN_OPTION_GRID_RESOLUTION].value.i;
	if (gridH < ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i)
	    gridH = ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i;

	nClip = region->numRects;
	pClip = region->rects;

	w->texUnits = nMatrix;

	vSize = 2 + nMatrix * 2;

	nVertices = w->vCount;
	nIndices  = w->vCount;

	v = w->vertices + (nVertices * vSize);
	i = w->indices  + nIndices;

	while (nClip--)
	{
	    x1 = pClip->x1;
	    y1 = pClip->y1;
	    x2 = pClip->x2;
	    y2 = pClip->y2;

	    iw = ((x2 - x1 - 1) / gridW) + 1;
	    ih = ((y2 - y1 - 1) / gridH) + 1;

	    if (nIndices + (iw * ih * 4) > w->indexSize)
	    {
		if (!moreWindowIndices (w, nIndices + (iw * ih * 4)))
		    return;

		i = w->indices + nIndices;
	    }

	    iw++;
	    ih++;

	    for (y = 0; y < ih - 1; y++)
	    {
		for (x = 0; x < iw - 1; x++)
		{
		    *i++ = nVertices + iw * (y + 1) + x;
		    *i++ = nVertices + iw * (y + 1) + x + 1;
		    *i++ = nVertices + iw * y + x + 1;
		    *i++ = nVertices + iw * y + x;

		    nIndices += 4;
		}
	    }

	    if (((nVertices + iw * ih) * vSize) > w->vertexSize)
	    {
		if (!moreWindowVertices (w, (nVertices + iw * ih) * vSize))
		    return;

		v = w->vertices + (nVertices * vSize);
	    }

	    for (y = y1;; y += gridH)
	    {
		if (y > y2)
		    y = y2;

		for (x = x1;; x += gridW)
		{
		    if (x > x2)
			x = x2;

		    bezierPatchEvaluate (ww->model,
					 (x - wx) / width,
					 (y - wy) / height,
					 &deformedX,
					 &deformedY);

		    if (rect)
		    {
			for (it = 0; it < nMatrix; it++)
			{
			    *v++ = COMP_TEX_COORD_X (&matrix[it], x);
			    *v++ = COMP_TEX_COORD_Y (&matrix[it], y);
			}
		    }
		    else
		    {
			for (it = 0; it < nMatrix; it++)
			{
			    *v++ = COMP_TEX_COORD_XY (&matrix[it], x, y);
			    *v++ = COMP_TEX_COORD_YX (&matrix[it], x, y);
			}
		    }

		    *v++ = deformedX;
		    *v++ = deformedY;

		    nVertices++;

		    if (x == x2)
			break;
		}

		if (y == y2)
		    break;
	    }

	    pClip++;
	}

	w->vCount = nIndices;
    }
    else
    {
	UNWRAP (ws, w->screen, addWindowGeometry);
	(*w->screen->addWindowGeometry) (w, matrix, nMatrix, region, clip);
	WRAP (ws, w->screen, addWindowGeometry, wobblyAddWindowGeometry);
    }
}

static void
wobblyDrawWindowGeometry (CompWindow *w)
{
    WOBBLY_WINDOW (w);

    if (ww->wobbly)
    {
	int     texUnit = w->texUnits;
	int     currentTexUnit = 0;
	int     stride = (1 + texUnit) * 2;
	GLfloat *vertices = w->vertices + (stride - 2);

	stride *= sizeof (GLfloat);

	glVertexPointer (2, GL_FLOAT, stride, vertices);

	while (texUnit--)
	{
	    if (texUnit != currentTexUnit)
	    {
		w->screen->clientActiveTexture (GL_TEXTURE0_ARB + texUnit);
		glEnableClientState (GL_TEXTURE_COORD_ARRAY);
		currentTexUnit = texUnit;
	    }
	    vertices -= 2;
	    glTexCoordPointer (2, GL_FLOAT, stride, vertices);
	}

	glDrawElements (GL_QUADS, w->vCount, GL_UNSIGNED_SHORT, w->indices);

	/* disable all texture coordinate arrays except 0 */
	texUnit = w->texUnits;
	if (texUnit > 1)
	{
	    while (--texUnit)
	    {
		(*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB + texUnit);
		glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	    }

	    (*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB);
	}
    }
    else
    {
	WOBBLY_SCREEN (w->screen);

	UNWRAP (ws, w->screen, drawWindowGeometry);
	(*w->screen->drawWindowGeometry) (w);
	WRAP (ws, w->screen, drawWindowGeometry, wobblyDrawWindowGeometry);
    }
}

static Bool
wobblyPaintWindow (CompWindow		   *w,
		   const WindowPaintAttrib *attrib,
		   Region		   region,
		   unsigned int		   mask)
{
    Bool status;

    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    if (ww->wobbly)
    {
	WindowPaintAttrib wAttrib = *attrib;

	if (mask & PAINT_WINDOW_SOLID_MASK)
	    return FALSE;

	mask |= PAINT_WINDOW_TRANSFORMED_MASK;

	wAttrib.xScale = 1.0f;
	wAttrib.yScale = 1.0f;

	UNWRAP (ws, w->screen, paintWindow);
	status = (*w->screen->paintWindow) (w, &wAttrib, region, mask);
	WRAP (ws, w->screen, paintWindow, wobblyPaintWindow);
    }
    else
    {
	UNWRAP (ws, w->screen, paintWindow);
	status = (*w->screen->paintWindow) (w, attrib, region, mask);
	WRAP (ws, w->screen, paintWindow, wobblyPaintWindow);
    }

    return status;
}

static Bool
wobblyEnableSnapping (CompDisplay     *d,
		      CompAction      *action,
		      CompActionState state,
		      CompOption      *option,
		      int	      nOption)
{
    CompScreen *s;
    CompWindow *w;

    WOBBLY_DISPLAY (d);

    for (s = d->screens; s; s = s->next)
    {
	for (w = s->windows; w; w = w->next)
	{
	    WOBBLY_WINDOW (w);

	    if (ww->grabbed && ww->model)
		modelUpdateSnapping (w, ww->model);
	}
    }

    wd->snapping = TRUE;

    return FALSE;
}

static Bool
wobblyDisableSnapping (CompDisplay     *d,
		       CompAction      *action,
		       CompActionState state,
		       CompOption      *option,
		       int	       nOption)
{
    CompScreen *s;
    CompWindow *w;

    WOBBLY_DISPLAY (d);

    if (!wd->snapping)
	return FALSE;

    for (s = d->screens; s; s = s->next)
    {
	for (w = s->windows; w; w = w->next)
	{
	    WOBBLY_WINDOW (w);

	    if (ww->grabbed && ww->model)
	    {
		if (modelDisableSnapping (w, ww->model))
		{
		    WOBBLY_SCREEN (w->screen);

		    ww->wobbly |= WobblyInitial;
		    ws->wobblyWindows |= ww->wobbly;

		    damagePendingOnScreen (w->screen);
		}
	    }
	}
    }

    wd->snapping = FALSE;

    return FALSE;
}

static Bool
wobblyShiver (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int	      nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findWindowAtDisplay (d, xid);
    if (w && isWobblyWin (w) && wobblyEnsureModel (w))
    {
	WOBBLY_SCREEN (w->screen);
	WOBBLY_WINDOW (w);

	modelSetMiddleAnchor (ww->model,
			      WIN_X (w), WIN_Y (w),
			      WIN_W (w), WIN_H (w));
	modelAdjustObjectsForShiver (ww->model,
				     WIN_X (w), WIN_Y (w),
				     WIN_W (w), WIN_H (w));

	ww->wobbly |= WobblyInitial;
	ws->wobblyWindows |= ww->wobbly;

	damagePendingOnScreen (w->screen);
    }

    return FALSE;
}

static void
wobblyHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    Window     activeWindow = 0;
    CompWindow *w;
    CompScreen *s;

    WOBBLY_DISPLAY (d);

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == d->winActiveAtom)
	    activeWindow = d->activeWindow;
	break;
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    WOBBLY_WINDOW (w);

	    if (ww->model)
	    {
		modelInitObjects (ww->model,
				  WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));

		modelInitSprings (ww->model,
				  WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));
	    }
	}
	break;
    default:
	if (event->type == d->xkbEvent)
	{
	    XkbAnyEvent *xkbEvent = (XkbAnyEvent *) event;

	    if (xkbEvent->xkb_type == XkbStateNotify)
	    {
		XkbStateNotifyEvent *stateEvent = (XkbStateNotifyEvent *) event;
		CompAction	    *action;
		unsigned int	    mods = 0xffffffff;

		action = &wd->opt[WOBBLY_DISPLAY_OPTION_SNAP].value.action;

		if (action->type & CompBindingTypeKey)
		    mods = action->key.modifiers;

		if ((stateEvent->mods & mods) == mods)
		    wobblyEnableSnapping (d, NULL, 0, NULL, 0);
		else
		    wobblyDisableSnapping (d, NULL, 0, NULL, 0);
	    }
	}
	break;
    }

    UNWRAP (wd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (wd, d, handleEvent, wobblyHandleEvent);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	{
	    WOBBLY_SCREEN (s);

	    if (ws->grabWindow			       &&
		(ws->moveWMask & ws->grabWindow->type) &&
		ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
	    {
		WOBBLY_WINDOW (ws->grabWindow);

		if (ww->state & MAXIMIZE_STATE)
		{
		    WOBBLY_WINDOW (ws->grabWindow);

		    if (ww->model && ww->grabbed)
		    {
			int dx, dy;

			if (ww->state & CompWindowStateMaximizedHorzMask)
			    dx = pointerX - lastPointerX;
			else
			    dx = 0;

			if (ww->state & CompWindowStateMaximizedVertMask)
			    dy = pointerY - lastPointerY;
			else
			    dy = 0;

			ww->model->anchorObject->position.x += dx;
			ww->model->anchorObject->position.y += dy;

			ww->wobbly |= WobblyInitial;
			ws->wobblyWindows |= ww->wobbly;

			damagePendingOnScreen (s);
		    }
		}
	    }
	}
	break;
    case PropertyNotify:
	if (event->xproperty.atom == d->winActiveAtom)
	{
	    if (d->activeWindow != activeWindow)
	    {
		w = findWindowAtDisplay (d, d->activeWindow);
		if (w && isWobblyWin (w))
		{
		    WOBBLY_WINDOW (w);
		    WOBBLY_SCREEN (w->screen);

		    if ((ws->focusWMask & w->type) &&
			ws->focusEffect		   &&
			wobblyEnsureModel (w))
		    {
			switch (ws->focusEffect) {
			case WobblyEffectShiver:
			    modelAdjustObjectsForShiver (ww->model,
							 WIN_X (w),
							 WIN_Y (w),
							 WIN_W (w),
							 WIN_H (w));
			default:
			    break;
			}

			ww->wobbly |= WobblyInitial;
			ws->wobblyWindows |= ww->wobbly;

			damagePendingOnScreen (w->screen);
		    }
		}
	    }
	}
    default:
	break;
    }
}

static Bool
wobblyDamageWindowRect (CompWindow *w,
			Bool	   initial,
			BoxPtr     rect)
{
    Bool status;

    WOBBLY_SCREEN (w->screen);

    if (!initial)
    {
	WOBBLY_WINDOW (w);

	if (ww->wobbly == WobblyForce)
	{
	    REGION region;

	    region.rects = &region.extents;
	    region.numRects = region.size = 1;

	    region.extents.x1 = ww->model->topLeft.x;
	    region.extents.y1 = ww->model->topLeft.y;
	    region.extents.x2 = ww->model->bottomRight.x + 0.5f;
	    region.extents.y2 = ww->model->bottomRight.y + 0.5f;

	    damageScreenRegion (w->screen, &region);

	    return TRUE;
	}
    }

    UNWRAP (ws, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ws, w->screen, damageWindowRect, wobblyDamageWindowRect);

    if (initial)
    {
	if (isWobblyWin (w))
	{
	    WOBBLY_WINDOW (w);
	    WOBBLY_SCREEN (w->screen);

	    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
		wobblyEnsureModel (w);

	    if ((ws->mapWMask & w->type) &&
		ws->mapEffect		 &&
		wobblyEnsureModel (w))
	    {
		switch (ws->mapEffect) {
		case WobblyEffectShiver:
		    modelAdjustObjectsForShiver (ww->model,
						 WIN_X (w), WIN_Y (w),
						 WIN_W (w), WIN_H (w));
		default:
		    break;
		}

		ww->wobbly |= WobblyInitial;
		ws->wobblyWindows |= ww->wobbly;

		damagePendingOnScreen (w->screen);
	    }
	}
    }

    return status;
}

static void
wobblySetWindowScale (CompWindow *w,
		      float      xScale,
		      float      yScale)
{
    WOBBLY_WINDOW (w);
    WOBBLY_SCREEN (w->screen);

    UNWRAP (ws, w->screen, setWindowScale);
    (*w->screen->setWindowScale) (w, xScale, yScale);
    WRAP (ws, w->screen, setWindowScale, wobblySetWindowScale);

    if (wobblyEnsureModel (w))
    {
	if (ww->model->scale.x != xScale ||
	    ww->model->scale.y != yScale)
	{
	    ww->model->scale.x = xScale;
	    ww->model->scale.y = yScale;

	    ww->model->scaleOrigin.x = w->output.left;
	    ww->model->scaleOrigin.y = w->output.top;

	    modelInitObjects (ww->model,
			      WIN_X (w), WIN_Y (w),
			      WIN_W (w), WIN_H (w));

	    modelInitSprings (ww->model,
			      WIN_X (w), WIN_Y (w),
			      WIN_W (w), WIN_H (w));
	}

	if (ww->model->scale.x != 1.0f || ww->model->scale.y != 1.0f)
	    ww->model->transformed = 1;
	else
	    ww->model->transformed = 0;
    }
}

static void
wobblyWindowResizeNotify (CompWindow *w)
{
    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b &&
	isWobblyWin (w)					      &&
	((w->state ^ ww->state) & MAXIMIZE_STATE))
    {
	if (wobblyEnsureModel (w))
	{
	    if (w->state & MAXIMIZE_STATE)
	    {
		if (!ww->grabbed && ww->model->anchorObject)
		{
		    ww->model->anchorObject->immobile = FALSE;
		    ww->model->anchorObject = NULL;
		}

		modelAddEdgeAnchors (ww->model,
				     WIN_X (w), WIN_Y (w),
				     WIN_W (w), WIN_H (w));
	    }
	    else
	    {
		modelRemoveEdgeAnchors (ww->model,
					WIN_X (w), WIN_Y (w),
					WIN_W (w), WIN_H (w));
		modelSetMiddleAnchor (ww->model,
				      WIN_X (w), WIN_Y (w),
				      WIN_W (w), WIN_H (w));
	    }

	    modelInitSprings (ww->model,
			      WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));

	    ww->wobbly |= WobblyInitial;
	    ws->wobblyWindows |= ww->wobbly;

	    damagePendingOnScreen (w->screen);
	}
    }
    else if (ww->model)
    {
	modelInitObjects (ww->model,
			  WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));

	modelInitSprings (ww->model,
			  WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));
    }

    /* update grab */
    if (ww->model && ww->grabbed)
    {
	if (ww->model->anchorObject)
	    ww->model->anchorObject->immobile = FALSE;

	ww->model->anchorObject = modelFindNearestObject (ww->model,
							  pointerX,
							  pointerY);
	ww->model->anchorObject->immobile = TRUE;

	modelAdjustObjectPosition (ww->model,
				   ww->model->anchorObject,
				   WIN_X (w), WIN_Y (w),
				   WIN_W (w), WIN_H (w));
    }

    ww->state = w->state;

    UNWRAP (ws, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w);
    WRAP (ws, w->screen, windowResizeNotify, wobblyWindowResizeNotify);
}

static void
wobblyWindowMoveNotify (CompWindow *w,
			int	   dx,
			int	   dy,
			Bool	   immediate)
{
    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    if (ww->model)
    {
	if (ww->grabbed && !immediate)
	{
	    if (ww->state & MAXIMIZE_STATE)
	    {
		int i;

		for (i = 0; i < ww->model->numObjects; i++)
		{
		    if (ww->model->objects[i].immobile)
		    {
			ww->model->objects[i].position.x += dx;
			ww->model->objects[i].position.y += dy;
		    }
		}
	    }
	    else
	    {
		ww->model->anchorObject->position.x += dx;
		ww->model->anchorObject->position.y += dy;
	    }

	    ww->wobbly |= WobblyInitial;
	    ws->wobblyWindows |= ww->wobbly;

	    damagePendingOnScreen (w->screen);
	}
	else
	    modelMove (ww->model, dx, dy);
    }

    UNWRAP (ws, w->screen, windowMoveNotify);
    (*w->screen->windowMoveNotify) (w, dx, dy, immediate);
    WRAP (ws, w->screen, windowMoveNotify, wobblyWindowMoveNotify);
}

static void
wobblyWindowGrabNotify (CompWindow   *w,
			int	     x,
			int	     y,
			unsigned int state,
			unsigned int mask)
{
    WOBBLY_SCREEN (w->screen);

    ws->grabMask   = mask;
    ws->grabWindow = w;

    if ((mask & CompWindowGrabButtonMask) &&
	(ws->moveWMask & w->type)         &&
	isWobblyWin (w))
    {
	WOBBLY_WINDOW (w);

	if (wobblyEnsureModel (w))
	{
	    Spring *s;
	    int	   i;

	    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
	    {
		if (w->state & MAXIMIZE_STATE)
		{
		    modelAddEdgeAnchors (ww->model,
					 WIN_X (w), WIN_Y (w),
					 WIN_W (w), WIN_H (w));
		}
		else
		{
		    modelRemoveEdgeAnchors (ww->model,
					    WIN_X (w), WIN_Y (w),
					    WIN_W (w), WIN_H (w));

		    if (ww->model->anchorObject)
			ww->model->anchorObject->immobile = FALSE;
		}
	    }
	    else
	    {
		if (ww->model->anchorObject)
		    ww->model->anchorObject->immobile = FALSE;
	    }

	    ww->model->anchorObject = modelFindNearestObject (ww->model, x, y);
	    ww->model->anchorObject->immobile = TRUE;

	    ww->grabbed = TRUE;

	    if (mask & CompWindowGrabMoveMask)
	    {
		WOBBLY_DISPLAY (w->screen->display);

		modelDisableSnapping (w, ww->model);
		if (wd->snapping)
		    modelUpdateSnapping (w, ww->model);
	    }

	    if (ws->grabWMask & w->type)
	    {
		for (i = 0; i < ww->model->numSprings; i++)
		{
		    s = &ww->model->springs[i];

		    if (s->a == ww->model->anchorObject)
		    {
			s->b->velocity.x -= s->offset.x * 0.05f;
			s->b->velocity.y -= s->offset.y * 0.05f;
		    }
		    else if (s->b == ww->model->anchorObject)
		    {
			s->a->velocity.x += s->offset.x * 0.05f;
			s->a->velocity.y += s->offset.y * 0.05f;
		    }
		}

		ww->wobbly |= WobblyInitial;
		ws->wobblyWindows |= ww->wobbly;

		damagePendingOnScreen (w->screen);
	    }
	}
    }

    UNWRAP (ws, w->screen, windowGrabNotify);
    (*w->screen->windowGrabNotify) (w, x, y, state, mask);
    WRAP (ws, w->screen, windowGrabNotify, wobblyWindowGrabNotify);
}

static void
wobblyWindowUngrabNotify (CompWindow *w)
{
    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    ws->grabMask   = 0;
    ws->grabWindow = NULL;

    if (ww->grabbed)
    {
	if (ww->model)
	{
	    if (ww->model->anchorObject)
		ww->model->anchorObject->immobile = FALSE;

	    ww->model->anchorObject = NULL;

	    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
	    {
		if (ww->state & MAXIMIZE_STATE)
		    modelAddEdgeAnchors (ww->model,
					 WIN_X (w), WIN_Y (w),
					 WIN_W (w), WIN_H (w));
	    }

	    ww->wobbly |= WobblyInitial;
	    ws->wobblyWindows |= ww->wobbly;

	    damagePendingOnScreen (w->screen);
	}

	ww->grabbed = FALSE;
    }

    UNWRAP (ws, w->screen, windowUngrabNotify);
    (*w->screen->windowUngrabNotify) (w);
    WRAP (ws, w->screen, windowUngrabNotify, wobblyWindowUngrabNotify);
}


static Bool
wobblyPaintScreen (CompScreen		   *s,
		   const ScreenPaintAttrib *sAttrib,
		   Region		   region,
		   int			   output,
		   unsigned int		   mask)
{
    Bool status;

    WOBBLY_SCREEN (s);

    if (ws->wobblyWindows)
	mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

    UNWRAP (ws, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, region, output, mask);
    WRAP (ws, s, paintScreen, wobblyPaintScreen);

    return status;
}

static CompOption *
wobblyGetDisplayOptions (CompDisplay *display,
			 int	     *count)
{
    WOBBLY_DISPLAY (display);

    *count = NUM_OPTIONS (wd);
    return wd->opt;
}

static Bool
wobblySetDisplayOption (CompDisplay     *display,
			char	        *name,
			CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    WOBBLY_DISPLAY (display);

    o = compFindOption (wd->opt, NUM_OPTIONS (wd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case WOBBLY_DISPLAY_OPTION_SNAP:
	/* ignore the key */
	value->action.key.keycode = 0;

	if (compSetActionOption (o, value))
	    return TRUE;
	break;
    case WOBBLY_DISPLAY_OPTION_SHIVER:
	if (setDisplayAction (display, o, value))
	    return TRUE;
	break;
    default:
	break;
    }

    return FALSE;
}

static void
wobblyDisplayInitOptions (WobblyDisplay *wd)
{
    CompOption *o;

    o = &wd->opt[WOBBLY_DISPLAY_OPTION_SNAP];
    o->name			  = "snap";
    o->shortDesc		  = N_("Snap windows");
    o->longDesc			  = N_("Toggle window snapping");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = wobblyEnableSnapping;
    o->value.action.terminate	  = wobblyDisableSnapping;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = 0;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = WOBBLY_SNAP_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   = 0;

    o = &wd->opt[WOBBLY_DISPLAY_OPTION_SHIVER];
    o->name		      = "shiver";
    o->shortDesc	      = N_("Shiver");
    o->longDesc		      = N_("Make window shiver");
    o->type		      = CompOptionTypeAction;
    o->value.action.initiate  = wobblyShiver;
    o->value.action.terminate = 0;
    o->value.action.bell      = FALSE;
    o->value.action.edgeMask  = 0;
    o->value.action.state     = CompActionStateInitBell;
    o->value.action.state    |= CompActionStateInitKey;
    o->value.action.state    |= CompActionStateInitButton;
    o->value.action.type      = 0;
}

static Bool
wobblyInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    WobblyDisplay *wd;

    wd = malloc (sizeof (WobblyDisplay));
    if (!wd)
	return FALSE;

    wd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (wd->screenPrivateIndex < 0)
    {
	free (wd);
	return FALSE;
    }

    WRAP (wd, d, handleEvent, wobblyHandleEvent);

    wd->snapping = FALSE;

    wobblyDisplayInitOptions (wd);

    d->privates[displayPrivateIndex].ptr = wd;

    return TRUE;
}

static void
wobblyFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    WOBBLY_DISPLAY (d);

    freeScreenPrivateIndex (d, wd->screenPrivateIndex);

    UNWRAP (wd, d, handleEvent);

    free (wd);
}

static Bool
wobblyInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    WobblyScreen *ws;

    WOBBLY_DISPLAY (s->display);

    ws = malloc (sizeof (WobblyScreen));
    if (!ws)
	return FALSE;

    ws->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ws->windowPrivateIndex < 0)
    {
	free (ws);
	return FALSE;
    }

    ws->wobblyWindows = FALSE;

    ws->mapEffect   = WobblyEffectShiver;
    ws->focusEffect = WobblyEffectNone;

    ws->grabMask   = 0;
    ws->grabWindow = NULL;

    wobblyScreenInitOptions (ws, s->display->display);

    WRAP (ws, s, preparePaintScreen, wobblyPreparePaintScreen);
    WRAP (ws, s, donePaintScreen, wobblyDonePaintScreen);
    WRAP (ws, s, paintScreen, wobblyPaintScreen);
    WRAP (ws, s, paintWindow, wobblyPaintWindow);
    WRAP (ws, s, damageWindowRect, wobblyDamageWindowRect);
    WRAP (ws, s, addWindowGeometry, wobblyAddWindowGeometry);
    WRAP (ws, s, drawWindowGeometry, wobblyDrawWindowGeometry);
    WRAP (ws, s, setWindowScale, wobblySetWindowScale);
    WRAP (ws, s, windowResizeNotify, wobblyWindowResizeNotify);
    WRAP (ws, s, windowMoveNotify, wobblyWindowMoveNotify);
    WRAP (ws, s, windowGrabNotify, wobblyWindowGrabNotify);
    WRAP (ws, s, windowUngrabNotify, wobblyWindowUngrabNotify);

    s->privates[wd->screenPrivateIndex].ptr = ws;

    return TRUE;
}

static void
wobblyFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    WOBBLY_SCREEN (s);

    freeWindowPrivateIndex (s, ws->windowPrivateIndex);

    free (ws->opt[WOBBLY_SCREEN_OPTION_MAP_EFFECT].value.s);
    free (ws->opt[WOBBLY_SCREEN_OPTION_FOCUS_EFFECT].value.s);

    UNWRAP (ws, s, preparePaintScreen);
    UNWRAP (ws, s, donePaintScreen);
    UNWRAP (ws, s, paintScreen);
    UNWRAP (ws, s, paintWindow);
    UNWRAP (ws, s, damageWindowRect);
    UNWRAP (ws, s, addWindowGeometry);
    UNWRAP (ws, s, drawWindowGeometry);
    UNWRAP (ws, s, setWindowScale);
    UNWRAP (ws, s, windowResizeNotify);
    UNWRAP (ws, s, windowMoveNotify);
    UNWRAP (ws, s, windowGrabNotify);
    UNWRAP (ws, s, windowUngrabNotify);

    free (ws);
}

static Bool
wobblyInitWindow (CompPlugin *p,
		  CompWindow *w)
{
    WobblyWindow *ww;

    WOBBLY_SCREEN (w->screen);

    ww = malloc (sizeof (WobblyWindow));
    if (!ww)
	return FALSE;

    ww->model   = 0;
    ww->wobbly  = 0;
    ww->grabbed = FALSE;
    ww->state   = w->state;

    w->privates[ws->windowPrivateIndex].ptr = ww;

    if (w->mapNum && ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
    {
	if (isWobblyWin (w))
	    wobblyEnsureModel (w);
    }

    return TRUE;
}

static void
wobblyFiniWindow (CompPlugin *p,
		  CompWindow *w)
{
    WOBBLY_WINDOW (w);

    if (ww->model)
    {
	free (ww->model->objects);
	free (ww->model);
    }

    free (ww);
}

static Bool
wobblyInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
wobblyFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
wobblyGetVersion (CompPlugin *plugin,
		  int	     version)
{
    return ABIVERSION;
}

CompPluginDep wobblyDeps[] = {
    { CompPluginRuleBefore, "fade" },
    { CompPluginRuleBefore, "cube" },
    { CompPluginRuleBefore, "scale" }
};

CompPluginVTable wobblyVTable = {
    "wobbly",
    N_("Wobbly Windows"),
    N_("Use spring model for wobbly window effect"),
    wobblyGetVersion,
    wobblyInit,
    wobblyFini,
    wobblyInitDisplay,
    wobblyFiniDisplay,
    wobblyInitScreen,
    wobblyFiniScreen,
    wobblyInitWindow,
    wobblyFiniWindow,
    wobblyGetDisplayOptions,
    wobblySetDisplayOption,
    wobblyGetScreenOptions,
    wobblySetScreenOption,
    wobblyDeps,
    sizeof (wobblyDeps) / sizeof (wobblyDeps[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &wobblyVTable;
}
