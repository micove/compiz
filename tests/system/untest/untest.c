/*
 * Unredirect Fullscreen Windows Stress Tester
 *
 * Copyright (C) 2012  Canonical Ltd.
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
 
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <GL/glx.h>

#define SECONDS 1000
#define MINUTES 60000

#define FPS 30
#define CYCLE_PERIOD 2  /* seconds */
#define FULLSCREEN_PERIOD (3 * SECONDS)
#define DIALOG_PERIOD (7 * SECONDS)

#define PADDING 20

#define RED     {0, 0xffff, 0x0000, 0x0000}
#define YELLOW  {0, 0xffff, 0xffff, 0x0000}
#define GREEN   {0, 0x0000, 0xffff, 0x0000}
#define CYAN    {0, 0x0000, 0xffff, 0xffff}
#define BLUE    {0, 0x0000, 0x0000, 0xffff}
#define MAGENTA {0, 0xffff, 0x0000, 0xffff}

static const char *app_title = "Unredirect Fullscreen Windows Stress Tester";
static const char *copyright = "Copyright (c) 2012 Canonical Ltd.";
static gint test_duration = 120; /* seconds */
static guint64 start_time = 0;
static GLXWindow glxwin = None;
static GtkButton *button;

static void blend (guint16 scale, const GdkColor *a, const GdkColor *b,
                   GdkColor *c)
{
    unsigned long src = 0xffff - scale;
    unsigned long dest = scale;
    c->pixel = 0;
    c->red =   (guint16)((src * a->red +   dest * b->red)   / 0xffff);
    c->green = (guint16)((src * a->green + dest * b->green) / 0xffff);
    c->blue =  (guint16)((src * a->blue +  dest * b->blue)  / 0xffff);
}

static gboolean cycle_color (gpointer data)
{
    static const GdkColor color_stop[] =
        {RED, YELLOW, GREEN, CYAN, BLUE, MAGENTA};
    static const int color_stops = sizeof(color_stop)/sizeof(color_stop[0]);
    static guint16 pos = 0;
    guint16 scale;
    GtkWindow *win = GTK_WINDOW (data);
    int from, to;
    GdkColor color;

    pos = (pos + 0xffff / (FPS * CYCLE_PERIOD)) & 0xffff;
    from = (pos * color_stops) >> 16;
    to = (from + 1) % color_stops;
    scale = (pos * color_stops) & 0xffff;
    blend (scale, color_stop+from, color_stop+to, &color);

    if (glxwin == None)
    {
        gtk_widget_modify_bg (GTK_WIDGET (win), GTK_STATE_NORMAL, &color);
    }
    else
    {
        glClearColor (color.red / 65535.0f,
                      color.green / 65535.0f,
                      color.blue / 65535.0f,
                      1.0);
        glClear (GL_COLOR_BUFFER_BIT);
        glFinish ();
    }

    return TRUE;
}

static gboolean on_redraw (GtkWidget *widget, void *cr, gpointer data)
{
    GtkProgressBar *b = GTK_PROGRESS_BAR (data);
    guint64 elapsed = g_get_real_time () - start_time;
    gtk_progress_bar_set_fraction (b,
                                   (float)elapsed / (1000000 * test_duration));
    g_print ("\r%d%% ", (int)(elapsed / (10000 * test_duration)));
    return FALSE;
}

static gboolean toggle_fullscreen (gpointer data)
{
    static gboolean is_fullscreen = FALSE;
    GtkWindow *w = GTK_WINDOW (data);
    is_fullscreen = !is_fullscreen;
    if (is_fullscreen)
        gtk_window_fullscreen (w);
    else
        gtk_window_unfullscreen (w);
    return TRUE;
}

static gboolean toggle_dialog (gpointer data)
{
    static gboolean visible = FALSE;
    GtkWindow *dialog = GTK_WINDOW (data);
    visible = !visible;
    gtk_widget_set_visible (GTK_WIDGET (dialog), visible);
    return TRUE;
}

static void close_window (GtkWidget *widget, gpointer user_data) 
{
    gtk_main_quit ();
    g_print ("\rYou're a quitter. No test results for you.\n");
}

static gboolean end_test (gpointer data)
{
    gtk_main_quit ();
    g_print ("\rCongratulations! Nothing crashed.\n");
    return FALSE;
}

static gboolean init_opengl (GtkWindow *win)
{
    gboolean ret = FALSE;
    static const int attr[] =
    {
        GLX_X_RENDERABLE, True,
        GLX_DOUBLEBUFFER, False,
        None
    };
    Display *dpy = gdk_x11_get_default_xdisplay ();
    int screen = gdk_x11_get_default_screen ();
    int nfb;
    GLXFBConfig *fb = glXChooseFBConfig (dpy, screen, attr, &nfb);

    if (fb != NULL && nfb > 0)
    {
        GdkWindow *gdkwin = gtk_widget_get_window (GTK_WIDGET (win));
        Window xwin = gdk_x11_window_get_xid (gdkwin);
        glxwin = glXCreateWindow (dpy, fb[0], xwin, NULL);
        if (glxwin != None)
        {
            GLXContext ctx = glXCreateNewContext (dpy, fb[0], GLX_RGBA_TYPE,
                                                  NULL, True);
            if (ctx != NULL)
            {
                if (glXMakeContextCurrent (dpy, glxwin, glxwin, ctx))
                {
                    const char *vendor = (const char*)
                                         glGetString (GL_VENDOR);
                    const char *renderer = (const char*)
                                           glGetString (GL_RENDERER);
                    const char *version = (const char*)
                                          glGetString (GL_VERSION);
                    g_print ("GL Vendor: %s\n"
                             "GL Renderer: %s\n"
                             "GL Version: %s\n"
                             "\n",
                             vendor != NULL ? vendor : "?",
                             renderer != NULL ? renderer : "?",
                             version != NULL ? version : "?");
                    ret = TRUE;
                }
                else
                {
                    g_warning ("glXMakeContextCurrent failed. "
                               "OpenGL won't be used.\n");
                    
                }
            }
        }
        else
        {
            g_warning ("glXCreateWindow failed. OpenGL won't be used.\n");
        }
        XFree (fb);
    }
    else
    {
        g_warning ("glXChooseFBConfig returned nothing. "
                   "OpenGL won't be used.\n");
    }

    return ret;
}

int main (int argc, char *argv[])
{
    static gboolean gl_disabled = FALSE;
    static GOptionEntry custom_options[] =
    {
        {
            "disable-opengl",
            'd',
            0,
            G_OPTION_ARG_NONE,
            &gl_disabled,
            "Disable OpenGL rendering. Use GTK only.",
            NULL
        },
        {
            "test-duration",
            't',
            0,
            G_OPTION_ARG_INT,
            &test_duration,
            "How long the test lasts (default 120 seconds)",
            "SECONDS"
        },
        {
            NULL,
            0,
            0,
            G_OPTION_ARG_NONE,
            NULL,
            NULL,
            NULL
        }
    };
    GtkWindow *win;
    GtkWindow *dialog;
    GtkLabel *label;
    GtkBox *vbox;
    GtkBox *hbox;
    GtkAlignment *align;
    GtkProgressBar *bar;

    g_print ("%s\n"
             "%s\n"
             "\n",
             app_title, copyright);

    if (!gtk_init_with_args (&argc, &argv, NULL, custom_options, NULL, NULL))
    {
        g_warning ("Invalid options? Try: %s --help\n", argv[0]);
        return 1;
    }

    win = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title (win, app_title);
    gtk_window_set_default_size (win, 300, 300);
    gtk_window_set_position (win, GTK_WIN_POS_CENTER);
    gtk_widget_show (GTK_WIDGET (win));

    align = GTK_ALIGNMENT (gtk_alignment_new (0.5f, 1.0f, 0.5f, 0.1f));
    gtk_container_add (GTK_CONTAINER (win), GTK_WIDGET (align));
    gtk_widget_show (GTK_WIDGET (align));

    vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, PADDING));
    gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (vbox));
    gtk_widget_show (GTK_WIDGET (vbox));

    hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, PADDING));
    gtk_box_set_homogeneous (hbox, FALSE);
    gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (hbox));
    gtk_widget_show (GTK_WIDGET (hbox));
     
    bar = GTK_PROGRESS_BAR (gtk_progress_bar_new ());
    gtk_progress_bar_set_text (bar, NULL);
    gtk_progress_bar_set_show_text (bar, TRUE);
    gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (bar));
    gtk_box_set_child_packing (hbox, GTK_WIDGET (bar), TRUE, TRUE, 0,
                               GTK_PACK_START);
    gtk_widget_show (GTK_WIDGET (bar));

    button = GTK_BUTTON (gtk_button_new_with_label ("Cancel"));
    gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (button));
    gtk_box_set_child_packing (hbox, GTK_WIDGET (button), FALSE, FALSE, 0,
                               GTK_PACK_END);
    gtk_widget_show (GTK_WIDGET (button));

    dialog = GTK_WINDOW (gtk_window_new (GTK_WINDOW_POPUP));
    gtk_window_set_transient_for (dialog, win);
    gtk_window_set_default_size (dialog, 100, 100);
    gtk_window_set_position (dialog, GTK_WIN_POS_CENTER);

    label = GTK_LABEL (gtk_label_new ("Popup!"));
    gtk_label_set_justify (label, GTK_JUSTIFY_CENTER);
    gtk_container_add (GTK_CONTAINER (dialog), GTK_WIDGET (label));
    gtk_widget_show (GTK_WIDGET (label));

    gtk_widget_realize (GTK_WIDGET (win));
    if (!gl_disabled)
        init_opengl (win);

    /* XXX For now, hide the widgets in OpenGL mode. People will think the
           flickering is a bug */
    if (glxwin)
        gtk_widget_hide (GTK_WIDGET (align));

    g_timeout_add (1000 / FPS, cycle_color, win);
    g_timeout_add (FULLSCREEN_PERIOD, toggle_fullscreen, win);
    g_timeout_add (test_duration * SECONDS, end_test, NULL);
    g_timeout_add (DIALOG_PERIOD, toggle_dialog, dialog);

    g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (close_window),
                      NULL);
    g_signal_connect (G_OBJECT (win), "draw", G_CALLBACK (on_redraw), bar);
    g_signal_connect (G_OBJECT (win), "destroy", G_CALLBACK (close_window),
                      NULL);

    start_time = g_get_real_time ();
    gtk_main ();

    return 0;
}
