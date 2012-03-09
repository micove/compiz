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
 */

#include "gtk-window-decorator.h"

/* TODO: Trash all of this and use a window property
 * instead - much much cleaner!
 */

gboolean
shadow_property_changed (WnckScreen *s)
{
    GdkDisplay *display = gdk_display_get_default ();
    Display    *xdisplay = GDK_DISPLAY_XDISPLAY (display);
    GdkScreen  *screen = gdk_display_get_default_screen (display);
    Window     root = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));
    Atom actual;
    int  result, format;
    unsigned long n, left;
    unsigned char *prop_data;
    gboolean	  changed = FALSE;
    XTextProperty shadow_color_xtp;

    result = XGetWindowProperty (xdisplay, root, compiz_shadow_info_atom,
				 0, 32768, 0, XA_INTEGER, &actual,
				 &format, &n, &left, &prop_data);

    if (result != Success)
	return FALSE;

    if (n == 4)
    {
	long *data      = (long *) prop_data;
	gdouble radius  = data[0];
	gdouble opacity = data[1];
	gint x_off      = data[2];
	gint y_off      = data[3];

	/* Radius and Opacity are multiplied by 1000 to keep precision,
	 * divide by that much to get our real radius and opacity
	 */
	radius /= 1000;
	opacity /= 1000;

	changed = radius != settings->shadow_radius   ||
		  opacity != settings->shadow_opacity ||
		  x_off != settings->shadow_offset_x  ||
		  y_off != settings->shadow_offset_y;

	settings->shadow_radius = (gdouble) MAX (0.0, MIN (radius, 48.0));
	settings->shadow_opacity = (gdouble) MAX (0.0, MIN (opacity, 6.0));
	settings->shadow_offset_x = (gint) MAX (-16, MIN (x_off, 16));
	settings->shadow_offset_y = (gint) MAX (-16, MIN (y_off, 16));
    }

    XFree (prop_data);

    result = XGetTextProperty (xdisplay, root, &shadow_color_xtp,
			       compiz_shadow_color_atom);

    if (shadow_color_xtp.value)
    {
	int  ret_count = 0;
	char **t_data = NULL;
	
	XTextPropertyToStringList (&shadow_color_xtp, &t_data, &ret_count);
	
	if (ret_count == 1)
	{
	    int c[4];

	    if (sscanf (t_data[0], "#%2x%2x%2x%2x",
			&c[0], &c[1], &c[2], &c[3]) == 4)
	    {
		settings->shadow_color[0] = c[0] << 8 | c[0];
		settings->shadow_color[1] = c[1] << 8 | c[1];
		settings->shadow_color[2] = c[2] << 8 | c[2];
		changed = TRUE;
	    }
	}

	XFree (shadow_color_xtp.value);
	if (t_data)
	    XFreeStringList (t_data);
    }

    return changed;
}

#ifdef USE_GCONF
static gboolean
blur_settings_changed (GConfClient *client)
{
    gchar *type;
    int   new_type = settings->blur_type;

    if (cmdline_options & CMDLINE_BLUR)
	return FALSE;

    type = gconf_client_get_string (client,
				    BLUR_TYPE_KEY,
				    NULL);

    if (type)
    {
	if (strcmp (type, "titlebar") == 0)
	    new_type = BLUR_TYPE_TITLEBAR;
	else if (strcmp (type, "all") == 0)
	    new_type = BLUR_TYPE_ALL;
	else if (strcmp (type, "none") == 0)
	    new_type = BLUR_TYPE_NONE;

	g_free (type);
    }

    if (new_type != settings->blur_type)
    {
	settings->blur_type = new_type;
	return TRUE;
    }

    return FALSE;
}

static gboolean
theme_changed (GConfClient *client)
{

#ifdef USE_METACITY
    gboolean use_meta_theme;

    if (cmdline_options & CMDLINE_THEME)
	return FALSE;

    use_meta_theme = gconf_client_get_bool (client,
					    USE_META_THEME_KEY,
					    NULL);

    if (use_meta_theme)
    {
	gchar *theme;

	theme = gconf_client_get_string (client,
					 META_THEME_KEY,
					 NULL);

	if (theme)
	{
	    meta_theme_set_current (theme, TRUE);
	    if (!meta_theme_get_current ())
		use_meta_theme = FALSE;

	    g_free (theme);
	}
	else
	{
	    use_meta_theme = FALSE;
	}
    }

    if (use_meta_theme)
    {
	theme_draw_window_decoration	= meta_draw_window_decoration;
	theme_calc_decoration_size	= meta_calc_decoration_size;
	theme_update_border_extents	= meta_update_border_extents;
	theme_get_event_window_position = meta_get_event_window_position;
	theme_get_button_position	= meta_get_button_position;
	theme_get_title_scale	    	= meta_get_title_scale;
    }
    else
    {
	theme_draw_window_decoration	= draw_window_decoration;
	theme_calc_decoration_size	= calc_decoration_size;
	theme_update_border_extents	= update_border_extents;
	theme_get_event_window_position = get_event_window_position;
	theme_get_button_position	= get_button_position;
	theme_get_title_scale	    	= get_title_scale;
    }

    return TRUE;
#else
    theme_draw_window_decoration    = draw_window_decoration;
    theme_calc_decoration_size	    = calc_decoration_size;
    theme_update_border_extents	    = update_border_extents;
    theme_get_event_window_position = get_event_window_position;
    theme_get_button_position	    = get_button_position;
    theme_get_title_scale	    = get_title_scale;

    return FALSE;
#endif

}

static gboolean
theme_opacity_changed (GConfClient *client)
{

#ifdef USE_METACITY
    gboolean shade_opacity, changed = FALSE;
    gdouble  opacity;

    opacity = gconf_client_get_float (client,
				      META_THEME_OPACITY_KEY,
				      NULL);

    if (!(cmdline_options & CMDLINE_OPACITY) &&
	opacity != settings->meta_opacity)
    {
	settings->meta_opacity = opacity;
	changed = TRUE;
    }

    if (opacity < 1.0)
    {
	shade_opacity = gconf_client_get_bool (client,
					       META_THEME_SHADE_OPACITY_KEY,
					       NULL);

	if (!(cmdline_options & CMDLINE_OPACITY_SHADE) &&
	    shade_opacity != settings->meta_shade_opacity)
	{
	    settings->meta_shade_opacity = shade_opacity;
	    changed = TRUE;
	}
    }

    opacity = gconf_client_get_float (client,
				      META_THEME_ACTIVE_OPACITY_KEY,
				      NULL);

    if (!(cmdline_options & CMDLINE_ACTIVE_OPACITY) &&
	opacity != settings->meta_active_opacity)
    {
	settings->meta_active_opacity = opacity;
	changed = TRUE;
    }

    if (opacity < 1.0)
    {
	shade_opacity =
	    gconf_client_get_bool (client,
				   META_THEME_ACTIVE_SHADE_OPACITY_KEY,
				   NULL);

	if (!(cmdline_options & CMDLINE_ACTIVE_OPACITY_SHADE) &&
	    shade_opacity != settings->meta_active_shade_opacity)
	{
	    settings->meta_active_shade_opacity = shade_opacity;
	    changed = TRUE;
	}
    }

    return changed;
#else
    return FALSE;
#endif

}

static gboolean
button_layout_changed (GConfClient *client)
{

#ifdef USE_METACITY
    gchar *button_layout;

    button_layout = gconf_client_get_string (client,
					     META_BUTTON_LAYOUT_KEY,
					     NULL);

    if (button_layout)
    {
	meta_update_button_layout (button_layout);

	settings->meta_button_layout_set = TRUE;

	g_free (button_layout);

	return TRUE;
    }

    if (settings->meta_button_layout_set)
    {
	settings->meta_button_layout_set = FALSE;
	return TRUE;
    }
#endif

    return FALSE;
}

void
set_frame_scale (decor_frame_t *frame,
		 gchar	       *font_str)
{
    gfloat	  scale = 1.0f;

    gwd_decor_frame_ref (frame);

    if (frame->titlebar_font)
	pango_font_description_free (frame->titlebar_font);

    frame->titlebar_font = pango_font_description_from_string (font_str);

    scale = (*theme_get_title_scale) (frame);

    pango_font_description_set_size (frame->titlebar_font,
				     MAX (pango_font_description_get_size (frame->titlebar_font) * scale, 1));

    gwd_decor_frame_unref (frame);
}

void
set_frames_scales (gpointer key,
		   gpointer value,
		   gpointer user_data)
{
    decor_frame_t *frame = (decor_frame_t *) value;
    gchar	  *font_str = (gchar *) user_data;

    gwd_decor_frame_ref (frame);

    set_frame_scale (frame, font_str);

    gwd_decor_frame_unref (frame);
}

static void
titlebar_font_changed (GConfClient *client)
{
    gchar *str;

    str = gconf_client_get_string (client,
				   COMPIZ_TITLEBAR_FONT_KEY,
				   NULL);
    if (!str)
	str = g_strdup ("Sans Bold 12");

    if (settings->font)
    {
	g_free (settings->font);
	settings->font = g_strdup (str);
    }

    gwd_frames_foreach (set_frames_scales, (gpointer) settings->font);

    if (str)
	g_free (str);
}

static void
titlebar_click_action_changed (GConfClient *client,
			       const gchar *key,
			       int         *action_value,
			       int          default_value)
{
    gchar *action;

    *action_value = default_value;

    action = gconf_client_get_string (client, key, NULL);
    if (action)
    {
	if (strcmp (action, "toggle_shade") == 0)
	    *action_value = CLICK_ACTION_SHADE;
	else if (strcmp (action, "toggle_maximize") == 0)
	    *action_value = CLICK_ACTION_MAXIMIZE;
	else if (strcmp (action, "minimize") == 0)
	    *action_value = CLICK_ACTION_MINIMIZE;
	else if (strcmp (action, "raise") == 0)
	    *action_value = CLICK_ACTION_RAISE;
	else if (strcmp (action, "lower") == 0)
	    *action_value = CLICK_ACTION_LOWER;
	else if (strcmp (action, "menu") == 0)
	    *action_value = CLICK_ACTION_MENU;
	else if (strcmp (action, "none") == 0)
	    *action_value = CLICK_ACTION_NONE;

	g_free (action);
    }
}

static void
wheel_action_changed (GConfClient *client)
{
    gchar *action;

    settings->wheel_action = WHEEL_ACTION_DEFAULT;

    action = gconf_client_get_string (client, WHEEL_ACTION_KEY, NULL);
    if (action)
    {
	if (strcmp (action, "shade") == 0)
	    settings->wheel_action = WHEEL_ACTION_SHADE;
	else if (strcmp (action, "none") == 0)
	    settings->wheel_action = WHEEL_ACTION_NONE;

	g_free (action);
    }
}

static void
value_changed (GConfClient *client,
	       const gchar *key,
	       GConfValue  *value,
	       void        *data)
{
    gboolean changed = FALSE;

    if (strcmp (key, COMPIZ_USE_SYSTEM_FONT_KEY) == 0)
    {
	if (gconf_client_get_bool (client,
				   COMPIZ_USE_SYSTEM_FONT_KEY,
				   NULL) != settings->use_system_font)
	{
	    settings->use_system_font = !settings->use_system_font;
	    changed = TRUE;
	}
    }
    else if (strcmp (key, COMPIZ_TITLEBAR_FONT_KEY) == 0)
    {
	titlebar_font_changed (client);
	changed = !settings->use_system_font;
    }
    else if (strcmp (key, COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY) == 0)
    {
	titlebar_click_action_changed (client, key,
				       &settings->double_click_action,
				       DOUBLE_CLICK_ACTION_DEFAULT);
    }
    else if (strcmp (key, COMPIZ_MIDDLE_CLICK_TITLEBAR_KEY) == 0)
    {
	titlebar_click_action_changed (client, key,
				       &settings->middle_click_action,
				       MIDDLE_CLICK_ACTION_DEFAULT);
    }
    else if (strcmp (key, COMPIZ_RIGHT_CLICK_TITLEBAR_KEY) == 0)
    {
	titlebar_click_action_changed (client, key,
				       &settings->right_click_action,
				       RIGHT_CLICK_ACTION_DEFAULT);
    }
    else if (strcmp (key, WHEEL_ACTION_KEY) == 0)
    {
	wheel_action_changed (client);
    }
    else if (strcmp (key, BLUR_TYPE_KEY) == 0)
    {
	if (blur_settings_changed (client))
	    changed = TRUE;
    }
    else if (strcmp (key, USE_META_THEME_KEY) == 0 ||
	     strcmp (key, META_THEME_KEY) == 0)
    {
	if (theme_changed (client))
	    changed = TRUE;
    }
    else if (strcmp (key, META_BUTTON_LAYOUT_KEY) == 0)
    {
	if (button_layout_changed (client))
	    changed = TRUE;
    }
    else if (strcmp (key, META_THEME_OPACITY_KEY)	       == 0 ||
	     strcmp (key, META_THEME_SHADE_OPACITY_KEY)	       == 0 ||
	     strcmp (key, META_THEME_ACTIVE_OPACITY_KEY)       == 0 ||
	     strcmp (key, META_THEME_ACTIVE_SHADE_OPACITY_KEY) == 0)
    {
	if (theme_opacity_changed (client))
	    changed = TRUE;
    }

    if (changed)
	decorations_changed (data);
}
#endif

gboolean
init_settings (WnckScreen *screen)
{
#ifdef USE_GCONF
    GConfClient	   *gconf;

    gconf = gconf_client_get_default ();

    gconf_client_add_dir (gconf,
			  GCONF_DIR,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    gconf_client_add_dir (gconf,
			  METACITY_GCONF_DIR,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    g_signal_connect (G_OBJECT (gconf),
		      "value_changed",
		      G_CALLBACK (value_changed),
		      screen);
    settings->use_system_font = gconf_client_get_bool (gconf,
						       COMPIZ_USE_SYSTEM_FONT_KEY,
						       NULL);
    theme_changed (gconf);
    theme_opacity_changed (gconf);
    button_layout_changed (gconf);
    titlebar_font_changed (gconf);

    titlebar_click_action_changed (gconf,
				   COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY,
				   &settings->double_click_action,
				   DOUBLE_CLICK_ACTION_DEFAULT);
    titlebar_click_action_changed (gconf,
				   COMPIZ_MIDDLE_CLICK_TITLEBAR_KEY,
				   &settings->middle_click_action,
				   MIDDLE_CLICK_ACTION_DEFAULT);
    titlebar_click_action_changed (gconf,
				   COMPIZ_RIGHT_CLICK_TITLEBAR_KEY,
				   &settings->right_click_action,
				   RIGHT_CLICK_ACTION_DEFAULT);
    wheel_action_changed (gconf);
    blur_settings_changed (gconf);

    g_object_unref (gconf);
#endif
    
    shadow_property_changed (screen);

    return TRUE;
}
