/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2005 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <compiz-core.h>

#include <glib.h>

static CompMetadata placeMetadata;

#define PLACE_MODE_CASCADE  0
#define PLACE_MODE_CENTERED 1
#define PLACE_MODE_SMART    2
#define PLACE_MODE_MAXIMIZE 3
#define PLACE_MODE_RANDOM   4
#define PLACE_MODE_LAST     PLACE_MODE_RANDOM

/* overlap types */
#define NONE    0
#define H_WRONG -1
#define W_WRONG -2

static int displayPrivateIndex;

typedef struct _PlaceDisplay {
    int		    screenPrivateIndex;
} PlaceDisplay;

#define PLACE_SCREEN_OPTION_WORKAROUND        0
#define PLACE_SCREEN_OPTION_MODE              1
#define PLACE_SCREEN_OPTION_POSITION_MATCHES  2
#define PLACE_SCREEN_OPTION_POSITION_X_VALUES 3
#define PLACE_SCREEN_OPTION_POSITION_Y_VALUES 4
#define PLACE_SCREEN_OPTION_VIEWPORT_MATCHES  5
#define PLACE_SCREEN_OPTION_VIEWPORT_X_VALUES 6
#define PLACE_SCREEN_OPTION_VIEWPORT_Y_VALUES 7
#define PLACE_SCREEN_OPTION_NUM               8

typedef struct _PlaceScreen {
    CompOption opt[PLACE_SCREEN_OPTION_NUM];

    PlaceWindowProc                 placeWindow;
    ValidateWindowResizeRequestProc validateWindowResizeRequest;
} PlaceScreen;

#define GET_PLACE_DISPLAY(d)					   \
    ((PlaceDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define PLACE_DISPLAY(d)		     \
    PlaceDisplay *pd = GET_PLACE_DISPLAY (d)

#define GET_PLACE_SCREEN(s, pd)					       \
    ((PlaceScreen *) (s)->base.privates[(pd)->screenPrivateIndex].ptr)

#define PLACE_SCREEN(s)							   \
    PlaceScreen *ps = GET_PLACE_SCREEN (s, GET_PLACE_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static Bool
placeMatchXYValue (CompWindow *w,
		   CompOption *matches,
		   CompOption *xValues,
		   CompOption *yValues,
		   int	      *x,
		   int	      *y)
{
    int i, min;

    if (w->type & CompWindowTypeDesktopMask)
	return FALSE;

    min = MIN (matches->value.list.nValue, xValues->value.list.nValue);
    min = MIN (min, yValues->value.list.nValue);

    for (i = 0; i < min; i++)
    {
	if (matchEval (&matches->value.list.value[i].match, w))
	{
	    *x = xValues->value.list.value[i].i;
	    *y = yValues->value.list.value[i].i;

	    return TRUE;
	}
    }

    return FALSE;
}

static Bool
placeMatchPosition (CompWindow *w,
		    int	       *x,
		    int	       *y)
{
    PLACE_SCREEN (w->screen);

    return placeMatchXYValue (w,
			      &ps->opt[PLACE_SCREEN_OPTION_POSITION_MATCHES],
			      &ps->opt[PLACE_SCREEN_OPTION_POSITION_X_VALUES],
			      &ps->opt[PLACE_SCREEN_OPTION_POSITION_Y_VALUES],
			      x,
			      y);
}

static Bool
placeMatchViewport (CompWindow *w,
		    int	       *x,
		    int	       *y)
{
    PLACE_SCREEN (w->screen);

    return placeMatchXYValue (w,
			      &ps->opt[PLACE_SCREEN_OPTION_VIEWPORT_MATCHES],
			      &ps->opt[PLACE_SCREEN_OPTION_VIEWPORT_X_VALUES],
			      &ps->opt[PLACE_SCREEN_OPTION_VIEWPORT_Y_VALUES],
			      x,
			      y);
}

static CompOption *
placeGetScreenOptions (CompPlugin *plugin,
		       CompScreen *screen,
		       int	  *count)
{
    PLACE_SCREEN (screen);

    *count = NUM_OPTIONS (ps);
    return ps->opt;
}

static Bool
placeSetScreenOption (CompPlugin      *plugin,
		      CompScreen      *screen,
		      const char      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    PLACE_SCREEN (screen);

    o = compFindOption (ps->opt, NUM_OPTIONS (ps), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case PLACE_SCREEN_OPTION_MODE:
	if (compSetIntOption (o, value))
	    return TRUE;
	break;
    case PLACE_SCREEN_OPTION_POSITION_MATCHES:
    case PLACE_SCREEN_OPTION_VIEWPORT_MATCHES:
	if (compSetOptionList (o, value))
	{
	    int i;

	    for (i = 0; i < o->value.list.nValue; i++)
		matchUpdate (screen->display, &o->value.list.value[i].match);

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

typedef enum {
    PlaceLeft,
    PlaceRight,
    PlaceTop,
    PlaceBottom
} PlaceWindowDirection;

static Bool
rectangleIntersect (XRectangle *src1,
		    XRectangle *src2,
		    XRectangle *dest)
{
    int dest_x, dest_y;
    int dest_w, dest_h;
    int return_val;

    g_return_val_if_fail (src1 != NULL, FALSE);
    g_return_val_if_fail (src2 != NULL, FALSE);
    g_return_val_if_fail (dest != NULL, FALSE);

    return_val = FALSE;

    dest_x = MAX (src1->x, src2->x);
    dest_y = MAX (src1->y, src2->y);
    dest_w = MIN (src1->x + src1->width, src2->x + src2->width) - dest_x;
    dest_h = MIN (src1->y + src1->height, src2->y + src2->height) - dest_y;

    if (dest_w > 0 && dest_h > 0)
    {
	dest->x = dest_x;
	dest->y = dest_y;
	dest->width = dest_w;
	dest->height = dest_h;
	return_val = TRUE;
    }
    else
    {
	dest->width = 0;
	dest->height = 0;
    }

    return return_val;
}

static gint
northwestcmp (gconstpointer a,
	      gconstpointer b)
{
    CompWindow *aw = (gpointer) a;
    CompWindow *bw = (gpointer) b;
    int	       from_origin_a;
    int	       from_origin_b;
    int	       ax, ay, bx, by;

    ax = aw->serverX - aw->input.left;
    ay = aw->serverY - aw->input.top;

    bx = bw->serverX - bw->input.left;
    by = bw->serverY - bw->input.top;

    /* probably there's a fast good-enough-guess we could use here. */
    from_origin_a = sqrt (ax * ax + ay * ay);
    from_origin_b = sqrt (bx * bx + by * by);

    if (from_origin_a < from_origin_b)
	return -1;
    else if (from_origin_a > from_origin_b)
	return 1;
    else
	return 0;
}


static void
get_workarea_of_current_output_device (CompScreen *s,
				       XRectangle *area)
{
    getWorkareaForOutput (s, s->currentOutputDev, area);
}

static int
get_window_width (CompWindow *window)
{
    return window->serverWidth + window->serverBorderWidth * 2;
}

static int
get_window_height (CompWindow *window)
{
    return window->serverHeight + window->serverBorderWidth * 2;
}

static void
get_outer_rect_of_window (CompWindow *w,
			  XRectangle *r)
{
    r->x      = w->serverX - w->input.left;
    r->y      = w->serverY - w->input.top;
    r->width  = get_window_width (w)  + w->input.left + w->input.right;
    r->height = get_window_height (w) + w->input.top  + w->input.bottom;
}

static void
find_next_cascade (CompWindow *window,
		   GList      *windows,
		   int        x,
		   int        y,
		   int        *new_x,
		   int        *new_y)
{
    GList      *tmp;
    GList      *sorted;
    int	       cascade_x, cascade_y;
    int	       x_threshold, y_threshold;
    int	       window_width, window_height;
    int	       cascade_stage;
    XRectangle work_area;

    sorted = g_list_copy (windows);
    sorted = g_list_sort (sorted, northwestcmp);

    /* This is a "fuzzy" cascade algorithm.
     * For each window in the list, we find where we'd cascade a
     * new window after it. If a window is already nearly at that
     * position, we move on.
     */

    /* arbitrary-ish threshold, honors user attempts to
     * manually cascade.
     */
#define CASCADE_FUZZ 15

    x_threshold = MAX (window->input.left, CASCADE_FUZZ);
    y_threshold = MAX (window->input.top, CASCADE_FUZZ);

    /* Find furthest-SE origin of all workspaces.
     * cascade_x, cascade_y are the target position
     * of NW corner of window frame.
     */

    get_workarea_of_current_output_device (window->screen, &work_area);

    cascade_x = MAX (0, work_area.x);
    cascade_y = MAX (0, work_area.y);

    /* Find first cascade position that's not used. */

    window_width = get_window_width (window) + window->input.left +
	window->input.right;
    window_height = get_window_height (window) + window->input.top +
	window->input.bottom;

    cascade_stage = 0;
    tmp = sorted;
    while (tmp != NULL)
    {
	CompWindow *w;
	int	   wx, wy;

	w = tmp->data;

	/* we want frame position, not window position */
	wx = w->serverX - w->input.left;
	wy = w->serverY - w->input.top;

	if (ABS (wx - cascade_x) < x_threshold &&
	    ABS (wy - cascade_y) < y_threshold)
	{
	    /* This window is "in the way", move to next cascade
	     * point. The new window frame should go at the origin
	     * of the client window we're stacking above.
	     */
	    wx = w->serverX;
	    wy = w->serverY;

	    cascade_x = wx;
	    cascade_y = wy;

	    /* If we go off the screen, start over with a new cascade */
	    if (((cascade_x + window_width) >
		 (work_area.x + work_area.width)) ||
		((cascade_y + window_height) >
		 (work_area.y + work_area.height)))
	    {
		cascade_x = MAX (0, work_area.x);
		cascade_y = MAX (0, work_area.y);

#define CASCADE_INTERVAL 50 /* space between top-left corners of cascades */

		cascade_stage += 1;
		cascade_x += CASCADE_INTERVAL * cascade_stage;

		/* start over with a new cascade translated to the right,
		 * unless we are out of space
		 */
		if ((cascade_x + window_width) <
		    (work_area.x + work_area.width))
		{
		    tmp = sorted;
		    continue;
		}
		else
		{
		    /* All out of space, this cascade_x won't work */
		    cascade_x = MAX (0, work_area.x);
		    break;
		}
	    }
	}
	else
	{
	    /* Keep searching for a further-down-the-diagonal window. */
	}

	tmp = tmp->next;
    }

    /* cascade_x and cascade_y will match the last window in the list
     * that was "in the way" (in the approximate cascade diagonal)
     */

    g_list_free (sorted);

    /* Convert coords to position of window, not position of frame. */
    *new_x = cascade_x + window->input.left;
    *new_y = cascade_y + window->input.top;
}

static void
find_most_freespace (CompWindow *window,
		     CompWindow *focus_window,
		     int        x,
		     int        y,
		     int        *new_x,
		     int        *new_y)
{
    PlaceWindowDirection side;
    int			 max_area;
    int			 max_width, max_height, left, right, top, bottom;
    int			 left_space, right_space, top_space, bottom_space;
    int			 frame_size_left, frame_size_top;
    XRectangle		 work_area;
    XRectangle		 avoid;
    XRectangle		 outer;

    frame_size_left = window->input.left;
    frame_size_top  = window->input.top;

    get_workarea_of_current_output_device (window->screen, &work_area);

    get_outer_rect_of_window (focus_window, &avoid);
    get_outer_rect_of_window (window, &outer);

    /* Find the areas of choosing the various sides of the focus window */
    max_width  = MIN (avoid.width, outer.width);
    max_height = MIN (avoid.height, outer.height);
    left_space   = avoid.x - work_area.x;
    right_space  = work_area.width - (avoid.x + avoid.width - work_area.x);
    top_space    = avoid.y - work_area.y;
    bottom_space = work_area.height - (avoid.y + avoid.height - work_area.y);
    left   = MIN (left_space,   outer.width);
    right  = MIN (right_space,  outer.width);
    top    = MIN (top_space,    outer.height);
    bottom = MIN (bottom_space, outer.height);

    /* Find out which side of the focus_window can show the most of the
     * window
     */
    side = PlaceLeft;
    max_area = left * max_height;
    if (right * max_height > max_area)
    {
	side = PlaceRight;
	max_area = right * max_height;
    }
    if (top * max_width > max_area)
    {
	side = PlaceTop;
	max_area = top * max_width;
    }
    if (bottom * max_width > max_area)
    {
	side = PlaceBottom;
	max_area = bottom * max_width;
    }

    /* Give up if there's no where to put it
     * (i.e. focus window is maximized)
     */
    if (max_area == 0)
	return;

    /* Place the window on the relevant side; if the whole window fits,
     * make it adjacent to the focus window; if not, make sure the
     * window doesn't go off the edge of the screen.
     */
    switch (side) {
    case PlaceLeft:
	*new_y = avoid.y + frame_size_top;
	if (left_space > outer.width)
	    *new_x = avoid.x - outer.width + frame_size_left;
	else
	    *new_x = work_area.x + frame_size_left;
	break;
    case PlaceRight:
	*new_y = avoid.y + frame_size_top;
	if (right_space > outer.width)
	    *new_x = avoid.x + avoid.width + frame_size_left;
	else
	    *new_x = work_area.x + work_area.width - outer.width +
		frame_size_left;
	break;
    case PlaceTop:
	*new_x = avoid.x + frame_size_left;
	if (top_space > outer.height)
	    *new_y = avoid.y - outer.height + frame_size_top;
	else
	    *new_y = work_area.y + frame_size_top;
	break;
    case PlaceBottom:
	*new_x = avoid.x + frame_size_left;
	if (bottom_space > outer.height)
	    *new_y = avoid.y + avoid.height + frame_size_top;
	else
	    *new_y = work_area.y + work_area.height - outer.height +
		frame_size_top;
	break;
    }
}

static void
avoid_being_obscured_as_second_modal_dialog (CompWindow *window,
					     int        *x,
					     int        *y)
{
    /* We can't center this dialog if it was denied focus and it
     * overlaps with the focus window and this dialog is modal and this
     * dialog is in the same app as the focus window (*phew*...please
     * don't make me say that ten times fast). See bug 307875 comment 11
     * and 12 for details, but basically it means this is probably a
     * second modal dialog for some app while the focus window is the
     * first modal dialog.  We should probably make them simultaneously
     * visible in general, but it becomes mandatory to do so due to
     * buggy apps (e.g. those using gtk+ *sigh*) because in those cases
     * this second modal dialog also happens to be modal to the first
     * dialog in addition to the main window, while it has only let us
     * know about the modal-to-the-main-window part.
     */

    CompWindow *focus_window;

    focus_window =
	findWindowAtDisplay (window->screen->display,
			     window->screen->display->activeWindow);

    if (focus_window				   &&
	(window->state & CompWindowStateModalMask) &&
	0 /* window->denied_focus_and_not_transient	       &&
	window_same_application (window, focus_window) &&
	window_intersect (window, focus_window) */
	)
    {
	find_most_freespace (window, focus_window, *x, *y, x, y);
    }
}

static gboolean
rectangle_overlaps_some_window (XRectangle *rect,
				GList      *windows)
{
    GList *tmp;
    XRectangle dest;

    tmp = windows;
    while (tmp != NULL)
    {
	CompWindow *other = tmp->data;
	XRectangle other_rect;

	switch (other->type) {
	case CompWindowTypeDockMask:
	case CompWindowTypeSplashMask:
	case CompWindowTypeDesktopMask:
	case CompWindowTypeDialogMask:
	case CompWindowTypeModalDialogMask:
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeUnknownMask:
	    break;
	case CompWindowTypeNormalMask:
	case CompWindowTypeUtilMask:
	case CompWindowTypeToolbarMask:
	case CompWindowTypeMenuMask:
	    get_outer_rect_of_window (other, &other_rect);

	    if (rectangleIntersect (rect, &other_rect, &dest))
		return TRUE;
	    break;
	}

	tmp = tmp->next;
    }

    return FALSE;
}

static gint
leftmost_cmp (gconstpointer a,
	      gconstpointer b)
{
    CompWindow *aw = (gpointer) a;
    CompWindow *bw = (gpointer) b;
    int	       ax, bx;

    ax = aw->serverX - aw->input.left;
    bx = bw->serverX - bw->input.left;

    if (ax < bx)
	return -1;
    else if (ax > bx)
	return 1;
    else
	return 0;
}

static gint
topmost_cmp (gconstpointer a,
	     gconstpointer b)
{
    CompWindow *aw = (gpointer) a;
    CompWindow *bw = (gpointer) b;
    int	       ay, by;

    ay = aw->serverY - aw->input.top;
    by = bw->serverY - bw->input.top;

    if (ay < by)
	return -1;
    else if (ay > by)
	return 1;
    else
	return 0;
}

static void
center_tile_rect_in_area (XRectangle *rect,
			  XRectangle *work_area)
{
    int fluff;

    /* The point here is to tile a window such that "extra"
     * space is equal on either side (i.e. so a full screen
     * of windows tiled this way would center the windows
     * as a group)
     */

    fluff = (work_area->width % (rect->width + 1)) / 2;
    rect->x = work_area->x + fluff;
    fluff = (work_area->height % (rect->height + 1)) / 3;
    rect->y = work_area->y + fluff;
}

static gboolean
rect_fits_in_work_area (XRectangle *work_area,
			XRectangle *rect)
{
    return ((rect->x >= work_area->x) &&
	    (rect->y >= work_area->y) &&
	    (rect->x + rect->width <= work_area->x + work_area->width) &&
	    (rect->y + rect->height <= work_area->y + work_area->height));
}

/* Find the leftmost, then topmost, empty area on the workspace
 * that can contain the new window.
 *
 * Cool feature to have: if we can't fit the current window size,
 * try shrinking the window (within geometry constraints). But
 * beware windows such as Emacs with no sane minimum size, we
 * don't want to create a 1x1 Emacs.
 */
static gboolean
find_first_fit (CompWindow *window,
		GList      *windows,
		int        x,
		int        y,
		int        *new_x,
		int        *new_y)
{
    /* This algorithm is limited - it just brute-force tries
     * to fit the window in a small number of locations that are aligned
     * with existing windows. It tries to place the window on
     * the bottom of each existing window, and then to the right
     * of each existing window, aligned with the left/top of the
     * existing window in each of those cases.
     */
    int	       retval;
    GList      *below_sorted;
    GList      *right_sorted;
    GList      *tmp;
    XRectangle rect;
    XRectangle work_area;

    retval = FALSE;

    /* Below each window */
    below_sorted = g_list_copy (windows);
    below_sorted = g_list_sort (below_sorted, leftmost_cmp);
    below_sorted = g_list_sort (below_sorted, topmost_cmp);

    /* To the right of each window */
    right_sorted = g_list_copy (windows);
    right_sorted = g_list_sort (right_sorted, topmost_cmp);
    right_sorted = g_list_sort (right_sorted, leftmost_cmp);

    get_outer_rect_of_window (window, &rect);

    get_workarea_of_current_output_device (window->screen, &work_area);

    work_area.x += (window->initialViewportX - window->screen->x) *
	window->screen->width;
    work_area.y += (window->initialViewportY - window->screen->y) *
	window->screen->height;

    center_tile_rect_in_area (&rect, &work_area);

    if (rect_fits_in_work_area (&work_area, &rect) &&
	!rectangle_overlaps_some_window (&rect, windows))
    {
	*new_x = rect.x + window->input.left;
	*new_y = rect.y + window->input.top;

	retval = TRUE;

	goto out;
    }

    /* try below each window */
    tmp = below_sorted;
    while (tmp != NULL)
    {
	CompWindow *w = tmp->data;
	XRectangle outer_rect;

	get_outer_rect_of_window (w, &outer_rect);

	rect.x = outer_rect.x;
	rect.y = outer_rect.y + outer_rect.height;

	if (rect_fits_in_work_area (&work_area, &rect) &&
	    !rectangle_overlaps_some_window (&rect, below_sorted))
	{
	    *new_x = rect.x + window->input.left;
	    *new_y = rect.y + window->input.top;

	    retval = TRUE;

	    goto out;
	}

	tmp = tmp->next;
    }

    /* try to the right of each window */
    tmp = right_sorted;
    while (tmp != NULL)
    {
	CompWindow *w = tmp->data;
	XRectangle outer_rect;

	get_outer_rect_of_window (w, &outer_rect);

	rect.x = outer_rect.x + outer_rect.width;
	rect.y = outer_rect.y;

	if (rect_fits_in_work_area (&work_area, &rect) &&
	    !rectangle_overlaps_some_window (&rect, right_sorted))
	{
	    *new_x = rect.x + window->input.left;
	    *new_y = rect.y + window->input.top;

	    retval = TRUE;

	    goto out;
	}

	tmp = tmp->next;
    }

out:
    g_list_free (below_sorted);
    g_list_free (right_sorted);

    return retval;
}

static void
placeCentered (CompWindow *window,
	       XRectangle *workarea,
	       int	  *x,
	       int	  *y)
{
    *x = workarea->x + (workarea->width - get_window_width (window)) / 2;
    *y = workarea->y + (workarea->height - get_window_height (window)) / 2;
}

static void
placeRandom (CompWindow *window,
	     XRectangle *workarea,
	     int	*x,
	     int	*y)
{
    int remainX, remainY;

    *x = workarea->x;
    *y = workarea->y;

    remainX = workarea->width - get_window_width (window);
    if (remainX > 0)
	*x += rand () % remainX;

    remainY = workarea->height - get_window_height (window);
    if (remainY > 0)
	*y += rand () % remainY;
}

static void
placeSmart (CompWindow *window,
	    XRectangle *workarea,
	    int        *x,
	    int        *y)
{
    /*
     * SmartPlacement by Cristian Tibirna (tibirna@kde.org)
     * adapted for kwm (16-19jan98) and for kwin (16Nov1999) using (with
     * permission) ideas from fvwm, authored by
     * Anthony Martin (amartin@engr.csulb.edu).
     * Xinerama supported added by Balaji Ramani (balaji@yablibli.com)
     * with ideas from xfce.
     * adapted for Compiz by Bellegarde Cedric (gnumdk(at)gmail.com)
     */
    CompWindow *wi;
    long int overlap, minOverlap = 0;
    int xOptimal, yOptimal;
    int possible;

    /* temp coords */
    int cxl, cxr, cyt, cyb;
    /* temp coords */
    int  xl,  xr,  yt,  yb;
    /* temp holder */
    int basket;
    /* CT lame flag. Don't like it. What else would do? */
    Bool firstPass = TRUE;

    /* get the maximum allowed windows space */
    int xTmp = workarea->x;
    int yTmp = workarea->y;

    xOptimal = xTmp; yOptimal = yTmp;

    /* client gabarit */
    int ch = get_window_height (window) + window->input.top +
	     window->input.bottom - 1;
    int cw = get_window_width (window) + window->input.left +
	     window->input.right - 1;

    /* loop over possible positions */
    do
    {
	/* test if enough room in x and y directions */
	if (yTmp + ch > (workarea->y + workarea->height) &&
	    ch < workarea->height)
	    overlap = H_WRONG; /* this throws the algorithm to an exit */
	else if (xTmp + cw > (workarea->x + workarea->width))
	    overlap = W_WRONG;
	else
	{
	    overlap = NONE; /* initialize */

	    cxl = xTmp;
	    cxr = xTmp + cw;
	    cyt = yTmp;
	    cyb = yTmp + ch;

	    for (wi = window->screen->windows; wi; wi = wi->next)
	    {
		if (!wi->invisible &&
		    wi != window &&
		    !(wi->wmType & (CompWindowTypeDockMask |
				    CompWindowTypeDesktopMask)))
		{

		    xl = wi->attrib.x - wi->input.left;
		    yt = wi->attrib.y - wi->input.top;
		    xr = xl + get_window_width (wi) + window->input.left
			+ wi->input.right;
		    yb = yt + get_window_height (wi) + window->input.top
			+ wi->input.bottom;

		    /* if windows overlap, calc the overall overlapping */
		    if ((cxl < xr) && (cxr > xl) &&
			(cyt < yb) && (cyb > yt))
		    {
			xl = MAX (cxl, xl); xr = MIN (cxr, xr);
			yt = MAX (cyt, yt); yb = MIN (cyb, yb);
			if (wi->state & CompWindowStateAboveMask)
			    overlap += 16 * (xr - xl) * (yb - yt);
			else if (wi->state & CompWindowStateBelowMask)
			    overlap += 0;
			else
			    overlap += (xr - xl) * (yb - yt);
		    }
		}
	    }
	}

	/* CT first time we get no overlap we stop */
	if (overlap == NONE)
	{
	    xOptimal = xTmp;
	    yOptimal = yTmp;
	    break;
	}

	if (firstPass)
	{
	    firstPass = FALSE;
	    minOverlap = overlap;
	}
	/* CT save the best position and the minimum overlap up to now */
	else if (overlap >= NONE && overlap < minOverlap)
	{
	    minOverlap = overlap;
	    xOptimal = xTmp;
	    yOptimal = yTmp;
	}

	/* really need to loop? test if there's any overlap */
	if (overlap > NONE)
	{
	    possible = workarea->x + workarea->width;

	    if (possible - cw > xTmp) possible -= cw;

	    /* compare to the position of each client on the same desk */
	    for (wi = window->screen->windows; wi; wi = wi->next)
	    {

		if (!wi->invisible &&
		    wi != window &&
		    !(wi->wmType & (CompWindowTypeDockMask |
				    CompWindowTypeDesktopMask)))
		{

		    xl = wi->attrib.x - wi->input.left;
		    yt = wi->attrib.y - wi->input.top;
		    xr = xl + get_window_width (wi) + wi->input.left
			+ wi->input.right;
		    yb = yt + get_window_height (wi) + wi->input.top
			+ wi->input.bottom;

		    /* if not enough room above or under the current
		     * client determine the first non-overlapped x position
		     */
		    if ((yTmp < yb) && (yt < ch + yTmp))
		    {
			if ((xr > xTmp) && (possible > xr)) possible = xr;

			basket = xl - cw;
			if ((basket > xTmp) && (possible > basket))
			    possible = basket;
		    }
		}
	    }
	    xTmp = possible;
	}

	/* else ==> not enough x dimension (overlap was wrong on horizontal) */
	else if (overlap == W_WRONG)
	{
	    xTmp = workarea->x;
	    possible = workarea->y + workarea->height;

	    if (possible - ch > yTmp) possible -= ch;

	    /* test the position of each window on the desk */
	    for (wi = window->screen->windows; wi ; wi = wi->next)
	    {
		if (!wi->invisible &&
		    wi != window &&
		    !(wi->wmType & (CompWindowTypeDockMask |
				    CompWindowTypeDesktopMask)))
		{
		    xl = wi->attrib.x - wi->input.left;
		    yt = wi->attrib.y - wi->input.top;
		    xr = xl + get_window_width (wi) + wi->input.left
			+ wi->input.right;
		    yb = yt + get_window_height (wi) + wi->input.top
			+ wi->input.bottom;

		    /* if not enough room to the left or right of the current
		     * client determine the first non-overlapped y position
		     */
		    if ((yb > yTmp) && (possible > yb))
			possible = yb;

		    basket = yt - ch;
		    if ((basket > yTmp) && (possible > basket))
			possible = basket;
		}
	    }
	    yTmp = possible;
	}
    }
    while ((overlap != NONE) && (overlap != H_WRONG) && yTmp <
	   (workarea->y + workarea->height));

    if (ch >= workarea->height)
	yOptimal = workarea->y;

    *x = xOptimal + window->input.left;
    *y = yOptimal + window->input.top;
}

static void
placeSendWindowMaximizationRequest (CompWindow *w)
{
    XEvent      xev;
    CompDisplay *d = w->screen->display;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = d->display;
    xev.xclient.format  = 32;

    xev.xclient.message_type = d->winStateAtom;
    xev.xclient.window	     = w->id;

    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = d->winStateMaximizedHorzAtom;
    xev.xclient.data.l[2] = d->winStateMaximizedVertAtom;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent (d->display, w->screen->root, FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}


static void
placeWin (CompWindow *window,
     	  int        x,
	  int        y,
	  int        *new_x,
	  int        *new_y)
{
    CompWindow *wi;
    GList      *windows;
    XRectangle work_area;
    int	       x0 = (window->initialViewportX - window->screen->x) *
	window->screen->width;
    int	       y0 = (window->initialViewportY - window->screen->y) *
	window->screen->height;
    int	       window_width, window_height;

    PLACE_SCREEN (window->screen);

    window_width = get_window_width (window);
    window_height = get_window_height (window);

    get_workarea_of_current_output_device (window->screen, &work_area);

    work_area.x += x0;
    work_area.y += y0;

    windows = NULL;

    switch (window->type) {
    case CompWindowTypeSplashMask:
    case CompWindowTypeDialogMask:
    case CompWindowTypeModalDialogMask:
    case CompWindowTypeNormalMask:
	/* Run placement algorithm on these. */
	break;
    case CompWindowTypeDockMask:
    case CompWindowTypeDesktopMask:
    case CompWindowTypeUtilMask:
    case CompWindowTypeToolbarMask:
    case CompWindowTypeMenuMask:
    case CompWindowTypeFullscreenMask:
    case CompWindowTypeUnknownMask:
	/* Assume the app knows best how to place these, no placement
	 * algorithm ever (other than "leave them as-is")
	 */
	goto done_no_constraints;
	break;
    }

    /* don't run placement algorithm on windows that can't be moved */
    if (!(window->actions & CompWindowActionMoveMask))
    {
	goto done_no_constraints;
    }

    if (window->type & CompWindowTypeFullscreenMask)
    {
	x = x0;
	y = y0;
	goto done_no_constraints;
    }

    if (window->state & (CompWindowStateMaximizedVertMask |
			 CompWindowStateMaximizedHorzMask))
    {
	if (window->state & CompWindowStateMaximizedVertMask)
	    y = work_area.y + window->input.top;

	if (window->state & CompWindowStateMaximizedHorzMask)
	    x = work_area.x + window->input.left;

	goto done;
    }

    if (ps->opt[PLACE_SCREEN_OPTION_WORKAROUND].value.b)
    {
	/* workarounds enabled */

	if ((window->sizeHints.flags & PPosition) ||
	    (window->sizeHints.flags & USPosition))
	{
	    avoid_being_obscured_as_second_modal_dialog (window, &x, &y);
	    goto done;
	}
    }
    else
    {
	switch (window->type) {
	case CompWindowTypeNormalMask:
	    /* Only accept USPosition on normal windows because the app is full
	     * of shit claiming the user set -geometry for a dialog or dock
	     */
	    if (window->sizeHints.flags & USPosition)
	    {
		/* don't constrain with placement algorithm */
		goto done;
	    }
	    break;
	case CompWindowTypeSplashMask:
	case CompWindowTypeDialogMask:
	case CompWindowTypeModalDialogMask:
	    /* Ignore even USPosition on dialogs, splashscreen */
	    break;
	case CompWindowTypeDockMask:
	case CompWindowTypeDesktopMask:
	case CompWindowTypeUtilMask:
	case CompWindowTypeToolbarMask:
	case CompWindowTypeMenuMask:
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeUnknownMask:
	    /* Assume the app knows best how to place these. */
	    if (window->sizeHints.flags & PPosition)
	    {
		goto done_no_constraints;
	    }
	    break;
	}
    }

    if (window->transientFor &&
	(window->type & (CompWindowTypeDialogMask |
			 CompWindowTypeModalDialogMask)))
    {
	/* Center horizontally, at top of parent vertically */

	CompWindow *parent;

	parent = findWindowAtDisplay (window->screen->display,
				      window->transientFor);
	if (parent)
	{
	    int	w;

	    x = parent->serverX;
	    y = parent->serverY;

	    w = get_window_width (parent);

	    /* center of parent */
	    x = x + w / 2;

	    /* center of child over center of parent */
	    x -= window_width / 2;

	    /* "visually" center window over parent, leaving twice as
	     * much space below as on top.
	     */
	    y += (get_window_height (parent) - window_height) / 3;

	    /* put top of child's frame, not top of child's client */
	    y += window->input.top;

	    /* clip to screen if parent is visible in current viewport */
	    if (parent->serverX < parent->screen->width   &&
		parent->serverX + parent->serverWidth > 0 &&
		parent->serverY < parent->screen->height  &&
		parent->serverY + parent->serverHeight > 0)
	    {
		XRectangle        area;
		int               output;
		CompWindowExtents extents;

		output = outputDeviceForWindow (parent);
		getWorkareaForOutput (window->screen, output, &area);

		extents.left   = x - window->input.left;
		extents.top    = y - window->input.top;
		extents.right  = x + window_width + window->input.right;
		extents.bottom = y + window_height + window->input.bottom;

		if (extents.left < area.x)
		    x += area.x - extents.left;
		else if (extents.right > area.x + area.width)
		    x += area.x + area.width - extents.right;

		if (extents.top < area.y)
		    y += area.y - extents.top;
		else if (extents.bottom > area.y + area.height)
		    y += area.y + area.height - extents.bottom;
	    }

	    avoid_being_obscured_as_second_modal_dialog (window, &x, &y);

	    goto done_no_constraints;
	}
    }

    /* FIXME UTILITY with transient set should be stacked up
     * on the sides of the parent window or something.
     */
    if (window->type == CompWindowTypeDialogMask      ||
	window->type == CompWindowTypeModalDialogMask ||
	window->type == CompWindowTypeSplashMask)
    {
	/* Center on screen */
	int w, h;

	w = window->screen->width;
	h = window->screen->height;

	x = (w - window_width) / 2;
	y = (h - window_height) / 2;

	goto done_check_denied_focus;
    }

    /* Find windows that matter (not minimized, on same workspace
     * as placed window, may be shaded - if shaded we pretend it isn't
     * for placement purposes)
     */
    for (wi = window->screen->windows; wi; wi = wi->next)
    {
	if (!wi->shaded && wi->attrib.map_state != IsViewable)
	    continue;

	if (wi->serverX >= work_area.x + work_area.width       ||
	    wi->serverY + get_window_width (wi) <= work_area.x ||
	    wi->serverY >= work_area.y + work_area.height      ||
	    wi->serverY + get_window_height (wi) <= work_area.y)
	    continue;

	if (wi->attrib.override_redirect)
	    continue;

	if (wi->state & (CompWindowTypeDesktopMask    |
			 CompWindowTypeDockMask       |
			 CompWindowTypeFullscreenMask |
			 CompWindowTypeUnknownMask))
	    continue;

	if (wi != window)
	    windows = g_list_prepend (windows, wi);
    }

    /* "Origin" placement algorithm */
    x = x0;
    y = y0;

    if (placeMatchPosition (window, &x, &y))
    {
	int output;

	output = outputDeviceForGeometry (window->screen, x, y,
					  window_width, window_height,
					  window->serverBorderWidth);

	getWorkareaForOutput (window->screen, output, &work_area);

	work_area.x += x0;
	work_area.y += y0;
    }
    else
    {
	switch (ps->opt[PLACE_SCREEN_OPTION_MODE].value.i) {
	case PLACE_MODE_CASCADE:
	    if (find_first_fit (window, windows, x, y, &x, &y))
		goto done_check_denied_focus;

	    /* if the window wasn't placed at the origin of screen,
	     * cascade it onto the current screen
	     */
	    find_next_cascade (window, windows, x, y, &x, &y);
	    break;
	case PLACE_MODE_CENTERED:
	    placeCentered (window, &work_area, &x, &y);
	    break;
	case PLACE_MODE_RANDOM:
	    placeRandom (window, &work_area, &x, &y);
	    break;
	case PLACE_MODE_SMART:
	    placeSmart (window, &work_area, &x, &y);
	    break;
	case PLACE_MODE_MAXIMIZE:
	    maximizeWindow (window, MAXIMIZE_STATE);
	    break;
	default:
	    break;
	}
    }

done_check_denied_focus:
    /* If the window is being denied focus and isn't a transient of the
     * focus window, we do NOT want it to overlap with the focus window
     * if at all possible.  This is guaranteed to only be called if the
     * focus_window is non-NULL, and we try to avoid that window.
     */
    if (0 /* window->denied_focus_and_not_transient */)
    {
	gboolean    found_fit = FALSE;
	CompWindow  *focus_window;

	focus_window =
	    findWindowAtDisplay (window->screen->display,
				 window->screen->display->activeWindow);
	if (focus_window)
	{
	    XRectangle wr, fwr, overlap;

	    get_outer_rect_of_window (window, &wr);
	    get_outer_rect_of_window (focus_window, &fwr);

	    /* No need to do anything if the window doesn't overlap at all */
	    found_fit = !rectangleIntersect (&wr, &fwr, &overlap);

	    /* Try to do a first fit again, this time only taking into
	     * account the focus window.
	     */
	    if (!found_fit)
	    {
		GList *focus_window_list;

		focus_window_list = g_list_prepend (NULL, focus_window);

		/* Reset x and y ("origin" placement algorithm) */
		x = 0;
		y = 0;

		found_fit = find_first_fit (window, focus_window_list,
					    x, y, &x, &y);

		g_list_free (focus_window_list);
	    }
	}

	/* If that still didn't work, just place it where we can see as much
	 * as possible.
	 */
	if (!found_fit)
	    find_most_freespace (window, focus_window, x, y, &x, &y);
    }

    g_list_free (windows);

done:
    /* Maximize windows if they are too big for their work area (bit of
     * a hack here). Assume undecorated windows probably don't intend to
     * be maximized.
     */
    if ((window->actions & MAXIMIZE_STATE) == MAXIMIZE_STATE &&
	(window->mwmDecor & (MwmDecorAll | MwmDecorTitle))   &&
	!(window->state & CompWindowStateFullscreenMask))
    {
	XRectangle outer;

	get_outer_rect_of_window (window, &outer);

	if (outer.width >= work_area.width && outer.height >= work_area.height)
	    maximizeWindow (window, MAXIMIZE_STATE);
    }

    if (x + window_width + window->input.right > work_area.x + work_area.width)
	x = work_area.x + work_area.width - window_width - window->input.right;

    if (x - window->input.left < work_area.x)
	x = work_area.x + window->input.left;

    if (y + window_height + window->input.bottom >
	work_area.y + work_area.height)
	y = work_area.y + work_area.height
	    - window_height - window->input.bottom;

    if (y - window->input.top < work_area.y)
	y = work_area.y + window->input.top;

done_no_constraints:
    *new_x = x;
    *new_y = y;
}

static void
placeValidateWindowResizeRequest (CompWindow     *w,
				  unsigned int   *mask,
				  XWindowChanges *xwc)
{
    Bool       checkPlacement = FALSE;
    CompScreen *s = w->screen;

    PLACE_SCREEN (s);

    UNWRAP (ps, s, validateWindowResizeRequest);
    (*s->validateWindowResizeRequest) (w, mask, xwc);
    WRAP (ps, s, validateWindowResizeRequest,
	  placeValidateWindowResizeRequest);

    if (w->type & (CompWindowTypeSplashMask      |
		   CompWindowTypeDialogMask      |
		   CompWindowTypeModalDialogMask |
		   CompWindowTypeNormalMask))
    {
	if (!(w->state & CompWindowStateFullscreenMask))
	{
	    if (!(w->sizeHints.flags & USPosition))
		checkPlacement = TRUE;
	}
    }

    if (checkPlacement)
    {
	XRectangle workArea;
	int        x, y, left, right, top, bottom;
	int        output;

	/* left, right, top, bottom target coordinates, clamped to viewport
	   sizes as we don't need to validate movements to other viewports;
	   we are only interested in inner-viewport movements */
	x = xwc->x % s->width;
	if (x < 0)
	    x += s->width;

	y = xwc->y % s->height;
	if (y < 0)
	    y += s->height;

	left   = x - w->input.left;
	right  = x + xwc->width + w->input.right;
	top    = y - w->input.top;
	bottom = y + xwc->height + w->input.bottom;

	output = outputDeviceForGeometry (s,
					  xwc->x, xwc->y,
					  xwc->width, xwc->height,
					  w->serverBorderWidth);

	getWorkareaForOutput (s, output, &workArea);

	if (xwc->width >= workArea.width &&
	    xwc->height >= workArea.height)
	{
	    placeSendWindowMaximizationRequest (w);
	}

	if ((right - left) > workArea.width)
	{
	    left  = workArea.x;
	    right = left + workArea.width;
	}
	else
	{
	    if (left < workArea.x)
	    {
		right += workArea.x - left;
		left  = workArea.x;
	    }

	    if (right > (workArea.x + workArea.width))
	    {
		left -= right - (workArea.x + workArea.width);
		right = workArea.x + workArea.width;
	    }
	}

	if ((bottom - top) > workArea.height)
	{
	    top    = workArea.y;
	    bottom = top + workArea.height;
	}
	else
	{
	    if (top < workArea.y)
	    {
		bottom += workArea.y - top;
		top    = workArea.y;
	    }

	    if (bottom > (workArea.y + workArea.height))
	    {
		top   -= bottom - (workArea.y + workArea.height);
		bottom = workArea.y + workArea.height;
	    }
	}

	/* bring left/right/top/bottom to actual window coordinates */
	left   += w->input.left;
	right  -= w->input.right;
	top    += w->input.top;
	bottom -= w->input.bottom;

	if (left != x)
	{
	    xwc->x += left - x;
	    *mask  |= CWX;
	}

	if (top != y)
	{
	    xwc->y += top - y;
	    *mask  |= CWY;
	}

	if ((right - left) != xwc->width)
	{
	    xwc->width = right - left;
	    *mask      |= CWWidth;
	}

	if ((bottom - top) != xwc->height)
	{
	    xwc->height = bottom - top;
	    *mask       |= CWHeight;
	}
    }
}

static Bool
placePlaceWindow (CompWindow *w,
		  int        x,
		  int        y,
		  int        *newX,
		  int        *newY)
{
    Bool status;

    PLACE_SCREEN (w->screen);

    UNWRAP (ps, w->screen, placeWindow);
    status = (*w->screen->placeWindow) (w, x, y, newX, newY);
    WRAP (ps, w->screen, placeWindow, placePlaceWindow);

    if (!status)
    {
	int viewportX, viewportY;

	placeWin (w, x, y, newX, newY);

	if (placeMatchViewport (w, &viewportX, &viewportY))
	{
	    viewportX = MAX (MIN (viewportX, w->screen->hsize), 0);
	    viewportY = MAX (MIN (viewportY, w->screen->vsize), 0);

	    *newX += (viewportX - w->screen->x) * w->screen->width;
	    *newY += (viewportY - w->screen->y) * w->screen->height;
	}
    }

    return TRUE;
}

static Bool
placeInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PlaceDisplay *pd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    pd = malloc (sizeof (PlaceDisplay));
    if (!pd)
	return FALSE;

    pd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (pd->screenPrivateIndex < 0)
    {
	free (pd);
	return FALSE;
    }

    d->base.privates[displayPrivateIndex].ptr = pd;

    return TRUE;
}

static void
placeFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PLACE_DISPLAY (d);

    freeScreenPrivateIndex (d, pd->screenPrivateIndex);

    free (pd);
}

static const CompMetadataOptionInfo placeScreenOptionInfo[] = {
    { "workarounds", "bool", 0, 0, 0 },
    { "mode", "int", RESTOSTRING (0, PLACE_MODE_LAST), 0, 0 },
    { "position_matches", "list", "<type>match</type>", 0, 0 },
    { "position_x_values", "list", "<type>int</type>", 0, 0 },
    { "position_y_values", "list", "<type>int</type>", 0, 0 },
    { "viewport_matches", "list", "<type>match</type>", 0, 0 },
    { "viewport_x_values", "list", "<type>int</type>", 0, 0 },
    { "viewport_y_values", "list", "<type>int</type>", 0, 0 }
};

static Bool
placeInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    PlaceScreen *ps;

    PLACE_DISPLAY (s->display);

    ps = malloc (sizeof (PlaceScreen));
    if (!ps)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &placeMetadata,
					    placeScreenOptionInfo,
					    ps->opt,
					    PLACE_SCREEN_OPTION_NUM))
    {
	free (ps);
	return FALSE;
    }

    WRAP (ps, s, placeWindow, placePlaceWindow);
    WRAP (ps, s, validateWindowResizeRequest,
	  placeValidateWindowResizeRequest);

    s->base.privates[pd->screenPrivateIndex].ptr = ps;

    return TRUE;
}

static void
placeFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    PLACE_SCREEN (s);

    UNWRAP (ps, s, placeWindow);
    UNWRAP (ps, s, validateWindowResizeRequest);

    compFiniScreenOptions (s, ps->opt, PLACE_SCREEN_OPTION_NUM);

    free (ps);
}

static CompBool
placeInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) placeInitDisplay,
	(InitPluginObjectProc) placeInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
placeFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) placeFiniDisplay,
	(FiniPluginObjectProc) placeFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
placeGetObjectOptions (CompPlugin *plugin,
		       CompObject *object,
		       int	  *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) 0, /* GetDisplayOptions */
	(GetPluginObjectOptionsProc) placeGetScreenOptions
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) (*count = 0), (plugin, object, count));
}

static CompBool
placeSetObjectOption (CompPlugin      *plugin,
		      CompObject      *object,
		      const char      *name,
		      CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) 0, /* SetDisplayOption */
	(SetPluginObjectOptionProc) placeSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
placeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&placeMetadata,
					 p->vTable->name, 0, 0,
					 placeScreenOptionInfo,
					 PLACE_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&placeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&placeMetadata, p->vTable->name);

    return TRUE;
}

static void
placeFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&placeMetadata);
}

static CompMetadata *
placeGetMetadata (CompPlugin *plugin)
{
    return &placeMetadata;
}

static CompPluginVTable placeVTable = {
    "place",
    placeGetMetadata,
    placeInit,
    placeFini,
    placeInitObject,
    placeFiniObject,
    placeGetObjectOptions,
    placeSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &placeVTable;
}
