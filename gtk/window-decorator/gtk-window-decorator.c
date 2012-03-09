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
 * 2D Mode: Copyright © 2009 Sam Spilsbury <smspillaz@gmail.com>
 * Frames Management: Copright © 2010 Canonical Ltd.
 *        Authored By: Sam Spilsbury <sam.spilsbury@canonical.com>
 */

#include "gtk-window-decorator.h"

gboolean minimal = FALSE;

#define SWITCHER_SPACE 40

guint cmdline_options = 0;

GdkPixmap *decor_normal_pixmap = NULL;
GdkPixmap *decor_active_pixmap = NULL;

Atom frame_input_window_atom;
Atom frame_output_window_atom;
Atom win_decor_atom;
Atom win_blur_decor_atom;
Atom wm_move_resize_atom;
Atom restack_window_atom;
Atom select_window_atom;
Atom mwm_hints_atom;
Atom switcher_fg_atom;
Atom compiz_shadow_info_atom;
Atom compiz_shadow_color_atom;
Atom toolkit_action_atom;
Atom toolkit_action_window_menu_atom;
Atom toolkit_action_force_quit_dialog_atom;

Time dm_sn_timestamp;

struct _cursor cursor[3][3] = {
    { C (top_left_corner),    C (top_side),    C (top_right_corner)    },
    { C (left_side),	      C (left_ptr),    C (right_side)	       },
    { C (bottom_left_corner), C (bottom_side), C (bottom_right_corner) }
};

struct _pos pos[3][3] = {
    {
	{  0,  0, 10, 21,   0, 0, 0, 0, 0, 1 },
	{ 10,  0, -8,  6,   0, 0, 1, 0, 0, 1 },
	{  2,  0, 10, 21,   1, 0, 0, 0, 0, 1 }
    }, {
	{  0, 10,  6, 11,   0, 0, 0, 1, 1, 0 },
	{  6,  6,  0, 15,   0, 0, 1, 0, 0, 1 },
	{  6, 10,  6, 11,   1, 0, 0, 1, 1, 0 }
    }, {
	{  0, 17, 10, 10,   0, 1, 0, 0, 1, 0 },
	{ 10, 21, -8,  6,   0, 1, 1, 0, 1, 0 },
	{  2, 17, 10, 10,   1, 1, 0, 0, 1, 0 }
    }
}, bpos[] = {
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 6, 2, 16, 16,   0, 0, 0, 0, 0, 0 }
};

const gchar * window_type_frames[WINDOW_TYPE_FRAMES_NUM] = {
    "normal", "dialog", "menu", "utility"
};

char *program_name;

GtkWidget     *switcher_label;

GHashTable    *frame_table;
GtkWidget     *action_menu = NULL;
gboolean      action_menu_mapped = FALSE;
decor_color_t _title_color[2];
gint	     double_click_timeout = 250;

GtkWidget     *tip_window;
GtkWidget     *tip_label;
GTimeVal	     tooltip_last_popdown = { 0, 0 };
gint	     tooltip_timer_tag = 0;

GSList *draw_list = NULL;
guint  draw_idle_id = 0;

Window    switcher_selected_window = None;
decor_t   *switcher_window = NULL;

XRenderPictFormat *xformat_rgba;
XRenderPictFormat *xformat_rgb;

decor_settings_t *settings;

int
main (int argc, char *argv[])
{
    GdkDisplay    *gdkdisplay;
    Display       *xdisplay;
    GdkScreen     *gdkscreen;
    WnckScreen    *screen;
    gint          i, j, status;
    gboolean      replace = FALSE;
    unsigned int  nchildren;
    Window        root_ret, parent_ret;
    Window        *children = NULL;
    GList	  *windows, *win;
    decor_frame_t *default_p, *bare_p, *switcher_p;

#ifdef USE_METACITY
    char       *meta_theme = NULL;
    MetaTheme  *theme = NULL;
#endif

    program_name = argv[0];

    settings = malloc (sizeof (decor_settings_t));

    if (!settings)
	return 1;

    gtk_init (&argc, &argv);

    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    settings->blur_type = BLUR_TYPE_NONE;
    settings->use_system_font = FALSE;

    settings->shadow_radius   = SHADOW_RADIUS;
    settings->shadow_opacity  = SHADOW_OPACITY;
    settings->shadow_color[0] = SHADOW_COLOR_RED;
    settings->shadow_color[1] = SHADOW_COLOR_GREEN;
    settings->shadow_color[2] = SHADOW_COLOR_BLUE;
    settings->shadow_offset_x = SHADOW_OFFSET_X;
    settings->shadow_offset_y = SHADOW_OFFSET_Y;
    settings->decoration_alpha = 0.5;

#ifdef USE_METACITY

    settings->meta_opacity              = META_OPACITY;
    settings->meta_shade_opacity        = META_SHADE_OPACITY;
    settings->meta_active_opacity       = META_ACTIVE_OPACITY;
    settings->meta_active_shade_opacity = META_ACTIVE_SHADE_OPACITY;

    settings->meta_button_layout_set = FALSE;
#endif

    settings->font = strdup ("Sans Bold 12");

    for (i = 0; i < argc; i++)
    {
	if (strcmp (argv[i], "--minimal") == 0)
	{
	    minimal = TRUE;
	}
	else if (strcmp (argv[i], "--replace") == 0)
	{
	    replace = TRUE;
	}
	else if (strcmp (argv[i], "--blur") == 0)
	{
	    if (argc > ++i)
	    {
		if (strcmp (argv[i], "titlebar") == 0)
		    settings->blur_type = BLUR_TYPE_TITLEBAR;
		else if (strcmp (argv[i], "all") == 0)
		    settings->blur_type = BLUR_TYPE_ALL;
	    }
	    cmdline_options |= CMDLINE_BLUR;
	}

#ifdef USE_METACITY
	else if (strcmp (argv[i], "--opacity") == 0)
	{
	    if (argc > ++i)
		settings->meta_opacity = atof (argv[i]);
	    cmdline_options |= CMDLINE_OPACITY;
	}
	else if (strcmp (argv[i], "--no-opacity-shade") == 0)
	{
	    settings->meta_shade_opacity = FALSE;
	    cmdline_options |= CMDLINE_OPACITY_SHADE;
	}
	else if (strcmp (argv[i], "--active-opacity") == 0)
	{
	    if (argc > ++i)
		settings->meta_active_opacity = atof (argv[i]);
	    cmdline_options |= CMDLINE_ACTIVE_OPACITY;
	}
	else if (strcmp (argv[i], "--no-active-opacity-shade") == 0)
	{
	    settings->meta_active_shade_opacity = FALSE;
	    cmdline_options |= CMDLINE_ACTIVE_OPACITY_SHADE;
	}
	else if (strcmp (argv[i], "--metacity-theme") == 0)
	{
	    if (argc > ++i)
		meta_theme = argv[i];
	    cmdline_options |= CMDLINE_THEME;
	}
#endif

	else if (strcmp (argv[i], "--help") == 0)
	{
	    fprintf (stderr, "%s "
		     "[--minimal] "
		     "[--replace] "
		     "[--blur none|titlebar|all] "

#ifdef USE_METACITY
		     "[--opacity OPACITY] "
		     "[--no-opacity-shade] "
		     "[--active-opacity OPACITY] "
		     "[--no-active-opacity-shade] "
		     "[--metacity-theme THEME] "
#endif

		     "[--help]"

		     "\n", program_name);
	    return 0;
	}
    }

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);

    frame_input_window_atom  = XInternAtom (xdisplay,
					    DECOR_INPUT_FRAME_ATOM_NAME, FALSE);
    frame_output_window_atom = XInternAtom (xdisplay,
					    DECOR_OUTPUT_FRAME_ATOM_NAME, FALSE);

    win_decor_atom	= XInternAtom (xdisplay, DECOR_WINDOW_ATOM_NAME, FALSE);
    win_blur_decor_atom	= XInternAtom (xdisplay, DECOR_BLUR_ATOM_NAME, FALSE);
    wm_move_resize_atom = XInternAtom (xdisplay, "_NET_WM_MOVERESIZE", FALSE);
    restack_window_atom = XInternAtom (xdisplay, "_NET_RESTACK_WINDOW", FALSE);
    select_window_atom	= XInternAtom (xdisplay, DECOR_SWITCH_WINDOW_ATOM_NAME,
				       FALSE);
    mwm_hints_atom	= XInternAtom (xdisplay, "_MOTIF_WM_HINTS", FALSE);
    switcher_fg_atom    = XInternAtom (xdisplay,
				       DECOR_SWITCH_FOREGROUND_COLOR_ATOM_NAME,
				       FALSE);
    
    compiz_shadow_info_atom  = XInternAtom (xdisplay, "_COMPIZ_NET_CM_SHADOW_PROPERTIES", FALSE);
    compiz_shadow_color_atom = XInternAtom (xdisplay, "_COMPIZ_NET_CM_SHADOW_COLOR", FALSE);

    toolkit_action_atom			  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION", FALSE);
    toolkit_action_window_menu_atom	  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_WINDOW_MENU", FALSE);
    toolkit_action_force_quit_dialog_atom =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_FORCE_QUIT_DIALOG",
		     FALSE);

    status = decor_acquire_dm_session (xdisplay,
				       gdk_screen_get_number (gdkscreen),
				       "gwd", replace, &dm_sn_timestamp);
    if (status != DECOR_ACQUIRE_STATUS_SUCCESS)
    {
	if (status == DECOR_ACQUIRE_STATUS_FAILED)
	{
	    fprintf (stderr,
		     "%s: Could not acquire decoration manager "
		     "selection on screen %d display \"%s\"\n",
		     program_name, gdk_screen_get_number (gdkscreen),
		     DisplayString (xdisplay));
	}
	else if (status == DECOR_ACQUIRE_STATUS_OTHER_DM_RUNNING)
	{
	    fprintf (stderr,
		     "%s: Screen %d on display \"%s\" already "
		     "has a decoration manager; try using the "
		     "--replace option to replace the current "
		     "decoration manager.\n",
		     program_name, gdk_screen_get_number (gdkscreen),
		     DisplayString (xdisplay));
	}

	return 1;
    }

    screen = wnck_screen_get_default ();

    initialize_decorations ();

    if (!init_settings (screen))
    {
	free (settings);
	fprintf (stderr, "%s: Failed to get necessary gtk settings\n", argv[0]);
	return 1;
    }

    theme_draw_window_decoration    = draw_window_decoration;
    theme_calc_decoration_size	    = calc_decoration_size;
    theme_update_border_extents	    = update_border_extents;
    theme_get_event_window_position = get_event_window_position;
    theme_get_button_position       = get_button_position;
    theme_get_title_scale	    = get_title_scale;

#ifdef USE_METACITY
    if (meta_theme)
    {
	meta_theme_set_current (meta_theme, TRUE);

	theme = meta_theme_get_current ();

	if (!theme)
	    g_warning ("specified a theme that does not exist! falling back to cairo decoration\n");
    }
    else
	theme = meta_theme_get_current ();

    if (theme)
    {
	theme_draw_window_decoration    = meta_draw_window_decoration;
	theme_calc_decoration_size	    = meta_calc_decoration_size;
	theme_update_border_extents	    = meta_update_border_extents;
	theme_get_event_window_position = meta_get_event_window_position;
	theme_get_button_position	    = meta_get_button_position;
	theme_get_title_scale	    = meta_get_title_scale;
    }
#endif

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    if (cursor[i][j].shape != XC_left_ptr)
		cursor[i][j].cursor =
		    XCreateFontCursor (xdisplay, cursor[i][j].shape);
	}
    }

    xformat_rgba = XRenderFindStandardFormat (xdisplay, PictStandardARGB32);
    xformat_rgb  = XRenderFindStandardFormat (xdisplay, PictStandardRGB24);

    frame_table = g_hash_table_new (NULL, NULL);

    if (!create_tooltip_window ())
    {
	free (settings);
	fprintf (stderr, "%s, Couldn't create tooltip window\n", argv[0]);
	return 1;
    }

    wnck_set_client_type (WNCK_CLIENT_TYPE_PAGER);

    gdk_window_add_filter (NULL,
			   selection_event_filter_func,
			   NULL);

    if (!minimal)
    {
	GdkWindow *root = create_foreign_window (gdk_x11_get_default_root_xwindow ());

 	gdk_window_add_filter (NULL,
 			       event_filter_func,
 			       NULL);
			       
	XQueryTree (xdisplay, gdk_x11_get_default_root_xwindow (),
		    &root_ret, &parent_ret, &children, &nchildren);

	for (i = 0; i < nchildren; i++)
	{
	    GdkWindow *toplevel = create_foreign_window  (children[i]);

	    /* Need property notify on all windows */

	    gdk_window_set_events (toplevel,
				   gdk_window_get_events (toplevel) |
				   GDK_PROPERTY_CHANGE_MASK);
	}

	/* Need MapNotify on new windows */
	gdk_window_set_events (root, gdk_window_get_events (root) |
			       GDK_STRUCTURE_MASK |
			       GDK_PROPERTY_CHANGE_MASK |
			       GDK_VISIBILITY_NOTIFY_MASK |
			       GDK_SUBSTRUCTURE_MASK);
 
 	connect_screen (screen);
    }

    /* Keep the default, bare and switcher decorations around
     * since otherwise they will be spuriously recreated */

    default_p = gwd_get_decor_frame ("default");
    bare_p = gwd_get_decor_frame ("bare");
    switcher_p = gwd_get_decor_frame ("switcher");

    decor_set_dm_check_hint (xdisplay, gdk_screen_get_number (gdkscreen),
			     WINDOW_DECORATION_TYPE_PIXMAP |
			     WINDOW_DECORATION_TYPE_WINDOW);

    update_default_decorations (gdkscreen);

    gtk_main ();

    win = windows = wnck_screen_get_windows (screen);

    while (win != NULL)
    {
	WnckWindow *w = (WnckWindow *) win->data;

	window_closed (screen, w);

	win = g_list_next (win);
    }

    g_list_free (windows);

    if (tip_label)
	gtk_widget_destroy (GTK_WIDGET (tip_label));

    if (tip_window)
	gtk_widget_destroy (GTK_WIDGET (tip_window));

    gwd_decor_frame_unref (default_p);
    gwd_decor_frame_unref (bare_p);
    gwd_decor_frame_unref (switcher_p);

    g_free (settings->font);

    free (settings);

    return 0;
}
