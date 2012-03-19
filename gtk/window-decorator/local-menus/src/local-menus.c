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

#include <string.h>
#include "local-menus.h"
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>

#define GLOBAL 0
#define LOCAL 1

#define ALLOWED 2
#define NOT_ALLOWED 1

gint menu_mode = GLOBAL;

GDBusProxy *global_lim_listener;


#ifdef META_HAS_LOCAL_MENUS
static void
gwd_menu_mode_changed (GSettings *settings,
		       gchar     *key,
		       gpointer  user_data)
{
    menu_mode = g_settings_get_enum (settings, "menu-mode");
}
#endif

active_local_menu *active_menu;
pending_local_menu *pending_menu;

GHashTable *get_windows_with_menus_table ()
{
   static GHashTable *windows_with_menus = NULL;

   if (!windows_with_menus)
     windows_with_menus = g_hash_table_new (NULL, NULL);

   return windows_with_menus;
}

static gboolean read_xprop_for_window (Display *dpy, Window xid)
{
  Atom ubuntu_appmenu_unique_name = XInternAtom (dpy, "_UBUNTU_APPMENU_UNIQUE_NAME", FALSE);
  Atom utf8_string = XInternAtom (dpy, "UTF8_STRING", FALSE);
  Atom actual;
  int  fmt;
  unsigned long nitems, nleft;
  unsigned char *prop;
  XGetWindowProperty (dpy, xid, ubuntu_appmenu_unique_name,
		      0L, 16L, FALSE, utf8_string, &actual, &fmt, &nitems, &nleft, &prop);

  if (actual == utf8_string && fmt == 8 && nitems > 1)
  {
    g_hash_table_replace (get_windows_with_menus_table (), GINT_TO_POINTER (xid), GINT_TO_POINTER (ALLOWED));
    return TRUE;
  }
  else
  {
    g_hash_table_replace (get_windows_with_menus_table (), GINT_TO_POINTER (xid), GINT_TO_POINTER (NOT_ALLOWED));
    return FALSE;
  }
}

gboolean
local_menu_allowed_on_window (Display *dpy, Window xid)
{
  gpointer local_menu_allowed_found = g_hash_table_lookup (get_windows_with_menus_table (), GINT_TO_POINTER (xid));

  if (local_menu_allowed_found)
    {
      return GPOINTER_TO_INT (local_menu_allowed_found) == ALLOWED;
    }
  else if (dpy && xid)
    {
      return read_xprop_for_window (dpy, xid);
    }

  return FALSE;
}

gboolean
gwd_window_should_have_local_menu (Window win)
{
#ifdef META_HAS_LOCAL_MENUS
    const gchar * const *schemas = g_settings_list_schemas ();
    static GSettings *lim_settings = NULL;
    while (*schemas != NULL && !lim_settings)
    {
	if (g_str_equal (*schemas, "com.canonical.indicator.appmenu"))
	{
	    lim_settings = g_settings_new ("com.canonical.indicator.appmenu");
	    menu_mode = g_settings_get_enum (lim_settings, "menu-mode");
	    g_signal_connect (lim_settings, "changed::menu-mode", G_CALLBACK (gwd_menu_mode_changed), NULL);
	    break;
	}
	++schemas;
    }

    if (lim_settings && win)
	return menu_mode == LOCAL && local_menu_allowed_on_window (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), win);
#endif

    return FALSE;
}

gchar *
gwd_get_entry_id_from_sync_variant (GVariant *args)
{
    /* We need to get the indicator data once we've called show */

    GVariantIter* iter            = NULL;
    gchar*        name_hint       = NULL;
    gchar*        indicator_id    = NULL;
    gchar*        entry_id        = NULL;
    gchar*        label           = NULL;
    gboolean      label_sensitive = FALSE;
    gboolean      label_visible   = FALSE;
    guint32       image_type      = 0;
    gchar*        image_data      = NULL;
    gboolean      image_sensitive = FALSE;
    gboolean      image_visible   = FALSE;
    gint32        priority        = -1;

    g_variant_get (args, "(a(ssssbbusbbi))", &iter);
    while (g_variant_iter_loop (iter, "(ssssbbusbbi)",
			    &indicator_id,
			    &entry_id,
			    &name_hint,
			    &label,
			    &label_sensitive,
			    &label_visible,
			    &image_type,
			    &image_data,
			    &image_sensitive,
			    &image_visible,
			    &priority))
    {
	g_variant_unref (args);
	return g_strdup (entry_id);
    }

    g_variant_unref (args);
    g_assert_not_reached ();
    return NULL;
}
#ifdef META_HAS_LOCAL_MENUS
static void
on_local_menu_activated (GDBusProxy *proxy,
			 gchar      *sender_name,
			 gchar      *signal_name,
			 GVariant   *parameters,
			 gpointer   user_data)
{

    if (g_strcmp0 (signal_name, "EntryActivated") == 0)
    {
	show_local_menu_data *d = (show_local_menu_data *) user_data;
	gchar *entry_id = NULL;
	gint            x_out, y_out;
	guint           width, height;

	g_variant_get (parameters, "(s(iiuu))", &entry_id, &x_out, &y_out, &width, &height, NULL);

	if (!d->local_menu_entry_id)
	{
	    GError   *error = NULL;
	    GVariant *params = g_variant_new ("(s)", "libappmenu.so", NULL);
	    GVariant *args = g_dbus_proxy_call_sync (proxy, "SyncOne", params, 0, 500, NULL, &error);

	    g_assert_no_error (error);
	    d->local_menu_entry_id = gwd_get_entry_id_from_sync_variant (args);
	}

	if (g_strcmp0 (entry_id, d->local_menu_entry_id) == 0)
	{
	    d->rect->x = x_out - d->dx;
	    d->rect->y = y_out - d->dy;
	    d->rect->width = width;
	    d->rect->height = height;
	    (*d->cb) (d->user_data);
	}
	else if (g_strcmp0 (entry_id, "") == 0)
	{
	    memset (d->rect, 0, sizeof (GdkRectangle));
	    (*d->cb) (d->user_data);

	    g_signal_handlers_disconnect_by_func (proxy, on_local_menu_activated, d);

	    if (active_menu)
	    {
		g_free (active_menu);
		active_menu = NULL;
	    }

	    g_free (d->local_menu_entry_id);
	    g_free (d);
	}
    }

}
#endif
gboolean
gwd_move_window_instead (gpointer user_data)
{
    (*pending_menu->cb) (pending_menu->user_data);
    g_source_remove (pending_menu->move_timeout_id);
    g_free (pending_menu->user_data);
    g_free (pending_menu);
    pending_menu = NULL;
    return FALSE;
}

void
local_menu_process_motion(gint x_root, gint y_root)
{
    if (!pending_menu)
	return;

    if (abs (pending_menu->x_root - x_root) > 4 &&
	abs (pending_menu->y_root - y_root) > 4)
	gwd_move_window_instead (pending_menu);
}

void
gwd_prepare_show_local_menu (start_move_window_cb start_move_window,
			     gpointer user_data_start_move_window,
			     gint     x_root,
			     gint     y_root)
{
    if (pending_menu)
    {
	g_source_remove (pending_menu->move_timeout_id);
	g_free (pending_menu->user_data);
	g_free (pending_menu);
	pending_menu = NULL;
    }

    pending_menu = g_new0 (pending_local_menu, 1);
    pending_menu->cb = start_move_window;
    pending_menu->user_data = user_data_start_move_window;
    pending_menu->move_timeout_id = g_timeout_add (150, gwd_move_window_instead, pending_menu);
}

#ifdef META_HAS_LOCAL_MENUS
void
local_menu_entry_activated_request (GDBusProxy *proxy,
				    gchar      *sender_name,
				    gchar      *signal_name,
				    GVariant   *parameters,
				    gpointer   user_data)
{
  if (g_strcmp0 (signal_name, "EntryActivateRequest") == 0)
    {
      gchar *activated_entry_id;
      gchar *local_menu_entry_id;
      local_menu_entry_activated_request_funcs *funcs = (local_menu_entry_activated_request_funcs *) user_data;

      g_variant_get (parameters, "(s)", &activated_entry_id, NULL);

      GError   *error = NULL;
      GVariant *params = g_variant_new ("(s)", "libappmenu.so", NULL);
      GVariant *args = g_dbus_proxy_call_sync (proxy, "SyncOne", params, 0, 500, NULL, &error);

      g_assert_no_error (error);
      local_menu_entry_id = gwd_get_entry_id_from_sync_variant (args);

      if (g_strcmp0 (activated_entry_id, local_menu_entry_id) == 0)
	{
	  WnckScreen *screen = wnck_screen_get_for_root (gdk_x11_get_default_root_xwindow ());
	  int dx, dy, top_height;
	  Window xid;

	  if (screen)
	    {
	      Box *rect = (*funcs->active_window_local_menu_rect_callback) ((gpointer) screen, &dx, &dy, &top_height, &xid);

	      if (rect)
		{
		  int x = rect->x1;
		  int y = top_height;

		  gwd_show_local_menu (gdk_x11_display_get_xdisplay (gdk_display_get_default ()),
					   xid, x + dx, y + dy, x, y, 0, 0,
					   funcs->show_window_menu_hidden_callback, GINT_TO_POINTER (xid));
		}
	    }
	}
    }
}
#endif

gboolean
gwd_show_local_menu (Display *xdisplay,
		     Window  frame_xwindow,
		     int      x,
		     int      y,
		     int      x_win,
		     int      y_win,
		     int      button,
		     guint32  timestamp,
		     show_window_menu_hidden_cb cb,
		     gpointer user_data_show_window_menu)
{
    if (pending_menu)
    {
	g_source_remove (pending_menu->move_timeout_id);
	g_free (pending_menu->user_data);
	g_free (pending_menu);
	pending_menu = NULL;
    }

#ifdef META_HAS_LOCAL_MENUS


    XUngrabPointer (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), CurrentTime);
    XUngrabKeyboard (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), CurrentTime);
    XSync (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), FALSE);

    GDBusProxy *proxy = global_lim_listener;

    if (proxy)
    {
	GVariant *message = g_variant_new ("(uiiu)", frame_xwindow, x, y, time);
	GError   *error = NULL;
	g_dbus_proxy_call_sync (proxy, "ShowAppMenu", message, 0, 500, NULL, &error);
	if (error)
	{
	    g_print ("error calling ShowAppMenu: %s\n", error->message);
	    return FALSE;
	}

	show_local_menu_data *data = g_new0 (show_local_menu_data, 1);

	if (active_menu)
	    g_free (active_menu);

	active_menu = g_new0 (active_local_menu, 1);

	data->cb = cb;
	data->user_data = user_data_show_window_menu;
	data->rect = &active_menu->rect;
	data->dx = x - x_win;
	data->dy = y - y_win;
	data->local_menu_entry_id = NULL;  

	g_signal_connect (proxy, "g-signal", G_CALLBACK (on_local_menu_activated), data);

	return TRUE;
    }
#endif
    return FALSE;
}

void
force_local_menus_on (Window           win,
		      MetaButtonLayout *button_layout)
{
#ifdef META_HAS_LOCAL_MENUS
    if (gwd_window_should_have_local_menu (win))
    {
	if (button_layout->left_buttons[0] != META_BUTTON_FUNCTION_LAST)
	{
	    unsigned int i = 0;
	    for (; i < MAX_BUTTONS_PER_CORNER; i++)
	    {
		if (button_layout->left_buttons[i] == META_BUTTON_FUNCTION_WINDOW_MENU)
		    break;
		else if (button_layout->left_buttons[i] == META_BUTTON_FUNCTION_LAST)
		{
		    if ((i + 1) < MAX_BUTTONS_PER_CORNER)
		    {
			button_layout->left_buttons[i + 1] = META_BUTTON_FUNCTION_LAST;
			button_layout->left_buttons[i] = META_BUTTON_FUNCTION_WINDOW_MENU;
			break;
		    }
		}
	    }
	}
	if (button_layout->right_buttons[0] != META_BUTTON_FUNCTION_LAST)
	{
	    unsigned int i = 0;
	    for (; i < MAX_BUTTONS_PER_CORNER; i++)
	    {
		if (button_layout->right_buttons[i] == META_BUTTON_FUNCTION_WINDOW_MENU)
		    break;
		else if (button_layout->right_buttons[i] == META_BUTTON_FUNCTION_LAST)
		{
		    if ((i + 1) < MAX_BUTTONS_PER_CORNER)
		    {
			button_layout->right_buttons[i + 1] = META_BUTTON_FUNCTION_LAST;
			button_layout->right_buttons[i] = META_BUTTON_FUNCTION_WINDOW_MENU;
			break;
		    }
		}
	    }
	}
    }
#endif
}

void
local_menu_cache_notify_window_destroyed (Window xid)
{
  g_hash_table_remove (get_windows_with_menus_table (), GINT_TO_POINTER (xid));
}

void
local_menu_cache_reload_xwindow (Display *dpy, Window xid)
{
  read_xprop_for_window (dpy, xid);
}


