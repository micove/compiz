#include "qdecorator.h"
#include "qwmutils.h"

#include <QtGui>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

//#define I_KNOW_WHAT_I_AM_DOING

static bool check_dm_hint ()
{
    Window	  xroot;
    Display	  *xdisplay = QX11Info::display();
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    Atom	  atom;
    bool	  dm = false;

    xroot = RootWindow(xdisplay, QX11Info::appScreen());

    atom = XInternAtom(xdisplay, "_NET_SUPPORTING_DM_CHECK", false);

    result = XGetWindowProperty(xdisplay, xroot,
                                atom, 0L, 1L, FALSE,
                                XA_WINDOW, &actual, &format,
                                &n, &left, &data);

    if (result == Success && n && data)
    {
	XWindowAttributes attr;
	Window		  window;

	memcpy(&window, data, sizeof (Window));

	XFree(data);

	QWM::trapXError();

	XGetWindowAttributes(xdisplay, window, &attr);
	XSync(xdisplay, false);

	if (!QWM::popXError())
            dm = true;
    }

    return dm;
}


int main(int argc, char **argv)
{
    QDecorator app(argc, argv);

    if (check_dm_hint())
    {
	fprintf(stderr, "%s: Another window decorator is already running\n",
		argv[0]);
	return 1;
    }

#ifndef I_KNOW_WHAT_I_AM_DOING
    fprintf(stderr, "Qt decorator is not yet ready!\n");
    exit(1);
#endif

    return app.exec();
}
