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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <compiz.h>

char *programName;
char **programArgv;
int  programArgc;

char *backgroundImage = NULL;

REGION   emptyRegion;
REGION   infiniteRegion;
GLushort defaultColor[4] = { 0xffff, 0xffff, 0xffff, 0xffff };
Window   currentRoot = 0;

int  defaultRefreshRate = 50;
char *defaultTextureFilter = "Good";

char *windowTypeString[] = {
    N_("Desktop"),
    N_("Dock"),
    N_("Toolbar"),
    N_("Menu"),
    N_("Utility"),
    N_("Splash"),
    N_("Dialog"),
    N_("Normal"),
    N_("DropdownMenu"),
    N_("PopupMenu"),
    N_("Tooltip"),
    N_("Notification"),
    N_("Combo"),
    N_("Dnd"),
    N_("ModalDialog"),
    N_("Fullscreen"),
    N_("Unknown")
};
int  nWindowTypeString =
    sizeof (windowTypeString) / sizeof (windowTypeString[0]);

Bool shutDown = FALSE;
Bool restartSignal = FALSE;

CompWindow *lastFoundWindow = 0;
CompWindow *lastDamagedWindow = 0;

Bool replaceCurrentWm = FALSE;
Bool indirectRendering = FALSE;
Bool strictBinding = TRUE;
Bool noDetection = FALSE;
Bool useDesktopHints = TRUE;
Bool onlyCurrentScreen = FALSE;

#ifdef USE_COW
Bool useCow = TRUE;
#endif

static void
usage (void)
{
    printf ("Usage: %s "
	    "[--display DISPLAY] "
	    "[--bg-image PNG] "
	    "[--refresh-rate RATE]\n       "
	    "[--fast-filter] "
	    "[--indirect-rendering] "
	    "[--loose-binding] "
	    "[--replace]\n       "
	    "[--sm-disable] "
	    "[--sm-client-id ID] "
	    "[--no-detection]\n       "
	    "[--ignore-desktop-hints] "
	    "[--only-current-screen]"

#ifdef USE_COW
	    " [--use-root-window]\n       "
#else
	    "\n       "
#endif

	    "[--version] "
	    "[--help] "
	    "[PLUGIN]...\n",
	    programName);
}

static void
signalHandler (int sig)
{
    int status;

    switch (sig) {
    case SIGCHLD:
	waitpid (-1, &status, WNOHANG | WUNTRACED);
	break;
    case SIGHUP:
	restartSignal = TRUE;
	break;
    case SIGINT:
    case SIGTERM:
	shutDown = TRUE;
    default:
	break;
    }
}

int
main (int argc, char **argv)
{
    char *displayName = 0;
    char *plugin[256];
    int  i, nPlugin = 0;
    Bool disableSm = FALSE;
    char *clientId = NULL;

    programName = argv[0];
    programArgc = argc;
    programArgv = argv;

    signal (SIGHUP, signalHandler);
    signal (SIGCHLD, signalHandler);
    signal (SIGINT, signalHandler);
    signal (SIGTERM, signalHandler);

    emptyRegion.rects = &emptyRegion.extents;
    emptyRegion.numRects = 0;
    emptyRegion.extents.x1 = 0;
    emptyRegion.extents.y1 = 0;
    emptyRegion.extents.x2 = 0;
    emptyRegion.extents.y2 = 0;
    emptyRegion.size = 0;

    infiniteRegion.rects = &infiniteRegion.extents;
    infiniteRegion.numRects = 1;
    infiniteRegion.extents.x1 = MINSHORT;
    infiniteRegion.extents.y1 = MINSHORT;
    infiniteRegion.extents.x2 = MAXSHORT;
    infiniteRegion.extents.y2 = MAXSHORT;

    for (i = 1; i < argc; i++)
    {
	if (!strcmp (argv[i], "--help"))
	{
	    usage ();
	    return 0;
	}
	else if (!strcmp (argv[i], "--version"))
	{
	    printf (PACKAGE_STRING "\n");
	    return 0;
	}
	else if (!strcmp (argv[i], "--display"))
	{
	    if (i + 1 < argc)
		displayName = argv[++i];
	}
	else if (!strcmp (argv[i], "--refresh-rate"))
	{
	    if (i + 1 < argc)
	    {
		defaultRefreshRate = atoi (programArgv[++i]);
		defaultRefreshRate = RESTRICT_VALUE (defaultRefreshRate,
						     1, 1000);
	    }
	}
	else if (!strcmp (argv[i], "--fast-filter"))
	{
	    defaultTextureFilter = "Fast";
	}
	else if (!strcmp (argv[i], "--indirect-rendering"))
	{
	    indirectRendering = TRUE;
	}
	else if (!strcmp (argv[i], "--loose-binding"))
	{
	    strictBinding = FALSE;
	}
	else if (!strcmp (argv[i], "--ignore-desktop-hints"))
	{
	    useDesktopHints = FALSE;
	}
	else if (!strcmp (argv[i], "--only-current-screen"))
	{
	    onlyCurrentScreen = TRUE;
	}

#ifdef USE_COW
	else if (!strcmp (argv[i], "--use-root-window"))
	{
	    useCow = FALSE;
	}
#endif

	else if (!strcmp (argv[i], "--replace"))
	{
	    replaceCurrentWm = TRUE;
	}
	else if (!strcmp (argv[i], "--sm-disable"))
	{
	    disableSm = TRUE;
	}
	else if (!strcmp (argv[i], "--sm-client-id"))
	{
	    if (i + 1 < argc)
		clientId = argv[++i];
	}
	else if (!strcmp (argv[i], "--no-detection"))
	{
	    noDetection = TRUE;
	}
	else if (!strcmp (argv[i], "--bg-image"))
	{
	    if (i + 1 < argc)
		backgroundImage = argv[++i];
	}
	else if (*argv[i] == '-')
	{
	    fprintf (stderr, "%s: Unknown option '%s'\n", programName, argv[i]);
	}
	else
	{
	    if (nPlugin < 256)
		plugin[nPlugin++] = argv[i];
	}
    }

    if (!disableSm)
	initSession (clientId);

    if (!addDisplay (displayName, plugin, nPlugin))
	return 1;

    eventLoop ();

    if (!disableSm)
	closeSession ();

    if (restartSignal)
    {
	execvp (programName, programArgv);
	return 1;
    }

    return 0;
}
