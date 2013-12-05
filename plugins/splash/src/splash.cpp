/*
 * Compiz splash plugin
 *
 * splash.cpp
 *
 * Copyright : (C) 2006 by Dennis Kasprzyk
 * E-mail    : onestone@beryl-project.org
 *
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
 */

#include "splash.h"

COMPIZ_PLUGIN_20090315 (splash, SplashPluginVTable);

const std::string SPLASH_BACKGROUND_DEFAULT("");
const std::string SPLASH_LOGO_DEFAULT("");

void
SplashScreen::preparePaint (int ms)
{
    bool lastShot = false;

    fade_in -= ms;

    if (fade_in < 0)
    {
	time += fade_in;
	fade_in = 0;

	if (time < 0)
	{
	    if (fade_out > 0 && fade_out <= ms)
		lastShot = true;

	    fade_out += time;

	    time = 0;

	    if (fade_out < 0)
		fade_out = 0;
	}
    }

    if (initiate)
    {
	fade_in = fade_out = optionGetFadeTime () * 1000.0;
	time = optionGetDisplayTime () * 1000.0;
	initiate = false;
    }

    if (fade_in || fade_out || time || lastShot)
    {
	active = true;
	mMove += ms / 500.0;

	if (!hasInit)
	{
	    hasInit = true;
	    mMove = 0.0;
	    CompString back_s (optionGetBackground ());
	    CompString logo_s (optionGetLogo ());
	    CompString pname ("splash");

	    back_img =
		GLTexture::readImageToTexture (back_s, pname,
				    	       backSize);
	    logo_img =
		GLTexture::readImageToTexture (logo_s, pname,
				    	       logoSize);

	    if (!back_img.size ())
	    {
		CompString defaultBack (SPLASH_BACKGROUND_DEFAULT);
		back_img =
		    GLTexture::readImageToTexture (defaultBack, pname, backSize);

		if (back_img.size ())
		{
		    compLogMessage ("splash", CompLogLevelWarn,
				    "Could not load splash background image "
				    "\"%s\" using default!",
				    back_s.c_str () );
		}
	    }

	    if (!logo_img.size ())
	    {
		CompString defaultLogo (SPLASH_LOGO_DEFAULT);
		logo_img =
		    GLTexture::readImageToTexture (defaultLogo, pname, logoSize);

		if (logo_img.size ())
		{
		    compLogMessage ("splash", CompLogLevelWarn,
				    "Could not load splash logo image "
				    "\"%s\" using default!",
				    logo_s.c_str () );
		}
	    }

	    if (!back_img.size ())
		compLogMessage ("splash", CompLogLevelWarn,
				"Could not load splash background image "
				"\"%s\" !", back_s.c_str () );

	    if (!logo_img.size ())
		compLogMessage ("splash", CompLogLevelWarn,
				"Could not load splash logo image \"%s\" !",
				logo_s.c_str () );
	}
    }
    else
    {
	active = false;

	if (hasInit)
	    hasInit = false;

	cScreen->preparePaintSetEnabled (this, false);
	gScreen->glPaintOutputSetEnabled (this, false);
	cScreen->donePaintSetEnabled (this, false);

	foreach (CompWindow *w, screen->windows ())
	{
	    SPLASH_WINDOW (w);

	    sw->gWindow->glPaintSetEnabled (sw, false);
	}

    }

    cScreen->preparePaint (ms);

}

void
SplashScreen::donePaint ()
{
    if (fade_in || fade_out || time)
	cScreen->damageScreen ();

    cScreen->donePaint ();
}

static CompRect
splashGetCurrentOutputRect ()
{
    int root_x = 0, root_y = 0;
    int ignore_i;
    unsigned int ignore_ui;
    int output;
    Window ignore_w;


    if (screen->outputDevs ().size () == 1)
	output = 0;
    else
    {
	XQueryPointer (screen->dpy (), screen->root (), &ignore_w, &ignore_w,
		       &root_x, &root_y, &ignore_i, &ignore_i, &ignore_ui);
	output = screen->outputDeviceForPoint (root_x, root_y);
    }

    CompRect rect (screen->outputDevs ()[output]);

    return rect;

}

bool
SplashScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
		       	     const GLMatrix &transform,
		       	     const CompRegion &region,
		       	     CompOutput *output,
		       	     unsigned int mask)
{
    GLMatrix sTransform = transform;
    GLVertexBuffer *stream = GLVertexBuffer::streamingBuffer ();

    bool status = true;

    float alpha = 0.0;

    if (active)
    {
	alpha = (1.0 - (fade_in / (optionGetFadeTime () * 1000.0) ) ) *
		(fade_out / (optionGetFadeTime () * 1000.0) );
	saturation = 1.0 -
			 ((1.0 - (optionGetSaturation () / 100.0) ) * alpha);
	brightness = 1.0 -
			 ((1.0 - (optionGetBrightness () / 100.0) ) * alpha);
    }

    status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    if (!active)
	return status;

    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (back_img.size ())
    {
	int x, y;

	for (x = 0; x < MESH_W; x++)
	{
	    for (y = 0; y < MESH_H; y++)
	    {
		mesh[x][y][0] =
		    (x / (MESH_W - 1.0) ) +
		    (0.02 * sin ( (y / (MESH_H - 1.0) * 8) + mMove) );
		mesh[x][y][1] =
		    (y / (MESH_H - 1.0) ) +
		    (0.02 * sin ( (mesh[x][y][0] * 8) + mMove) );
	    }
	}

	foreach (GLTexture* tex, back_img)
	{
	    tex->enable (GLTexture::Good);

	    if (!screen->outputDevs ().size () > 1)
	    {
	        CompRect headOutputRect = 
			splashGetCurrentOutputRect ();

		x = (headOutputRect.width () - backSize.width ()) / 2;
		y = (headOutputRect.height () - backSize.height ()) / 2;

		x += headOutputRect.x ();
		y += headOutputRect.y ();
	    }
	    else
	    {
		x = (screen->width () - backSize.width ()) / 2;
		y = (screen->height () - backSize.height ()) / 2;
	    }

	    GLTexture::Matrix mat = tex->matrix ();

	    sTransform.translate (x, y, 0);

	    float cx1, cx2, cy1, cy2;

	    std::vector<GLfloat> coords;
	    std::vector<GLfloat> vertices;

	    coords.reserve (12 * (MESH_W - 1) * (MESH_H - 1));
	    vertices.reserve (18 * (MESH_W - 1) * (MESH_H - 1));

	    for (x = 0; x < MESH_W - 1; x++)
	    {
		for (y = 0; y < MESH_H - 1; y++)
		{
		    cx1 = (x / (MESH_W - 1.0) ) * backSize.width ();
		    cx2 = ( (x + 1) / (MESH_W - 1.0) ) * backSize.width ();
		    cy1 = (y / (MESH_H - 1.0) ) * backSize.height ();
		    cy2 = ( (y + 1) / (MESH_H - 1.0) ) * backSize.height ();

		    coords.push_back (COMP_TEX_COORD_X (mat, cx1));
		    coords.push_back (COMP_TEX_COORD_Y (mat, cy1));

		    coords.push_back (COMP_TEX_COORD_X (mat, cx1));
		    coords.push_back (COMP_TEX_COORD_Y (mat, cy2));

		    coords.push_back (COMP_TEX_COORD_X (mat, cx2));
		    coords.push_back (COMP_TEX_COORD_Y (mat, cy2));

		    coords.push_back (COMP_TEX_COORD_X (mat, cx2));
		    coords.push_back (COMP_TEX_COORD_Y (mat, cy2));

		    coords.push_back (COMP_TEX_COORD_X (mat, cx2));
		    coords.push_back (COMP_TEX_COORD_Y (mat, cy1));

		    coords.push_back (COMP_TEX_COORD_X (mat, cx1));
		    coords.push_back (COMP_TEX_COORD_Y (mat, cy1));

		    vertices.push_back (mesh[x][y][0] * backSize.width ());
		    vertices.push_back (mesh[x][y][1] * backSize.height ());
		    vertices.push_back (0.0f);

		    vertices.push_back (mesh[x][y + 1][0] * backSize.width ());
		    vertices.push_back (mesh[x][y + 1][1] * backSize.height ());
		    vertices.push_back (0.0f);

		    vertices.push_back (mesh[x + 1][y + 1][0] * backSize.width ());
		    vertices.push_back (mesh[x + 1][y + 1][1] * backSize.height ());
		    vertices.push_back (0.0f);

		    vertices.push_back (mesh[x + 1][y + 1][0] * backSize.width ());
		    vertices.push_back (mesh[x + 1][y + 1][1] * backSize.height ());
		    vertices.push_back (0.0f);

		    vertices.push_back (mesh[x + 1][y][0] * backSize.width ());
		    vertices.push_back (mesh[x + 1][y][1] * backSize.height ());
		    vertices.push_back (0.0f);

		    vertices.push_back (mesh[x][y][0] * backSize.width ());
		    vertices.push_back (mesh[x][y][1] * backSize.height ());
		    vertices.push_back (0.0f);
		}
	    }

	    stream->begin (GL_TRIANGLES);
	    stream->color4f (1.0, 1.0, 1.0, alpha);
	    stream->addVertices (vertices.size () / 3, &vertices[0]);
	    stream->addTexCoords (0, coords.size () / 2, &coords[0]);
	    if (stream->end ())
		stream->render (sTransform);

	    if (screen->outputDevs ().size () > 1)
	    {
		CompRect headOutputRect =
		    splashGetCurrentOutputRect ();

		x = (headOutputRect.width () - backSize.width ()) / 2;
		y = (headOutputRect.height () - backSize.height ()) / 2;

		x += headOutputRect.x ();
		y += headOutputRect.y ();
	    }
	    else
	    {
		x = (screen->width () - backSize.width ()) / 2;
		y = (screen->height () - backSize.height ()) / 2;
	    }

	    sTransform.translate (-x, -y, 0);

	    tex->disable ();

	}
    }

    if (logo_img.size ())
    {
	foreach (GLTexture* tex, logo_img)
	{
	    tex->enable (GLTexture::Good);
	    int x, y;

	    if (screen->outputDevs ().size () > 1)
	    {
		CompRect headOutputRect =
		    splashGetCurrentOutputRect ();

		x = (headOutputRect.width () - logoSize.width ()) / 2;
		y = (headOutputRect.height () - logoSize.height ()) / 2;

		x += headOutputRect.x ();
		y += headOutputRect.y ();
	    }
	    else
	    {
		x = (screen->width () - logoSize.width ()) / 2;
		y = (screen->height () - logoSize.height ()) / 2;
	    }

	    GLTexture::Matrix mat = tex->matrix ();

	    sTransform.translate (x, y, 0);

	    GLfloat coords[12];
	    GLfloat vertices[18];

	    coords[0] = COMP_TEX_COORD_X (mat, 0);
	    coords[1] = COMP_TEX_COORD_Y (mat, 0);

	    coords[2] = COMP_TEX_COORD_X (mat, 0);
	    coords[3] = COMP_TEX_COORD_Y (mat, logoSize.height ());

	    coords[4] = COMP_TEX_COORD_X (mat, logoSize.width ());
	    coords[5] = COMP_TEX_COORD_Y (mat, logoSize.height ());

	    coords[6] = COMP_TEX_COORD_X (mat, logoSize.width ());
	    coords[7] = COMP_TEX_COORD_Y (mat, logoSize.height ());

	    coords[8] = COMP_TEX_COORD_X (mat, logoSize.width ());
	    coords[9] = COMP_TEX_COORD_Y (mat, 0);

	    coords[10] = COMP_TEX_COORD_X (mat, 0);
	    coords[11] = COMP_TEX_COORD_Y (mat, 0);

	    vertices[0] = 0;
	    vertices[1] = 0;
	    vertices[2] = 0;

	    vertices[3] = 0;
	    vertices[4] = logoSize.height ();
	    vertices[5] = 0;

	    vertices[6] = logoSize.width ();
	    vertices[7] = logoSize.height ();
	    vertices[8] = 0;

	    vertices[9] = logoSize.width ();
	    vertices[10] = logoSize.height ();
	    vertices[11] = 0;

	    vertices[12] = logoSize.width ();
	    vertices[13] = 0;
	    vertices[14] = 0;

	    vertices[15] = 0;
	    vertices[16] = 0;
	    vertices[17] = 0;

	    stream->begin (GL_TRIANGLES);
	    stream->color4f (1.0, 1.0, 1.0, alpha);
	    stream->addVertices (6, vertices);
	    stream->addTexCoords (0, 6, coords);
	    if (stream->end ())
		stream->render (sTransform);

	    sTransform.translate (-x, -y, 0);

	    tex->disable ();
	}
    }

    glDisable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return status;
}

bool
SplashWindow::glPaint (const GLWindowPaintAttrib &attrib,
		       const GLMatrix &transform,
		       const CompRegion &region,
		       unsigned int mask)
{
    bool status;

    SPLASH_SCREEN (screen);

    if (ss->active)
    {
	GLWindowPaintAttrib pA = attrib;
	pA.brightness = (attrib.brightness * ss->brightness);
	pA.saturation = (attrib.saturation * ss->saturation);

	status = gWindow->glPaint (pA, transform, region, mask);
    }
    else
    {
	status = gWindow->glPaint (attrib, transform, region, mask);
    }

    return status;
}


bool
SplashScreen::initiateSplash (CompAction         *action,
	  		      CompAction::State  state,
	  		      CompOption::Vector options)
{
    initiate = true;

    cScreen->preparePaintSetEnabled (this, true);
    gScreen->glPaintOutputSetEnabled (this, true);
    cScreen->donePaintSetEnabled (this, true);

    foreach (CompWindow *w, screen->windows ())
    {
	SPLASH_WINDOW (w);

	sw->gWindow->glPaintSetEnabled (sw, true);
    }

    return false;
}

/* replace with ctor, dtor, init etc */

SplashScreen::SplashScreen (CompScreen *screen) :
    PluginClassHandler <SplashScreen, CompScreen> (screen),
    SplashOptions (),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    splashAtom (XInternAtom (screen->dpy (), "_COMPIZ_WM_SPLASH", 0)),
    fade_in (0),
    fade_out (0),
    time (0),
    backSize (0, 0),
    logoSize (0, 0),
    hasInit (false),
    hasLogo (false),
    hasBack (false),
    mMove (0.0),
    brightness (0),
    saturation (0),
    initiate (false),
    active (false)
{

    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);

    if (optionGetFirststart () )
    {
	Atom actual;
	int result, format;
	unsigned long n, left;
	unsigned char *propData;

	result = XGetWindowProperty (screen->dpy (), screen->root (),
				     splashAtom, 0L, 8192L, false,
				     XA_INTEGER, &actual, &format,
				     &n, &left, &propData);

	if (result == Success && n && propData)
	{
	    XFree (propData);
	}
	else
	{
	    int value = 1;
	    XChangeProperty (screen->dpy (), screen->root (), splashAtom,
			     XA_INTEGER, 32, PropModeReplace,
			     (unsigned char *) &value, 1);
	}

	initiate = true; // should fix later

	if (initiate)
	{
	    cScreen->preparePaintSetEnabled (this, true);
	    gScreen->glPaintOutputSetEnabled (this, true);
	    cScreen->donePaintSetEnabled (this, true);
	}
    }

    optionSetInitiateKeyInitiate (boost::bind (&SplashScreen::initiateSplash,
						this, _1, _2, _3));

}

SplashWindow::SplashWindow (CompWindow *window) :
    PluginClassHandler <SplashWindow, CompWindow> (window),
    window (window),
    gWindow (GLWindow::get (window))
{
    GLWindowInterface::setHandler (gWindow, false);

    SPLASH_SCREEN (screen);

    if (ss->initiate)
    {
	gWindow->glPaintSetEnabled (this, true);
    }
}

bool
SplashPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	return true;

    return false;
}
