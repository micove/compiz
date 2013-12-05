/*
 * Compiz fire effect plugin
 *
 * firepaint.cpp
 *
 * Copyright : (C) 2007 by Dennis Kasprzyk
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

#include "firepaint.h"

COMPIZ_PLUGIN_20090315 (firepaint, FirePluginVTable);

/* 3 vertices per triangle, 2 triangles per particle */
const unsigned short CACHESIZE_FACTOR  = 3 * 2;
/* 2 coordinates, x and y */
const unsigned short COORD_COMPONENTS  = CACHESIZE_FACTOR * 2;
/* each vertex is stored as 3 GLfloats */
const unsigned short VERTEX_COMPONENTS = CACHESIZE_FACTOR * 3;
/* 4 colors, RGBA */
const unsigned short COLOR_COMPONENTS  = CACHESIZE_FACTOR * 4;

Particle::Particle () :
    life (0),
    fade (0),
    width (0),
    height (0),
    w_mod (0),
    h_mod (0),
    r (0),
    g (0),
    b (0),
    a (0),
    x (0),
    y (0),
    z (0),
    xi (0),
    yi (0),
    zi (0),
    xg (0),
    yg (0),
    zg (0),
    xo (0),
    yo (0),
    zo (0)
{
}

ParticleSystem::ParticleSystem (int n) :
    x (0),
    y (0)
{
    initParticles (n);
}

ParticleSystem::ParticleSystem () :
    slowdown (1.0f),
    tex (0),
    active (false),
    x (0),
    y (0),
    darken (0.0f),
    blendMode (0)
{
    initParticles (0);
}

ParticleSystem::~ParticleSystem ()
{
    finiParticles ();
}

void
ParticleSystem::initParticles (int f_numParticles)
{
    particles.clear ();

    // Initialize cache
    vertices_cache.clear ();
    coords_cache.clear ();
    colors_cache.clear ();
    dcolors_cache.clear ();

    for (int i = 0; i < f_numParticles; ++i)
    {
	Particle p;
	p.life = 0.0f;
	particles.push_back (p);
    }
}

void
ParticleSystem::drawParticles(const GLMatrix &transform)
{
    int i, j, k, l;

    /* Check that the cache is big enough */
    if (vertices_cache.size () < particles.size () * VERTEX_COMPONENTS)
	vertices_cache.resize (particles.size () * VERTEX_COMPONENTS);

    if (coords_cache.size () < particles.size () * COORD_COMPONENTS)
	coords_cache.resize (particles.size () * COORD_COMPONENTS);

    if (colors_cache.size () < particles.size () * COLOR_COMPONENTS)
	colors_cache.resize (particles.size () * COLOR_COMPONENTS);

    if (darken > 0)
	if (dcolors_cache.size () < particles.size () * COLOR_COMPONENTS)
	    dcolors_cache.resize (particles.size () * COLOR_COMPONENTS);

    GLboolean glBlendEnabled = glIsEnabled (GL_BLEND);

    if (!glBlendEnabled)
	glEnable (GL_BLEND);

    if (tex)
    {
	glBindTexture (GL_TEXTURE_2D, tex);
	glEnable (GL_TEXTURE_2D);
    }

    i = j = k = l = 0;

    GLfloat w, h;
    GLfloat xMinusW, xPlusW, yMinusH, yPlusH;
    GLushort r, g, b, a, dark_a;

    /* for each particle, use two triangles to display it */
    foreach (Particle &part, particles) 
    {
	if (part.life > 0.0f)
	{
	    w = part.width  / 2.0f;
	    h = part.height / 2.0f;

	    r = part.r * 65535.0f;
	    g = part.g * 65535.0f;
	    b = part.b * 65535.0f;
	    a      = part.life * part.a * 65535.0f;
	    dark_a = part.life * part.a * 65535.0f * darken;

	    w += w * part.w_mod * part.life;
	    h += h * part.h_mod * part.life;

	    xMinusW = part.x - w;
	    xPlusW  = part.x + w;

	    yMinusH = part.y - h;
	    yPlusH  = part.y + h;

	    //first triangle
	    vertices_cache[i + 0] = xMinusW;
	    vertices_cache[i + 1] = yMinusH;
	    vertices_cache[i + 2] = part.z;

	    vertices_cache[i + 3] = xMinusW;
	    vertices_cache[i + 4] = yPlusH;
	    vertices_cache[i + 5] = part.z;

	    vertices_cache[i + 6] = xPlusW;
	    vertices_cache[i + 7] = yPlusH;
	    vertices_cache[i + 8] = part.z;

	    //second triangle
	    vertices_cache[i + 9]  = xPlusW;
	    vertices_cache[i + 10] = yPlusH;
	    vertices_cache[i + 11] = part.z;

	    vertices_cache[i + 12] = xPlusW;
	    vertices_cache[i + 13] = yMinusH;
	    vertices_cache[i + 14] = part.z;

	    vertices_cache[i + 15] = xMinusW;
	    vertices_cache[i + 16] = yMinusH;
	    vertices_cache[i + 17] = part.z;

	    i += 18;

	    coords_cache[j + 0] = 0.0;
	    coords_cache[j + 1] = 0.0;

	    coords_cache[j + 2] = 0.0;
	    coords_cache[j + 3] = 1.0;

	    coords_cache[j + 4] = 1.0;
	    coords_cache[j + 5] = 1.0;

	    //second
	    coords_cache[j + 6] = 1.0;
	    coords_cache[j + 7] = 1.0;

	    coords_cache[j + 8] = 1.0;
	    coords_cache[j + 9] = 0.0;
	    
	    coords_cache[j + 10] = 0.0;
	    coords_cache[j + 11] = 0.0;

	    j += 12;

	    colors_cache[k + 0] = r;
	    colors_cache[k + 1] = g;
	    colors_cache[k + 2] = b;
	    colors_cache[k + 3] = a;

	    colors_cache[k + 4] = r;
	    colors_cache[k + 5] = g;
	    colors_cache[k + 6] = b;
	    colors_cache[k + 7] = a;

	    colors_cache[k + 8]  = r;
	    colors_cache[k + 9]  = g;
	    colors_cache[k + 10] = b;
	    colors_cache[k + 11] = a;

	    //second
	    colors_cache[k + 12] = r;
	    colors_cache[k + 13] = g;
	    colors_cache[k + 14] = b;
	    colors_cache[k + 15] = a;

	    colors_cache[k + 16] = r;
	    colors_cache[k + 17] = g;
	    colors_cache[k + 18] = b;
	    colors_cache[k + 19] = a;

	    colors_cache[k + 20] = r;
	    colors_cache[k + 21] = g;
	    colors_cache[k + 22] = b;
	    colors_cache[k + 23] = a;

	    k += 24;

	    if (darken > 0)
	    {
		dcolors_cache[l + 0] = r;
		dcolors_cache[l + 1] = g;
		dcolors_cache[l + 2] = b;
		dcolors_cache[l + 3] = dark_a;

		dcolors_cache[l + 4] = r;
		dcolors_cache[l + 5] = g;
		dcolors_cache[l + 6] = b;
		dcolors_cache[l + 7] = dark_a;

		dcolors_cache[l + 8]  = r;
		dcolors_cache[l + 9]  = g;
		dcolors_cache[l + 10] = b;
		dcolors_cache[l + 11] = dark_a;

		//second
		dcolors_cache[l + 12] = r;
		dcolors_cache[l + 13] = g;
		dcolors_cache[l + 14] = b;
		dcolors_cache[l + 15] = dark_a;

		dcolors_cache[l + 16] = r;
		dcolors_cache[l + 17] = g;
		dcolors_cache[l + 18] = b;
		dcolors_cache[l + 19] = dark_a;

		dcolors_cache[l + 20] = r;
		dcolors_cache[l + 21] = g;
		dcolors_cache[l + 22] = b;
		dcolors_cache[l + 23] = dark_a;

		l += 24;
	    }
	}
    }

    GLVertexBuffer *stream = GLVertexBuffer::streamingBuffer ();

    if (darken > 0)
    {
	glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
	stream->begin (GL_TRIANGLES);
	stream->addVertices (i / 3, &vertices_cache[0]);
	stream->addTexCoords (0, j / 2, &coords_cache[0]);
	stream->addColors (l / 4, &dcolors_cache[0]);

	if (stream->end ())
	    stream->render (transform);
    }

    /* draw particles */
    glBlendFunc (GL_SRC_ALPHA, blendMode);
    stream->begin (GL_TRIANGLES);

    stream->addVertices (i / 3, &vertices_cache[0]);
    stream->addTexCoords (0, j / 2, &coords_cache[0]);
    stream->addColors (k / 4, &colors_cache[0]);

    if (stream->end ())
	stream->render (transform);

    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_TEXTURE_2D);

    /* only disable blending if it was disabled before */
    if (!glBlendEnabled)
	glDisable (GL_BLEND);
}

void
ParticleSystem::updateParticles (float time)
{
    float speed      = (time / 50.0);
    float f_slowdown = slowdown * (1 - MAX (0.99, time / 1000.0)) * 1000;

    active = false;

    foreach (Particle &part, particles)
    {
	if (part.life > 0.0f)
	{
	    // move particle
	    part.x += part.xi / f_slowdown;
	    part.y += part.yi / f_slowdown;
	    part.z += part.zi / f_slowdown;

	    // modify speed
	    part.xi += part.xg * speed;
	    part.yi += part.yg * speed;
	    part.zi += part.zg * speed;

	    // modify life
	    part.life -= part.fade * speed;
	    active = true;
	}
    }
}

void
ParticleSystem::finiParticles ()
{
    particles.clear ();

    if (tex)
	glDeleteTextures (1, &tex);
}

static void
toggleFunctions (bool enabled)
{
    FIRE_SCREEN (screen);
    screen->handleEventSetEnabled (fs, enabled);
    fs->cScreen->preparePaintSetEnabled (fs, enabled);
    fs->gScreen->glPaintOutputSetEnabled (fs, enabled);
    fs->cScreen->donePaintSetEnabled (fs, enabled);
}

void
FireScreen::fireAddPoint (int  x,
			  int  y,
			  bool requireGrab)
{
    if (!requireGrab || grabIndex)
    {
	XPoint p;

	p.x = x;
	p.y = y;

	points.push_back (p);

	toggleFunctions (true);
    }
}

bool
FireScreen::addParticle (CompAction         *action,
			 CompAction::State  state,
			 CompOption::Vector options)
{
    float x = CompOption::getFloatOptionNamed (options, "x", 0);
    float y = CompOption::getFloatOptionNamed (options, "y", 0);

    fireAddPoint (x, y, false);

    cScreen->damageScreen ();

    return true;
}

bool
FireScreen::initiate (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options)
{
    if (screen->otherGrabExist (NULL))
	return false;

    if (!grabIndex)
	grabIndex = screen->pushGrab (None, "firepaint");

    if (state & CompAction::StateInitButton)
	action->setState (action->state () | CompAction::StateTermButton);

    if (state & CompAction::StateInitKey)
	action->setState (action->state () | CompAction::StateTermKey);

    fireAddPoint (pointerX, pointerY, true);

    return true;
}

bool
FireScreen::terminate (CompAction         *action,
		       CompAction::State  state,
		       CompOption::Vector options)
{
    if (grabIndex)
    {
	screen->removeGrab (grabIndex, NULL);
	grabIndex = 0;
    }

    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));
    return false;
}

bool
FireScreen::clear (CompAction         *action,
		   CompAction::State  state,
		   CompOption::Vector options)
{
    points.clear ();
    return true;
}

void
FireScreen::preparePaint (int time)
{
    float bg = optionGetBgBrightness () / 100.0f;

    if (init && !points.empty ())
    {
	ps.initParticles (optionGetNumParticles ());
	init = false;

	glGenTextures (1, &ps.tex);
	glBindTexture (GL_TEXTURE_2D, ps.tex);

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0,
		      GL_RGBA, GL_UNSIGNED_BYTE, fireTex);
	glBindTexture (GL_TEXTURE_2D, 0);

	ps.slowdown  = optionGetFireSlowdown ();
	ps.darken    = 0.5f; /* TODO: Magic number */
	ps.blendMode = GL_ONE;
    }

    if (!init)
	ps.updateParticles (time);

    if (!points.empty ())
    {
	int   rVal2;
	float rVal, size = 4;
	float fireLife   = optionGetFireLife ();
	float fireWidth  = optionGetFireSize ();
	float fireHeight = fireWidth * 1.5f;
	bool  mystFire   = optionGetFireMystical ();
	float max_new    = MIN ((int) ps.particles.size (),  (int) points.size () * 2) *
			   ((float) time / 50.0f) * (1.05f - fireLife);

	for (unsigned int i = 0; i < ps.particles.size () && max_new > 0; ++i)
	{
	    Particle &part = ps.particles.at (i);

	    if (part.life <= 0.0f)
	    {
		/* give gt new life */
		rVal = (float) (random () & 0xff) / 255.0;
		part.life = 1.0f;
		/* Random Fade Value */
		part.fade = (rVal * (1 - fireLife) +
			     (0.2f * (1.01 - fireLife)));

		/* set size */
		part.width  = fireWidth;
		part.height = fireHeight;
		rVal = (float) (random () & 0xff) / 255.0;
		part.w_mod = size * rVal;
		part.h_mod = size * rVal;

		/* choose random position */
		rVal2 = random () % points.size ();
		part.x = points.at (rVal2).x;
		part.y = points.at (rVal2).y;
		part.z = 0.0f;
		part.xo = part.x;
		part.yo = part.y;
		part.zo = part.z;

		/* set speed and direction */
		rVal = (float) (random () & 0xff) / 255.0;
		part.xi = ( (rVal * 20.0) - 10.0f);
		rVal = (float) (random () & 0xff) / 255.0;
		part.yi = ( (rVal * 20.0) - 15.0f);
		part.zi = 0.0f;
		rVal = (float) (random () & 0xff) / 255.0;

		if (mystFire)
		{
		    /* Random colors! (aka Mystical Fire) */
		    rVal = (float) (random () & 0xff) / 255.0;
		    part.r = rVal;
		    rVal = (float) (random () & 0xff) / 255.0;
		    part.g = rVal;
		    rVal = (float) (random () & 0xff) / 255.0;
		    part.b = rVal;
		}
		else
		{
		    part.r = optionGetFireColorRed () / 0xffff -
			     (rVal / 1.7 * optionGetFireColorRed () / 0xffff);
		    part.g = optionGetFireColorGreen () / 0xffff -
			     (rVal / 1.7 * optionGetFireColorGreen () / 0xffff);
		    part.b = optionGetFireColorBlue () / 0xffff -
			     (rVal / 1.7 * optionGetFireColorBlue () / 0xffff);
		}

		/* set transparency */
		part.a = (float) optionGetFireColorAlpha () / 0xffff;

		/* set gravity */
		part.xg = (part.x < part.xo) ? 1.0 : -1.0;
		part.yg = -3.0f;
		part.zg = 0.0f;

		ps.active = true;

		max_new -= 1;
	    }
	    else
		part.xg = (part.x < part.xo) ? 1.0 : -1.0;
	}
    }

    if (points.size () && brightness != bg)
    {
	float div = 1.0 - bg;
	div *= (float) time / 500.0;
	brightness = MAX (bg, brightness - div);
    }

    if (points.empty () && brightness != 1.0)
    {
	float div = 1.0 - bg;
	div *= (float) time / 500.0;
	brightness = MIN (1.0, brightness + div);
    }

    if (!init && points.empty () && !ps.active)
    {
	ps.finiParticles ();
	init = true;
    }

    cScreen->preparePaint (time);
}

bool
FireScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			   const GLMatrix            &transform,
			   const CompRegion          &region,
			   CompOutput                *output,
			   unsigned int              mask)
{
    bool status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    if ((!init && ps.active) || brightness < 1.0)
    {
	GLMatrix sTransform = transform;

	sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	if (brightness < 1.0)
	{
	    /* cover the screen with a rectangle and darken it
	     * (coded as two GL_TRIANGLES for GLES compatibility)
	     */
	    GLfloat vertices[18];
	    GLushort colors[24];

	    vertices[0] = (GLfloat)output->region ()->extents.x1;
	    vertices[1] = (GLfloat)output->region ()->extents.y1;
	    vertices[2] = 0.0f;

	    vertices[3] = (GLfloat)output->region ()->extents.x1;
	    vertices[4] = (GLfloat)output->region ()->extents.y2;
	    vertices[5] = 0.0f;

	    vertices[6] = (GLfloat)output->region ()->extents.x2;
	    vertices[7] = (GLfloat)output->region ()->extents.y2;
	    vertices[8] = 0.0f;

	    vertices[9]  = (GLfloat)output->region ()->extents.x2;
	    vertices[10] = (GLfloat)output->region ()->extents.y2;
	    vertices[11] = 0.0f;

	    vertices[12] = (GLfloat)output->region ()->extents.x2;
	    vertices[13] = (GLfloat)output->region ()->extents.y1;
	    vertices[14] = 0.0f;

	    vertices[15] = (GLfloat)output->region ()->extents.x1;
	    vertices[16] = (GLfloat)output->region ()->extents.y1;
	    vertices[17] = 0.0f;

	    for (int i = 0; i <= 5; ++i)
	    {
		colors[i*4+0] = 0;
		colors[i*4+1] = 0;
		colors[i*4+2] = 0;
		colors[i*4+3] = (1.0 - brightness) * 65535.0f;
	    }

	    GLVertexBuffer *stream        = GLVertexBuffer::streamingBuffer ();
	    GLboolean      glBlendEnabled = glIsEnabled (GL_BLEND);

	    if (!glBlendEnabled)
		glEnable (GL_BLEND);

	    stream->begin (GL_TRIANGLES);
	    stream->addVertices (6, vertices);
	    stream->addColors (6, colors);

	    if (stream->end ())
		stream->render (sTransform);

	    /* only disable blending if it was already disabled */
	    if (!glBlendEnabled)
		glDisable (GL_BLEND);
	}

	if (!init && ps.active)
	    ps.drawParticles (sTransform);
    }

    return status;
}

void
FireScreen::donePaint ()
{
    if ( (!init && ps.active) || !points.empty () || brightness < 1.0)
	cScreen->damageScreen ();
    else
	toggleFunctions (false);

    cScreen->donePaint ();
}

void
FireScreen::handleEvent (XEvent *event)
{
    switch (event->type)
    {
    case MotionNotify:
	fireAddPoint (pointerX, pointerY, true);
	break;

    case EnterNotify:
    case LeaveNotify:
	fireAddPoint (pointerX, pointerY, true);
	break;

    default:
	break;
    }

    screen->handleEvent (event);
}

FireScreen::FireScreen (CompScreen *screen) :
    PluginClassHandler <FireScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    init (true),
    brightness (1.0),
    grabIndex (0)
{
    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);

    optionSetInitiateKeyInitiate (boost::bind (&FireScreen::initiate, this, _1,
						_2, _3));
    optionSetInitiateButtonInitiate (boost::bind (&FireScreen::initiate, this,
						   _1, _2, _3));
    optionSetInitiateKeyTerminate (boost::bind (&FireScreen::terminate, this,
						 _1, _2, _3));
    optionSetInitiateButtonTerminate (boost::bind (&FireScreen::terminate, this,
						   _1, _2, _3));

    optionSetClearKeyInitiate (boost::bind (&FireScreen::clear, this, _1, _2,
					    _3));
    optionSetClearButtonInitiate (boost::bind (&FireScreen::clear, this, _1, _2,
					       _3));

    optionSetAddParticleInitiate (boost::bind (&FireScreen::addParticle, this,
						_1, _2, _3));
}

FireScreen::~FireScreen ()
{
    if (!init)
	ps.finiParticles ();
}

bool
FirePluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	return true;

    return false;
}
