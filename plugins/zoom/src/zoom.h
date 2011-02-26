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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <X11/cursorfont.h>

#include <core/core.h>
#include <core/pluginclasshandler.h>

#include <composite/composite.h>
#include <opengl/opengl.h>

#include "zoom_options.h"

#define ZOOM_SCREEN(s)						        \
    ZoomScreen *zs = ZoomScreen::get (s)

struct ZoomBox {
    float x1;
    float y1;
    float x2;
    float y2;
};

class ZoomScreen :
    public ScreenInterface,
    public CompositeScreenInterface,
    public GLScreenInterface,
    public PluginClassHandler<ZoomScreen,CompScreen>,
    public ZoomOptions
{
    public:

	ZoomScreen (CompScreen *screen);
	~ZoomScreen ();

	void handleEvent (XEvent *);

	void preparePaint (int);
	void donePaint ();

	bool glPaintOutput (const GLScreenPaintAttrib &,
			    const GLMatrix &, const CompRegion &,
			    CompOutput *, unsigned int);

	void getCurrentZoom (int output, ZoomBox *pBox);
	void handleMotionEvent (int xRoot, int yRoot);
	void initiateForSelection (int output);

	void zoomInEvent ();
	void zoomOutEvent ();

	CompositeScreen *cScreen;
	GLScreen        *gScreen;

	float pointerSensitivity;

	CompScreen::GrabHandle grabIndex;
	bool grab;

	int zoomed;

	bool adjust;

	CompScreen::GrabHandle panGrabIndex;
	Cursor panCursor;

	GLfloat velocity;
	GLfloat scale;

	ZoomBox current[16];
	ZoomBox last[16];

	int x1, y1, x2, y2;

	int zoomOutput;
};

class ZoomPluginVTable :
    public CompPlugin::VTableForScreen<ZoomScreen>
{
    public:

	bool init ();
};
