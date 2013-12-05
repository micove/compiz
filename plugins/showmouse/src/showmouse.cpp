/*
 *
 * Compiz show mouse pointer plugin
 *
 * showmouse.cpp
 *
 * Copyright : (C) 2008 by Dennis Kasprzyk
 * E-mail    : onestone@opencompositing.org
 *
 * Ported to Compiz 0.9 by:
 * Copyright : (C) 2009 by Sam Spilsbury
 * E-mail    : smpillaz@gmail.com
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

#include "showmouse.h"

COMPIZ_PLUGIN_20090315 (showmouse, ShowmousePluginVTable);

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
    x (0),
    y (0)
{
    initParticles (0);
}

ParticleSystem::~ParticleSystem ()
{
    finiParticles ();
}

void
ParticleSystem::initParticles (int            f_numParticles)
{
    particles.clear ();

    tex = 0;
    slowdown = 1;
    active = false;
    darken = 0;

    // Initialize cache
    vertices_cache.clear ();
    coords_cache.clear ();
    colors_cache.clear ();
    dcolors_cache.clear ();

    for (int i = 0; i < f_numParticles; i++)
    {
	Particle p;
	p.life = 0.0f;
	particles.push_back (p);
    }
}

void
ParticleSystem::drawParticles (const GLMatrix    &transform)
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

    glEnable (GL_BLEND);

    if (tex)
	glBindTexture (GL_TEXTURE_2D, tex);

    i = j = k = l = 0;

    /* use 2 triangles per particle */
    foreach (Particle &part, particles)
    {
	if (part.life > 0.0f)
	{
	    float w = part.width / 2;
	    float h = part.height / 2;

	    GLushort r, g, b, a, dark_a;

	    r = part.r * 65535.0f;
	    g = part.g * 65535.0f;
	    b = part.b * 65535.0f;
	    a = part.life * part.a * 65535.0f;
	    dark_a = part.life * part.a * darken * 65535.0f;

	    w += (w * part.w_mod) * part.life;
	    h += (h * part.h_mod) * part.life;

	    //first triangle
	    vertices_cache[i + 0] = part.x - w;
	    vertices_cache[i + 1] = part.y - h;
	    vertices_cache[i + 2] = part.z;

	    vertices_cache[i + 3] = part.x - w;
	    vertices_cache[i + 4] = part.y + h;
	    vertices_cache[i + 5] = part.z;

	    vertices_cache[i + 6] = part.x + w;
	    vertices_cache[i + 7] = part.y + h;
	    vertices_cache[i + 8] = part.z;

	    //second triangle
	    vertices_cache[i + 9] = part.x + w;
	    vertices_cache[i + 10] = part.y + h;
	    vertices_cache[i + 11] = part.z;

	    vertices_cache[i + 12] = part.x + w;
	    vertices_cache[i + 13] = part.y - h;
	    vertices_cache[i + 14] = part.z;

	    vertices_cache[i + 15] = part.x - w;
	    vertices_cache[i + 16] = part.y - h;
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

	    colors_cache[k + 8] = r;
	    colors_cache[k + 9] = g;
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

	    if(darken > 0)
	    {
		dcolors_cache[l + 0] = r;
		dcolors_cache[l + 1] = g;
		dcolors_cache[l + 2] = b;
		dcolors_cache[l + 3] = dark_a;

		dcolors_cache[l + 4] = r;
		dcolors_cache[l + 5] = g;
		dcolors_cache[l + 6] = b;
		dcolors_cache[l + 7] = dark_a;

		dcolors_cache[l + 8] = r;
		dcolors_cache[l + 9] = g;
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

    // draw particles
    glBlendFunc (GL_SRC_ALPHA, blendMode);
    stream->begin (GL_TRIANGLES);

    stream->addVertices (i / 3, &vertices_cache[0]);
    stream->addTexCoords (0, j / 2, &coords_cache[0]);
    stream->addColors (k / 4, &colors_cache[0]);

    if (stream->end ())
	stream->render (transform);

    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_BLEND);
}

void
ParticleSystem::updateParticles (float          time)
{
    float speed = (time / 50.0);
    float f_slowdown = slowdown * (1 - MAX (0.99, time / 1000.0) ) * 1000;

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
    SHOWMOUSE_SCREEN (screen);
    ss->cScreen->preparePaintSetEnabled (ss, enabled);
    ss->gScreen->glPaintOutputSetEnabled (ss, enabled);
    ss->cScreen->donePaintSetEnabled (ss, enabled);
}

void
ShowmouseScreen::genNewParticles (int f_time)
{
    bool rColor     = optionGetRandom ();
    float life      = optionGetLife ();
    float lifeNeg   = 1 - life;
    float fadeExtra = 0.2f * (1.01 - life);
    float max_new   = ps.particles.size () * ((float)f_time / 50) * (1.05 - life);

    unsigned short *c = optionGetColor ();

    float colr1 = (float)c[0] / 0xffff;
    float colg1 = (float)c[1] / 0xffff;
    float colb1 = (float)c[2] / 0xffff;
    float colr2 = 1.0 / 4.0 * (float)c[0] / 0xffff;
    float colg2 = 1.0 / 4.0 * (float)c[1] / 0xffff;
    float colb2 = 1.0 / 4.0 * (float)c[2] / 0xffff;
    float cola  = (float)c[3] / 0xffff;
    float rVal;

    float partw = optionGetSize () * 5;
    float parth = partw;

    unsigned int i, j;

    float pos[10][2];
    unsigned int nE = optionGetEmitters ();
    float rA     = (2 * M_PI) / nE;
    int radius   = optionGetRadius ();

    for (i = 0; i < nE; i++)
    {
	pos[i][0]  = sin (rot + (i * rA)) * radius;
	pos[i][0] += mousePos.x ();
	pos[i][1]  = cos (rot + (i * rA)) * radius;
	pos[i][1] += mousePos.y ();
    }

    for (i = 0; i < ps.particles.size () && max_new > 0; i++)
    {
	Particle &part = ps.particles.at (i);
	if (part.life <= 0.0f)
	{
	    // give gt new life
	    rVal = (float)(random() & 0xff) / 255.0;
	    part.life = 1.0f;
	    part.fade = rVal * lifeNeg + fadeExtra; // Random Fade Value

	    // set size
	    part.width = partw;
	    part.height = parth;
	    part.w_mod = part.h_mod = -1;

	    // choose random position
	    j        = random() % nE;
	    part.x  = pos[j][0];
	    part.y  = pos[j][1];
	    part.z  = 0.0;
	    part.xo = part.x;
	    part.yo = part.y;
	    part.zo = part.z;

	    // set speed and direction
	    rVal     = (float)(random() & 0xff) / 255.0;
	    part.xi = ((rVal * 20.0) - 10.0f);
	    rVal     = (float)(random() & 0xff) / 255.0;
	    part.yi = ((rVal * 20.0) - 10.0f);
	    part.zi = 0.0f;

	    if (rColor)
	    {
		// Random colors! (aka Mystical Fire)
		rVal    = (float)(random() & 0xff) / 255.0;
		part.r = rVal;
		rVal    = (float)(random() & 0xff) / 255.0;
		part.g = rVal;
		rVal    = (float)(random() & 0xff) / 255.0;
		part.b = rVal;
	    }
	    else
	    {
		rVal    = (float)(random() & 0xff) / 255.0;
		part.r = colr1 - rVal * colr2;
		part.g = colg1 - rVal * colg2;
		part.b = colb1 - rVal * colb2;
	    }
	    // set transparency
	    part.a = cola;

	    // set gravity
	    part.xg = 0.0f;
	    part.yg = 0.0f;
	    part.zg = 0.0f;

	    ps.active = true;
	    max_new   -= 1;
	}
    }
}

void
ShowmouseScreen::doDamageRegion ()
{
    float w, h;

    float x1 = screen->width ();
    float x2 = 0;
    float y1 = screen->height ();
    float y2 = 0;

    foreach (Particle &p, ps.particles)
    {
	w = p.width / 2;
	h = p.height / 2;

	w += (w * p.w_mod) * p.life;
	h += (h * p.h_mod) * p.life;
	
	x1 = MIN (x1, p.x - w);
	x2 = MAX (x2, p.x + w);
	y1 = MIN (y1, p.y - h);
	y2 = MAX (y2, p.y + h);
    }

    CompRegion r (floor (x1), floor (y1), (ceil (x2) - floor (x1)),
					  (ceil (y2) - floor (y1)));
    cScreen->damageRegion (r);
}

void
ShowmouseScreen::positionUpdate (const CompPoint &p)
{
    mousePos = p;
}

void
ShowmouseScreen::preparePaint (int f_time)
{
    if (active && !pollHandle.active ())
    {
	mousePos = MousePoller::getCurrentPosition ();
	pollHandle.start ();
    }

    if (active && !ps.active)
    {
	ps.initParticles (optionGetNumParticles ());
	ps.slowdown = optionGetSlowdown ();
	ps.darken = optionGetDarken ();
	ps.blendMode = (optionGetBlend()) ? GL_ONE :
			    GL_ONE_MINUS_SRC_ALPHA;
	ps.active = true;

	glGenTextures(1, &ps.tex);
	glBindTexture(GL_TEXTURE_2D, ps.tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, starTex);
	glBindTexture(GL_TEXTURE_2D, 0);
    }

    rot = fmod (rot + (((float)f_time / 1000.0) * 2 * M_PI *
		    optionGetRotationSpeed ()), 2 * M_PI);

    if (ps.active)
    {
	ps.updateParticles (f_time);
	doDamageRegion ();
    }

    if (active)
	genNewParticles (f_time);

    cScreen->preparePaint (f_time);
}

void
ShowmouseScreen::donePaint ()
{
    if (active || (ps.active))
	doDamageRegion ();

    if (!active && pollHandle.active ())
	pollHandle.stop ();

    if (!active && !ps.active)
    {
	ps.finiParticles ();
	toggleFunctions (false);
    }

    cScreen->donePaint ();
}

bool
ShowmouseScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
				const GLMatrix		  &transform,
				const CompRegion	  &region,
				CompOutput		  *output,
				unsigned int		  mask)
{
    GLMatrix sTransform = transform;

    bool status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    if (!ps.active)
	return status;

    //sTransform.reset ();

    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

    ps.drawParticles (sTransform);

    return status;
}

bool
ShowmouseScreen::terminate (CompAction         *action,
			    CompAction::State  state,
			    CompOption::Vector options)
{
    active = false;

    doDamageRegion ();

    gScreen->glPaintOutputSetEnabled (gScreen, false);

    return true;
}

bool
ShowmouseScreen::initiate (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector options)
{
    if (active)
	return terminate (action, state, options);

    active = true;

    toggleFunctions (true);

    gScreen->glPaintOutputSetEnabled (gScreen, true);

    return true;
}

ShowmouseScreen::ShowmouseScreen (CompScreen *screen) :
    PluginClassHandler <ShowmouseScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    active (false),
    rot (0.0f)
{
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);

    pollHandle.setCallback (boost::bind (&ShowmouseScreen::positionUpdate, this,
					 _1));

    optionSetInitiateInitiate (boost::bind (&ShowmouseScreen::initiate, this,
					    _1, _2, _3));
    optionSetInitiateTerminate (boost::bind (&ShowmouseScreen::terminate, this,
					     _1, _2, _3));

    optionSetInitiateButtonInitiate (boost::bind (&ShowmouseScreen::initiate,
						  this,  _1, _2, _3));
    optionSetInitiateButtonTerminate (boost::bind (&ShowmouseScreen::terminate,
						   this,  _1, _2, _3));

    optionSetInitiateEdgeInitiate (boost::bind (&ShowmouseScreen::initiate,
						this,  _1, _2, _3));
    optionSetInitiateEdgeTerminate (boost::bind (&ShowmouseScreen::terminate,
						 this,  _1, _2, _3));
}

ShowmouseScreen::~ShowmouseScreen ()
{
    ps.finiParticles ();

    if (pollHandle.active ())
	pollHandle.stop ();
}

bool
ShowmousePluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI)	&&
	CompPlugin::checkPluginABI ("mousepoll", COMPIZ_MOUSEPOLL_ABI))
	return true;

    return false;
}
