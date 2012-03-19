#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "local-menus.h"
#include <X11/Xlib.h>
#include <gio/gio.h>
#include <sys/poll.h>
#include <X11/Xatom.h>

class GtkWindowDecoratorTestLocalMenu :
    public ::testing::Test
{
    public:

#ifdef META_HAS_LOCAL_MENUS
	Window getWindow () { return mXWindow; }
	GSettings  * getSettings () { return mSettings; }
	virtual void SetUp ()
	{
	    gtk_init (NULL, NULL);

	    mXDisplay = XOpenDisplay (NULL);

	    XSelectInput (mXDisplay, DefaultRootWindow (mXDisplay), PropertyChangeMask);

	    mXWindow = XCreateSimpleWindow (mXDisplay, DefaultRootWindow (mXDisplay), 0, 0, 100, 100, 0, 0, 0);

	    Atom net_client_list = XInternAtom (mXDisplay, "_NET_CLIENT_LIST", FALSE);

	    XMapRaised (mXDisplay, mXWindow);

	    XFlush (mXDisplay);

	    /* Wait for _NET_CLIENT_LIST to be updated with this window */
	    while (1)
	    {
		struct pollfd pfd;

		pfd.events = POLLIN;
		pfd.revents = 0;
		pfd.fd = ConnectionNumber (mXDisplay);

		XEvent event;

		poll (&pfd, 1, -1);

		XNextEvent (mXDisplay, &event);

		gboolean foundWindow = FALSE;

		switch (event.type)
		{
		    case PropertyNotify:
		    {
			if (event.xproperty.atom == net_client_list)
			{
			    Atom type;
			    int  fmt;
			    unsigned long nitems, nleft;
			    unsigned char *prop;

			    XGetWindowProperty (mXDisplay, event.xproperty.window, net_client_list,
						0L, 128L, FALSE, XA_WINDOW, &type, &fmt, &nitems, &nleft, &prop);

			    if (fmt == 32 && type == XA_WINDOW && nitems && !nleft)
			    {
				Window *windows = (Window *) prop;

				while (nitems--)
				{
				    if (*(windows++) == mXWindow)
					foundWindow = TRUE;
				}
			    }

			    XFree (prop);
			}

			break;
		    }
		    default:
			break;
		}

		if (foundWindow)
		    break;
	    }

	    g_setenv("GSETTINGS_BACKEND", "memory", true);
	    mSettings = g_settings_new ("com.canonical.indicator.appmenu");
	}

	virtual void TearDown ()
	{
	    XDestroyWindow (mXDisplay, mXWindow);
	    XCloseDisplay (mXDisplay);

	    g_object_unref (mSettings);
	}
#endif

    private:

	Window     mXWindow;
	Display    *mXDisplay;
	GSettings *mSettings;
};
