/*
 * Copyright © 2006 Novell, Inc.
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

#include "water_options.h"

#define WATER_SCREEN(s) \
    WaterScreen *ws = WaterScreen::get (s)

#define TEXTURE_SIZE 256

#define K 0.1964f

#define TEXTURE_NUM 3

#define TINDEX(ws, i) (((ws)->tIndex + (i)) % TEXTURE_NUM)

#define WATER_INITIATE_MODIFIERS_DEFAULT (ControlMask | CompSuperMask)

struct WaterFunction {
    GLFragment::FunctionId id;

    int target;
    int param;
    int unit;
};

class WaterScreen :
    public ScreenInterface,
    public CompositeScreenInterface,
    public PluginClassHandler<WaterScreen,CompScreen>,
    public WaterOptions
{
    public:
	WaterScreen (CompScreen *screen);
	~WaterScreen ();

	void optionChange (WaterOptions::Options num);

	void handleEvent (XEvent *);

	void preparePaint (int);
	void donePaint ();

	GLFragment::FunctionId
	getBumpMapFragmentFunction (GLTexture *texture,
				    int       unit,
				    int       param);

	void allocTexture (int index);

	bool fboPrologue (int tIndex);
	void fboEpilogue ();
	bool fboUpdate (float dt, float fade);
	bool fboVertices (GLenum type, XPoint *p, int n, float v);

	void softwareUpdate (float dt, float fade);
	void softwarePoints (XPoint *p, int n, float add);
	void softwareLines (XPoint *p, int n, float v);
	void softwareVertices (GLenum type, XPoint *p, int n, float v);

	void waterUpdate (float dt);
	void scaleVertices (XPoint *p, int n);
	void waterVertices (GLenum type, XPoint *p, int n, float v);

	bool rainTimeout ();
	bool wiperTimeout ();

	void waterReset ();

	void handleMotionEvent ();

	CompositeScreen *cScreen;
	GLScreen        *gScreen;

	float offsetScale;

	CompScreen::GrabHandle grabIndex;

	int width, height;

	GLuint program;
	GLuint texture[TEXTURE_NUM];

	int     tIndex;
	GLenum  target;
	GLfloat tx, ty;

	int count;

	GLuint fbo;
	GLint  fboStatus;

	void          *data;
	float         *d0;
	float         *d1;
	unsigned char *t0;

	CompTimer rainTimer;
	CompTimer wiperTimer;

	float wiperAngle;
	float wiperSpeed;

	std::vector<WaterFunction> bumpMapFunctions;
};

class WaterWindow :
    public GLWindowInterface,
    public PluginClassHandler<WaterWindow,CompWindow>
{
    public:
	WaterWindow (CompWindow *window) :
	    PluginClassHandler<WaterWindow,CompWindow> (window),
	    window (window),
	    gWindow (GLWindow::get (window)),
	    wScreen (WaterScreen::get (screen)),
	    gScreen (GLScreen::get (screen))
	{
	    GLWindowInterface::setHandler (gWindow, false);
	}

	void glDrawTexture (GLTexture *texture, GLFragment::Attrib &,
			    unsigned int);

	CompWindow  *window;
	GLWindow    *gWindow;
	WaterScreen *wScreen;
	GLScreen    *gScreen;
};

class WaterPluginVTable :
    public CompPlugin::VTableForScreenAndWindow<WaterScreen,WaterWindow>
{
    public:

	bool init ();
};
