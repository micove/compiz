/*
 * Copyright © 2006 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Reveman <davidr@novell.com>
 *
 * 2D Mode: Copyright © 2010 Sam Spilsbury <smspillaz@gmail.com>
 * Frames Management: Copright © 2011 Canonical Ltd.
 *        Authored By: Sam Spilsbury <sam.spilsbury@canonical.com>
 */

#include "gtk-window-decorator.h"

decor_frame_t *
create_switcher_frame (const gchar *type)
{
    AtkObject	   *switcher_label_obj;
    decor_frame_t *frame = decor_frame_new (type);
    decor_extents_t _switcher_extents    = { 6, 6, 6, 6 + SWITCHER_SPACE };

    decor_context_t _switcher_context = {
	{ 0, 0, 0, 0 },
	6, 6, 6, 6 + SWITCHER_SPACE,
	0, 0, 0, 0
    };

    frame->win_extents = _switcher_extents;
    frame->max_win_extents = _switcher_extents;
    frame->win_extents = _switcher_extents;
    frame->window_context_inactive = _switcher_context;
    frame->window_context_active = _switcher_context;
    frame->window_context_no_shadow = _switcher_context;
    frame->max_window_context_active = _switcher_context;
    frame->max_window_context_inactive = _switcher_context;
    frame->max_window_context_no_shadow = _switcher_context;
    frame->update_shadow = switcher_frame_update_shadow;

    /* keep the switcher frame around since we need to keep its
     * contents */

    gwd_decor_frame_ref (frame);

    switcher_label = gtk_label_new ("");
    switcher_label_obj = gtk_widget_get_accessible (switcher_label);
    atk_object_set_role (switcher_label_obj, ATK_ROLE_STATUSBAR);
    gtk_container_add (GTK_CONTAINER (frame->style_window_rgba), switcher_label);

    return frame;
}

void
destroy_switcher_frame (decor_frame_t *frame)
{
    gtk_widget_destroy (switcher_label);
    decor_frame_destroy (frame);
}

static void
draw_switcher_background (decor_t *d)
{
    Display	  *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    cairo_t	  *cr;
    GtkStyleContext *context;
    GdkRGBA bg, fg;
    decor_color_t color;
    double	  alpha = SWITCHER_ALPHA / 65535.0;
    double	  x1, y1, x2, y2, h;
    int		  top;
    unsigned long pixel;
    ushort	  a = SWITCHER_ALPHA;

    if (!d->buffer_surface)
	return;

    context = gtk_widget_get_style_context (d->frame->style_window_rgba);
    gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &fg);

    color.r = bg.red;
    color.g = bg.green;
    color.b = bg.blue;

    cr = cairo_create (d->buffer_surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    top = d->frame->win_extents.top;

    x1 = d->frame->window_context_active.left_space - d->frame->win_extents.left;
    y1 = d->frame->window_context_active.top_space - d->frame->win_extents.top;
    x2 = d->width - d->frame->window_context_active.right_space + d->frame->win_extents.right;
    y2 = d->height - d->frame->window_context_active.bottom_space + d->frame->win_extents.bottom;

    h = y2 - y1 - d->frame->win_extents.top - d->frame->win_extents.top;

    cairo_set_line_width (cr, 1.0);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    draw_shadow_background (d, cr, d->frame->border_shadow_active, &d->frame->window_context_active);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + 0.5,
			    d->frame->win_extents.left - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + d->frame->win_extents.left,
			    y1 + 0.5,
			    x2 - x1 - d->frame->win_extents.left -
			    d->frame->win_extents.right,
			    top - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP);

    fill_rounded_rectangle (cr,
			    x2 - d->frame->win_extents.right,
			    y1 + 0.5,
			    d->frame->win_extents.right - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + top,
			    d->frame->win_extents.left - 0.5,
			    h,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x2 - d->frame->win_extents.right,
			    y1 + top,
			    d->frame->win_extents.right - 0.5,
			    h,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y2 - d->frame->win_extents.top,
			    d->frame->win_extents.left - 0.5,
			    d->frame->win_extents.top - 0.5,
			    5.0, CORNER_BOTTOMLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + d->frame->win_extents.left,
			    y2 - d->frame->win_extents.top,
			    x2 - x1 - d->frame->win_extents.left -
			    d->frame->win_extents.right,
			    d->frame->win_extents.top - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM);

    fill_rounded_rectangle (cr,
			    x2 - d->frame->win_extents.right,
			    y2 - d->frame->win_extents.top,
			    d->frame->win_extents.right - 0.5,
			    d->frame->win_extents.top - 0.5,
			    5.0, CORNER_BOTTOMRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_RIGHT);

    cairo_rectangle (cr, x1 + d->frame->win_extents.left,
		     y1 + top,
		     x2 - x1 - d->frame->win_extents.left - d->frame->win_extents.right,
		     h);
    bg.alpha = alpha;
    gdk_cairo_set_source_rgba (cr, &bg);
    cairo_fill (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_clip (cr);

    cairo_translate (cr, 1.0, 1.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.4);
    cairo_stroke (cr);

    cairo_translate (cr, -2.0, -2.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.1);
    cairo_stroke (cr);

    cairo_translate (cr, 1.0, 1.0);

    cairo_reset_clip (cr);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    fg.alpha = alpha;
    gdk_cairo_set_source_rgba (cr, &fg);

    cairo_stroke (cr);

    cairo_destroy (cr);

    copy_to_front_buffer (d);

    pixel  = (((a * (int) (bg.blue * 65535.0)) >> 24) & 0x0000ff);
    pixel |= (((a * (int) (bg.green * 65535.0)) >> 16) & 0x00ff00);
    pixel |= (((a * (int) (bg.red * 65535.0)) >> 8) & 0xff0000);
    pixel |= ((a & 0xff00) << 16);

    decor_update_switcher_property (d);

    gdk_error_trap_push ();
    XSetWindowBackground (xdisplay, d->prop_xid, pixel);
    XClearWindow (xdisplay, d->prop_xid);

    gdk_display_sync (gdk_display_get_default ());
    gdk_error_trap_pop_ignored ();

    d->prop_xid = 0;
}

static void
draw_switcher_foreground (decor_t *d)
{
    cairo_t	  *cr;
    GtkStyleContext *context;
    GdkRGBA bg, fg;
    double	  alpha = SWITCHER_ALPHA / 65535.0;

    if (!d->surface || !d->buffer_surface)
	return;

    context = gtk_widget_get_style_context (d->frame->style_window_rgba);
    gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);
    gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &fg);

    cr = cairo_create (d->buffer_surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    cairo_rectangle (cr, d->frame->window_context_active.left_space,
		     d->height - d->frame->window_context_active.bottom_space,
		     d->width - d->frame->window_context_active.left_space -
		     d->frame->window_context_active.right_space,
		     SWITCHER_SPACE);

    bg.alpha = alpha;
    gdk_cairo_set_source_rgba (cr, &bg);
    cairo_fill (cr);

    if (d->layout)
    {
	int w;

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	fg.alpha = 1.0;
	gdk_cairo_set_source_rgba (cr, &fg);

	pango_layout_get_pixel_size (d->layout, &w, NULL);

	cairo_move_to (cr, d->width / 2 - w / 2,
		       d->height - d->frame->window_context_active.bottom_space +
		       SWITCHER_SPACE / 2 - d->frame->text_height / 2);

	pango_cairo_show_layout (cr, d->layout);
    }

    cairo_destroy (cr);

    copy_to_front_buffer (d);
}

void
draw_switcher_decoration (decor_t *d)
{
    if (d->prop_xid)
	draw_switcher_background (d);

    draw_switcher_foreground (d);
}

void
switcher_window_closed ()
{
    decor_t *d = switcher_window;
    Display *xdisplay = gdk_x11_get_default_xdisplay ();

    if (d->layout)
	g_object_unref (G_OBJECT (d->layout));

    if (d->name)
	g_free (d->name);

    if (d->surface)
	cairo_surface_destroy (d->surface);

    if (d->buffer_surface)
	cairo_surface_destroy (d->buffer_surface);

    if (d->cr)
	cairo_destroy (d->cr);

    if (d->picture)
	XRenderFreePicture (xdisplay, d->picture);

    gwd_decor_frame_unref (switcher_window->frame);
    g_free (switcher_window);
    switcher_window = NULL;
}

/* Switcher is override-redirect now, we need to track
 * it separately */
decor_t *
switcher_window_opened (Window popup, Window window)
{
    decor_t      *d;

    d = switcher_window = calloc (1, sizeof (decor_t));
    if (!d)
	return NULL;

    return d;
}


gboolean
update_switcher_window (Window     popup,
			Window     selected)
{
    decor_t           *d = switcher_window;
    cairo_surface_t   *surface, *buffer_surface = NULL;
    unsigned int      height, width = 0, border, depth;
    int		      x, y;
    Window	      root_return;
    WnckWindow        *selected_win;
    Display           *xdisplay;
    XRenderPictFormat *format;

    if (!d)
	d = switcher_window_opened (popup, selected);

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    /* FIXME: Thats a round-trip */
    XGetGeometry (gdk_x11_get_default_xdisplay (), popup, &root_return,
		  &x, &y, &width, &height, &border, &depth);

    d->decorated = FALSE;
    d->draw	 = draw_switcher_decoration;
    d->frame     = gwd_get_decor_frame ("switcher");

    decor_get_default_layout (&d->frame->window_context_active, width, 1, &d->border_layout);

    width  = d->border_layout.width;
    height = d->border_layout.height;

    selected_win = wnck_window_get (selected);
    if (selected_win)
    {
	glong		name_length;
	PangoLayoutLine *line;
	const gchar	*name;

	if (d->name)
	{
	    g_free (d->name);
	    d->name = NULL;
	}

	name = wnck_window_get_name (selected_win);
	if (name && (name_length = strlen (name)))
	{
	    if (!d->layout)
	    {
		d->layout = pango_layout_new (d->frame->pango_context);
		if (d->layout)
		    pango_layout_set_wrap (d->layout, PANGO_WRAP_CHAR);
	    }

	    if (d->layout)
	    {
		int tw;

		tw = width - d->frame->window_context_active.left_space -
		    d->frame->window_context_active.right_space - 64;
		pango_layout_set_auto_dir (d->layout, FALSE);
		pango_layout_set_width (d->layout, tw * PANGO_SCALE);
		pango_layout_set_text (d->layout, name, name_length);

		line = pango_layout_get_line (d->layout, 0);

		name_length = line->length;
		if (pango_layout_get_line_count (d->layout) > 1)
		{
		    if (name_length < 4)
		    {
			g_object_unref (G_OBJECT (d->layout));
			d->layout = NULL;
		    }
		    else
		    {
			d->name = g_strndup (name, name_length);
			strcpy (d->name + name_length - 3, "...");
		    }
		}
		else
		    d->name = g_strndup (name, name_length);

		if (d->layout)
		    pango_layout_set_text (d->layout, d->name, name_length);
	    }
	}
	else if (d->layout)
	{
	    g_object_unref (G_OBJECT (d->layout));
	    d->layout = NULL;
	}
    }

    if (selected != switcher_selected_window)
    {
	gtk_label_set_text (GTK_LABEL (switcher_label), "");
	if (selected_win && d->name)
	    gtk_label_set_text (GTK_LABEL (switcher_label), d->name);
	switcher_selected_window = selected;
    }

    surface = create_native_surface_and_wrap (width, height, d->frame->style_window_rgba);
    if (!surface)
	return FALSE;

    buffer_surface = create_surface (width, height, d->frame->style_window_rgba);
    if (!buffer_surface)
    {
	cairo_surface_destroy (surface);
	return FALSE;
    }

    if (d->surface)
	cairo_surface_destroy (d->surface);

    if (d->x11Pixmap)
	decor_post_delete_pixmap (xdisplay,
				  0,
				  d->x11Pixmap);

    if (d->buffer_surface)
	cairo_surface_destroy (d->buffer_surface);

    if (d->cr)
	cairo_destroy (d->cr);

    if (d->picture)
	XRenderFreePicture (xdisplay, d->picture);

    d->surface        = surface;
    d->x11Pixmap      = cairo_xlib_surface_get_drawable (d->surface);
    d->buffer_surface = buffer_surface;
    d->cr             = cairo_create (surface);

    format = get_format_for_surface (d, d->buffer_surface);
    d->picture = XRenderCreatePicture (xdisplay, cairo_xlib_surface_get_drawable (buffer_surface),
				       format, 0, NULL);

    d->width  = width;
    d->height = height;

    d->prop_xid = popup;

    queue_decor_draw (d);

    return TRUE;
}
