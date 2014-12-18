/*
 * Compiz wizard particle system plugin
 *
 * wizard.cpp
 *
 * Written by : Sebastian Kuhlen
 * E-mail     : DiCon@tankwar.de
 *
 * Ported to Compiz 0.9.x
 * Copyright : (c) 2010 Scott Moreau <oreaus@gmail.com>
 *
 * This plugin and parts of its code have been inspired by the showmouse plugin
 * by Dennis Kasprzyk
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

#include <math.h>
#include <string.h>

#include "wizard.h"

/* 3 vertices per triangle, 2 triangles per particle */
const unsigned short CACHESIZE_FACTOR = 3 * 2;

/* 2 coordinates, x and y */
const unsigned short COORD_COMPONENTS = CACHESIZE_FACTOR * 2;

/* each vertex is stored as 3 GLfloats */
const unsigned short VERTEX_COMPONENTS = CACHESIZE_FACTOR * 3;

/* 4 colors, RGBA */
const unsigned short COLOR_COMPONENTS = CACHESIZE_FACTOR * 4;

ParticleSystem::ParticleSystem () :
    tex (0),
    init (false)
{

}

void
ParticleSystem::initParticles (int	f_hardLimit,
			       int	f_softLimit)
{
    particles.clear ();

    hardLimit    = f_hardLimit;
    softLimit    = f_softLimit;
    active       = false;
    lastCount    = 0;

    // Initialize cache
    vertices_cache.clear ();
    coords_cache.clear ();
    colors_cache.clear ();
    dcolors_cache.clear ();

    for (int i = 0; i < hardLimit; ++i)
    {
	Particle part;
	part.t = 0.0f;
	particles.push_back (part);
    }
}

void
WizardScreen::loadGPoints ()
{
    CompOption::Value::Vector GStrength = optionGetGStrength ();
    CompOption::Value::Vector GPosx = optionGetGPosx ();
    CompOption::Value::Vector GPosy = optionGetGPosy ();
    CompOption::Value::Vector GSpeed = optionGetGSpeed ();
    CompOption::Value::Vector GAngle = optionGetGAngle ();
    CompOption::Value::Vector GMovement = optionGetGMovement ();

    /* ensure that all properties have been updated  */
    unsigned int ng = GStrength.size ();
    if (ng != GPosx.size ()  ||
	ng != GPosy.size ()  ||
	ng != GSpeed.size () ||
	ng != GAngle.size () ||
	ng != GMovement.size ())
       return;

    ps.g.clear ();

    for (unsigned int i = 0; i < ng; ++i)
    {
	GPoint gi;
	gi.strength = (float)GStrength.at (i).i () / 1000.0f;
	gi.x = (float)GPosx.at (i).i ();
	gi.y = (float)GPosy.at (i).i ();
	gi.espeed = (float)GSpeed.at (i).i () / 100.0f;
	gi.eangle = (float)GAngle.at (i).i () / 180.0f * M_PI;
	gi.movement = GMovement.at (i).i ();
	ps.g.push_back (gi);
    }
}

void
WizardScreen::loadEmitters ()
{
    CompOption::Value::Vector EActive = optionGetEActive ();
    CompOption::Value::Vector ETrigger = optionGetETrigger ();
    CompOption::Value::Vector EPosx = optionGetEPosx ();
    CompOption::Value::Vector EPosy = optionGetEPosy ();
    CompOption::Value::Vector ESpeed = optionGetESpeed ();
    CompOption::Value::Vector EAngle = optionGetEAngle ();
    CompOption::Value::Vector EMovement = optionGetEMovement ();
    CompOption::Value::Vector ECount = optionGetECount ();
    CompOption::Value::Vector EH = optionGetEH ();
    CompOption::Value::Vector EDh = optionGetEDh ();
    CompOption::Value::Vector EL = optionGetEL ();
    CompOption::Value::Vector EDl = optionGetEDl ();
    CompOption::Value::Vector EA = optionGetEA ();
    CompOption::Value::Vector EDa = optionGetEDa ();
    CompOption::Value::Vector EDx = optionGetEDx ();
    CompOption::Value::Vector EDy = optionGetEDy ();
    CompOption::Value::Vector EDcirc = optionGetEDcirc ();
    CompOption::Value::Vector EVx = optionGetEVx ();
    CompOption::Value::Vector EVy = optionGetEVy ();
    CompOption::Value::Vector EVt = optionGetEVt ();
    CompOption::Value::Vector EVphi = optionGetEVphi ();
    CompOption::Value::Vector EDvx = optionGetEDvx ();
    CompOption::Value::Vector EDvy = optionGetEDvy ();
    CompOption::Value::Vector EDvcirc = optionGetEDvcirc ();
    CompOption::Value::Vector EDvt = optionGetEDvt ();
    CompOption::Value::Vector EDvphi = optionGetEDvphi ();
    CompOption::Value::Vector ES = optionGetES ();
    CompOption::Value::Vector EDs = optionGetEDs ();
    CompOption::Value::Vector ESnew = optionGetESnew ();
    CompOption::Value::Vector EDsnew = optionGetEDsnew ();
    CompOption::Value::Vector EG = optionGetEG ();
    CompOption::Value::Vector EDg = optionGetEDg ();
    CompOption::Value::Vector EGp = optionGetEGp ();

    /* ensure that all properties have been updated  */
    unsigned int ne = EActive.size ();
    if (ne != ETrigger.size ()  ||
	ne != EPosx.size ()     ||
	ne != EPosy.size ()     ||
	ne != ESpeed.size ()    ||
	ne != EAngle.size ()    ||
	ne != EMovement.size () ||
	ne != ECount.size ()    ||
	ne != EH.size ()        ||
	ne != EDh.size ()       ||
	ne != EL.size ()        ||
	ne != EDl.size ()       ||
	ne != EA.size ()        ||
	ne != EDa.size ()       ||
	ne != EDx.size ()       ||
	ne != EDy.size ()       ||
	ne != EDcirc.size ()    ||
	ne != EVx.size ()       ||
	ne != EVy.size ()       ||
	ne != EVt.size ()       ||
	ne != EVphi.size ()     ||
	ne != EDvx.size ()      ||
	ne != EDvy.size ()      ||
	ne != EDvcirc.size ()   ||
	ne != EDvt.size ()      ||
	ne != EDvphi.size ()    ||
	ne != ES.size ()        ||
	ne != EDs.size ()       ||
	ne != ESnew.size ()     ||
	ne != EDsnew.size ()    ||
	ne != EG.size ()        ||
	ne != EDg.size ()       ||
	ne != EGp.size ())
       return;

    ps.e.clear ();

    for (unsigned int i = 0; i < ne; ++i)
    {
	Emitter ei;
	ei.set_active = ei.active = EActive.at (i).b ();
	ei.trigger = ETrigger.at (i).i ();
	ei.x = (float)EPosx.at (i).i ();
	ei.y = (float)EPosy.at (i).i ();
	ei.espeed = (float)ESpeed.at (i).i () / 100.0f;
	ei.eangle = (float)EAngle.at (i).i () / 180.0f * M_PI;
	ei.movement = EMovement.at (i).i ();
	ei.count = (float)ECount.at (i).i ();
	ei.h = (float)EH.at (i).i () / 1000.0f;
	ei.dh = (float)EDh.at (i).i () / 1000.0f;
	ei.l = (float)EL.at (i).i () / 1000.0f;
	ei.dl = (float)EDl.at (i).i () / 1000.0f;
	ei.a = (float)EA.at (i).i () / 1000.0f;
	ei.da = (float)EDa.at (i).i () / 1000.0f;
	ei.dx = (float)EDx.at (i).i ();
	ei.dy = (float)EDy.at (i).i ();
	ei.dcirc = (float)EDcirc.at (i).i ();
	ei.vx = (float)EVx.at (i).i () / 1000.0f;
	ei.vy = (float)EVy.at (i).i () / 1000.0f;
	ei.vt = (float)EVt.at (i).i () / 10000.0f;
	ei.vphi = (float)EVphi.at (i).i () / 10000.0f;
	ei.dvx = (float)EDvx.at (i).i () / 1000.0f;
	ei.dvy = (float)EDvy.at (i).i () / 1000.0f;
	ei.dvcirc = (float)EDvcirc.at (i).i () / 1000.0f;
	ei.dvt = (float)EDvt.at (i).i () / 10000.0f;
	ei.dvphi = (float)EDvphi.at (i).i () / 10000.0f;
	ei.s = (float)ES.at (i).i ();
	ei.ds = (float)EDs.at (i).i ();
	ei.snew = (float)ESnew.at (i).i ();
	ei.dsnew = (float)EDsnew.at (i).i ();
	ei.g = (float)EG.at (i).i () / 1000.0f;
	ei.dg = (float)EDg.at (i).i () / 1000.0f;
	ei.gp = (float)EGp.at (i).i () / 10000.0f;
	ps.e.push_back (ei);
    }
}

void
ParticleSystem::drawParticles (const GLMatrix &transform)
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

    float offA, offB, cOff;
    GLfloat xMinusoffA, xMinusoffB, xPlusoffA, xPlusoffB;
    GLfloat yMinusoffA, yMinusoffB, yPlusoffA, yPlusoffB;
    GLushort r, g, b, a, dark_a;

    /* for each particle, use two triangles to display it */
    foreach (Particle &part, particles)
    {
	if (part.t > 0.0f)
	{
	    cOff = part.s / 2.0f;		//Corner offset from center

	    if (part.t > tnew)		//New particles start larger
		cOff += (part.snew - part.s) * (part.t - tnew)
			/ (1.0f - tnew) / 2.0f;
	    else if (part.t < told)	//Old particles shrink
		cOff -= part.s * (told - part.t) / told / 2.;

	    //Offsets after rotation of Texture
	    offA = cOff * (cos (part.phi) - sin (part.phi));
	    offB = cOff * (cos (part.phi) + sin (part.phi));

	    r = part.c[0] * 65535.0f;
	    g = part.c[1] * 65535.0f;
	    b = part.c[2] * 65535.0f;

	    if (part.t > tnew)		//New particles start at a == 1
		a = part.a + (1.0f - part.a) * (part.t - tnew)
			    / (1.0f - tnew) * 65535.0f;
	    else if (part.t < told)	//Old particles fade to a = 0
		a = part.a * part.t / told * 65535.0f;
	    else				//The others have their own a
		a = part.a * 65535.0f;

	    dark_a = a * darken;

	    xMinusoffA = part.x - offA;
	    xMinusoffB = part.x - offB;
	    xPlusoffA  = part.x + offA;
	    xPlusoffB  = part.x + offB;

	    yMinusoffA = part.y - offA;
	    yMinusoffB = part.y - offB;
	    yPlusoffA  = part.y + offA;
	    yPlusoffB  = part.y + offB;

	    //first triangle
	    vertices_cache[i + 0] = xMinusoffB;
	    vertices_cache[i + 1] = yMinusoffA;
	    vertices_cache[i + 2] = 0;

	    vertices_cache[i + 3] = xMinusoffA;
	    vertices_cache[i + 4] = yPlusoffB;
	    vertices_cache[i + 5] = 0;

	    vertices_cache[i + 6] = xPlusoffB;
	    vertices_cache[i + 7] = yPlusoffA;
	    vertices_cache[i + 8] = 0;

	    //second triangle
	    vertices_cache[i + 9] = xPlusoffB;
	    vertices_cache[i + 10] = yPlusoffA;
	    vertices_cache[i + 11] = 0;

	    vertices_cache[i + 12] = xPlusoffA;
	    vertices_cache[i + 13] = yMinusoffB;
	    vertices_cache[i + 14] = 0;

	    vertices_cache[i + 15] = xMinusoffB;
	    vertices_cache[i + 16] = yMinusoffA;
	    vertices_cache[i + 17] = 0;

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
    int i, j;
    int newCount = 0;
    Particle *part;
    GPoint *gi;
    float gdist, gangle;
    active = false;

    part = &particles[0];
    for (i = 0; i < hardLimit; i++, part++)
    {
	if (part->t > 0.0f)
	{
	    // move particle
	    part->x += part->vx * time;
	    part->y += part->vy * time;

	    // Rotation
	    part->phi += part->vphi*time;

	    //Aging of particles
	    part->t += part->vt * time;
	    //Additional aging of particles increases if softLimit is exceeded
	    if (lastCount > softLimit)
		part->t += part->vt * time * (lastCount - softLimit)
			   / (hardLimit - softLimit);

	    //Global gravity
	    part->vx += gx * time;
	    part->vy += gy * time;

	    //GPoint gravity
	    gi = &g[0];
	    for (j = 0; (unsigned int)j < g.size (); ++j, ++gi)
	    {
		if (gi->strength != 0)
		{
		    gdist = sqrt ((part->x-gi->x)*(part->x-gi->x)
				  + (part->y-gi->y)*(part->y-gi->y));
		    if (gdist > 1)
		    {
			gangle = atan2 (gi->y-part->y, gi->x-part->x);
			part->vx += gi->strength / gdist * cos (gangle) * time;
			part->vy += gi->strength / gdist * sin (gangle) * time;
		    }
		}
	    }
	    active = true;
	    newCount++;
	}
    }
    lastCount = newCount;

    //Particle gravity
    Particle *gpart;
    part = &particles[0];
    for (i = 0; i < hardLimit; ++i, ++part)
    {
	if (part->t > 0.0f && part->g != 0)
	{
	    gpart = &particles[0];
	    for (j = 0; j < hardLimit; ++j, ++gpart)
	    {
		if (gpart->t > 0.0f)
		{
		    gdist = sqrt ((part->x-gpart->x)*(part->x-gpart->x)
				  + (part->y-gpart->y)*(part->y-gpart->y));
		    if (gdist > 1)
		    {
			gangle = atan2 (part->y-gpart->y, part->x-gpart->x);
			gpart->vx += part->g/gdist* cos (gangle) * part->t*time;
			gpart->vy += part->g/gdist* sin (gangle) * part->t*time;
		    }
		}
	    }
	}
    }
}

void
ParticleSystem::finiParticles ()
{
    particles.clear ();

    if (tex)
	glDeleteTextures (1, &tex);

    init = false;
}


void
ParticleSystem::genNewParticles (Emitter *e)
{
    float q, p, t = 0, h, l;
    int count = e->count;

    Particle *part = &particles[0];
    int i, j;

    for (i = 0; i < hardLimit && count > 0; ++i, ++part)
    {
	if (part->t <= 0.0f)
	{
	    //Position
	    part->x = rRange (e->x, e->dx);		// X Position
	    part->y = rRange (e->y, e->dy);		// Y Position
	    if ((q = rRange (e->dcirc/2.,e->dcirc/2.)) > 0)
	    {
		p = rRange (0, M_PI);
		part->x += q * cos (p);
		part->y += q * sin (p);
	    }

	    //Speed
	    part->vx = rRange (e->vx, e->dvx);		// X Speed
	    part->vy = rRange (e->vy, e->dvy);		// Y Speed
	    if ((q = rRange (e->dvcirc / 2.0f, e->dvcirc / 2.0f)) > 0)
	    {
		p = rRange (0, M_PI);
		part->vx += q * cos (p);
		part->vy += q * sin (p);
	    }
	    part->vt = rRange (e->vt, e->dvt);		// Aging speed
	    if (part->vt > -0.0001f)
		part->vt = -0.0001f;

	    //Size, Gravity and Rotation
	    part->s = rRange (e->s, e->ds);		// Particle size
	    part->snew = rRange (e->snew, e->dsnew);	// Particle start size
	    if (e->gp > (float)(random () & 0xffff) / 65535.0f)
		part->g = rRange (e->g, e->dg);		// Particle gravity
	    else
		part->g = 0.0f;
	    part->phi = rRange (0, M_PI);		// Random orientation
	    part->vphi = rRange (e->vphi, e->dvphi);	// Rotation speed

	    //Alpha
	    part->a = rRange (e->a, e->da);		// Alpha
	    if (part->a > 1)
		part->a = 1.0f;
	    else if (part->a < 0)
		part->a = 0.0f;

	    //HSL to RGB conversion from Wikipedia simplified by S = 1
	    h = rRange (e->h, e->dh); //Random hue within range
	    if (h < 0)
		h += 1.0f;
	    else if (t > 1)
		h -= 1.0f;
	    l = rRange (e->l, e->dl); //Random lightness ...
	    if (l > 1)
		l = 1.0f;
	    else if (l < 0)
		l = 0.0f;
	    q = e->l * 2;
	    if (q > 1)
		q = 1.0f;
	    p = 2 * e->l - q;
	    for (j = 0; j < 3; j++)
	    {
		t = h + (1-j)/3.0f;
		if (t < 0)
		    t += 1.0f;
		else if (t > 1)
		    t -= 1.0f;
		if (t < 1/6.)
		    part->c[j] = p + ((q-p)*6*t);
		else if (t < 0.5f)
		    part->c[j] = q;
		else if (t < 2/3.)
		    part->c[j] = p + ((q-p)*6*(2/3.-t));
		else
		    part->c[j] = p;
	    }

	    // give new life
	    part->t = 1.0f;

	    active = true;
	    count -= 1;
	}
    }
}

void
WizardScreen::positionUpdate (const CompPoint &pos)
{
    mx = pos.x ();
    my = pos.y ();

    if (ps.init && active)
    {
	Emitter *ei = &(ps.e[0]);
	GPoint  *gi = &(ps.g[0]);

	for (unsigned int i = 0; i < ps.g.size (); ++i, ++gi)
	{
	    if (gi->movement == MOVEMENT_MOUSEPOSITION)
	    {
		gi->x = pos.x ();
		gi->y = pos.y ();
	    }
	}

	for (unsigned int i = 0; i < ps.e.size (); ++i, ++ei)
	{
	    if (ei->movement == MOVEMENT_MOUSEPOSITION)
	    {
		ei->x = pos.x ();
		ei->y = pos.y ();
	    }
	    if (ei->active && ei->trigger == TRIGGER_MOUSEMOVEMENT)
		ps.genNewParticles (ei);
	}
    }
}

void
WizardScreen::preparePaint (int time)
{
    if (active && !pollHandle.active ())
	pollHandle.start ();

    if (active && !ps.init)
    {
	ps.init = true;
	loadGPoints ();
	loadEmitters ();
	ps.initParticles (optionGetHardLimit (), optionGetSoftLimit ());
	ps.darken = optionGetDarken ();
	ps.blendMode = (optionGetBlend ()) ? GL_ONE :
					      GL_ONE_MINUS_SRC_ALPHA;
	ps.tnew = optionGetTnew ();
	ps.told = optionGetTold ();
	ps.gx = optionGetGx ();
	ps.gy = optionGetGy ();

	ps.active = true;
	glGenTextures (1, &ps.tex);
	glBindTexture (GL_TEXTURE_2D, ps.tex);

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0,
		      GL_RGBA, GL_UNSIGNED_BYTE, particleTex);
	glBindTexture (GL_TEXTURE_2D, 0);
    }

    if (ps.init && active)
    {
	Emitter *ei = &(ps.e[0]);
	GPoint  *gi = &(ps.g[0]);

	for (unsigned int i = 0; i < ps.g.size (); ++i, ++gi)
	{
	    if (gi->movement == MOVEMENT_BOUNCE || gi->movement == MOVEMENT_WRAP)
	    {
		gi->x += gi->espeed * cos (gi->eangle) * time;
		gi->y += gi->espeed * sin (gi->eangle) * time;
		if (gi->x >= screen->width ())
		{
		    if (gi->movement == MOVEMENT_BOUNCE)
		    {
			gi->x = 2*screen->width () - gi->x - 1;
			gi->eangle = M_PI - gi->eangle;
		    }
		    else if (gi->movement == MOVEMENT_WRAP)
			gi->x -= screen->width ();
		}
		else if (gi->x < 0)
		{
		    if (gi->movement == MOVEMENT_BOUNCE)
		    {
			gi->x *= -1;
			gi->eangle = M_PI - gi->eangle;
		    }
		    else if (gi->movement == MOVEMENT_WRAP)
			gi->x += screen->width ();
		}
		if (gi->y >= screen->height ())
		{
		    if (gi->movement == MOVEMENT_BOUNCE)
		    {
			gi->y = 2*screen->height () - gi->y - 1;
			gi->eangle *= -1;
		    }
		    else if (gi->movement == MOVEMENT_WRAP)
			gi->y -= screen->height ();
		}
		else if (gi->y < 0)
		{
		    if (gi->movement == MOVEMENT_BOUNCE)
		    {
			gi->y *= -1;
			gi->eangle *= -1;
		    }
		    else if (gi->movement == MOVEMENT_WRAP)
			gi->y += screen->height ();
		}
	    }
	    if (gi->movement == MOVEMENT_FOLLOWMOUSE
		&& (my!=gi->y||mx!=gi->x))
	    {
		gi->eangle = atan2(my-gi->y, mx-gi->x);
		gi->x += gi->espeed * cos(gi->eangle) * time;
		gi->y += gi->espeed * sin(gi->eangle) * time;
	    }
	}

	for (unsigned int i = 0; i < ps.e.size (); ++i, ++ei)
	{
	    if (ei->movement == MOVEMENT_BOUNCE || ei->movement == MOVEMENT_WRAP)
	    {
		ei->x += ei->espeed * cos (ei->eangle) * time;
		ei->y += ei->espeed * sin (ei->eangle) * time;
		if (ei->x >= screen->width ())
		{
		    if (ei->movement == MOVEMENT_BOUNCE)
		    {
			ei->x = 2*screen->width () - ei->x - 1;
			ei->eangle = M_PI - ei->eangle;
		    }
		    else if (ei->movement == MOVEMENT_WRAP)
			ei->x -= screen->width ();
		}
		else if (ei->x < 0)
		{
		    if (ei->movement == MOVEMENT_BOUNCE)
		    {
			ei->x *= -1;
			ei->eangle = M_PI - ei->eangle;
		    }
		    else if (ei->movement == MOVEMENT_WRAP)
			ei->x += screen->width ();
		}
		if (ei->y >= screen->height ())
		{
		    if (ei->movement == MOVEMENT_BOUNCE)
		    {
			ei->y = 2*screen->height () - ei->y - 1;
			ei->eangle *= -1;
		    }
		    else if (ei->movement == MOVEMENT_WRAP)
			ei->y -= screen->height ();
		}
		else if (ei->y < 0)
		{
		    if (ei->movement == MOVEMENT_BOUNCE)
		    {
			ei->y *= -1;
			ei->eangle *= -1;
		    }
		    else if (ei->movement == MOVEMENT_WRAP)
			ei->y += screen->height ();
		}
	    }
	    if (ei->movement == MOVEMENT_FOLLOWMOUSE
		&& (my!=ei->y||mx!=ei->x))
	    {
		ei->eangle = atan2 (my-ei->y, mx-ei->x);
		ei->x += ei->espeed * cos (ei->eangle) * time;
		ei->y += ei->espeed * sin (ei->eangle) * time;
	    }
	    if (ei->trigger == TRIGGER_RANDOMPERIOD
		&& ei->set_active && !((int)random ()&0xff))
		ei->active = !ei->active;
	    if (ei->active && (
		    (ei->trigger == TRIGGER_PERSISTENT) ||
		    (ei->trigger == TRIGGER_RANDOMSHOT && !((int)random()&0xff)) ||
		    (ei->trigger == TRIGGER_RANDOMPERIOD)
		    ))
		ps.genNewParticles (ei);
	}
    }

    if (ps.active)
    {
	ps.updateParticles (time);
	cScreen->damageScreen ();
    }

    cScreen->preparePaint (time);
}

void
WizardScreen::donePaint ()
{
    if (active || ps.active)
	cScreen->damageScreen ();

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
WizardScreen::glPaintOutput (const GLScreenPaintAttrib	&sa,
			     const GLMatrix		&transform,
			     const CompRegion		&region,
			     CompOutput			*output,
			     unsigned int		mask)
{
    bool status = gScreen->glPaintOutput (sa, transform, region, output, mask);
    GLMatrix sTransform = transform;

    if (!ps.active)
	return status;

    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

    ps.drawParticles (sTransform);

    return status;
}

bool
WizardScreen::toggle ()
{
    active = !active;
    if (active)
	toggleFunctions (true);

    cScreen->damageScreen ();
    return true;
}

void
WizardScreen::toggleFunctions(bool enabled)
{
    cScreen->preparePaintSetEnabled (this, enabled);
    cScreen->donePaintSetEnabled (this, enabled);
    gScreen->glPaintOutputSetEnabled (this, enabled);
}

void
WizardScreen::optionChanged (CompOption	      *opt,
			     WizardOptions::Options num)
{
    /* checked seperately to allow testing
     * the results of individual settings
     * without disturbing the particles
     * already on the screen
     */
    if (opt->name () == "hard_limit")
	ps.initParticles (optionGetHardLimit (), optionGetSoftLimit ());
    else if (opt->name () == "soft_limit")
	ps.softLimit = optionGetSoftLimit ();
    else if (opt->name () == "darken")
	ps.darken = optionGetDarken ();
    else if (opt->name () == "blend")
	ps.blendMode = (optionGetBlend ()) ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA;
    else if (opt->name () == "tnew")
	ps.tnew = optionGetTnew ();
    else if (opt->name () == "told")
	ps.told = optionGetTold ();
    else if (opt->name () == "gx")
	ps.gx = optionGetGx ();
    else if (opt->name () == "gy")
	ps.gy = optionGetGy ();
    else
    {
	loadGPoints ();
	loadEmitters ();
    }
}

WizardScreen::WizardScreen (CompScreen *screen) :
    PluginClassHandler <WizardScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    active (false)
{
    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);

#define optionNotify(name)						       \
    optionSet##name##Notify (boost::bind (&WizardScreen::optionChanged,	       \
    this, _1, _2))

    optionNotify (HardLimit);
    optionNotify (SoftLimit);
    optionNotify (Darken);
    optionNotify (Blend);
    optionNotify (Tnew);
    optionNotify (Told);
    optionNotify (Gx);
    optionNotify (Gy);

    optionNotify (GStrength);
    optionNotify (GPosx);
    optionNotify (GPosy);
    optionNotify (GSpeed);
    optionNotify (GAngle);
    optionNotify (GMovement);
    optionNotify (EActive);
    optionNotify (EName);
    optionNotify (ETrigger);
    optionNotify (EPosx);
    optionNotify (EPosy);
    optionNotify (ESpeed);
    optionNotify (EAngle);
    optionNotify (GMovement);
    optionNotify (ECount);
    optionNotify (EH);
    optionNotify (EDh);
    optionNotify (EL);
    optionNotify (EDl);
    optionNotify (EA);
    optionNotify (EDa);
    optionNotify (EDx);
    optionNotify (EDy);
    optionNotify (EDcirc);
    optionNotify (EVx);
    optionNotify (EVy);
    optionNotify (EVt);
    optionNotify (EVphi);
    optionNotify (EDvx);
    optionNotify (EDvy);
    optionNotify (EDvcirc);
    optionNotify (EDvt);
    optionNotify (EDvphi);
    optionNotify (ES);
    optionNotify (EDs);
    optionNotify (ESnew);
    optionNotify (EDsnew);
    optionNotify (EG);
    optionNotify (EDg);
    optionNotify (EGp);

#undef optionNotify

    pollHandle.setCallback (boost::bind (&WizardScreen::positionUpdate, this, _1));

    optionSetToggleInitiate (boost::bind (&WizardScreen::toggle, this));
}

WizardScreen::~WizardScreen ()
{
    if (pollHandle.active ())
	pollHandle.stop ();

    if (ps.active)
	cScreen->damageScreen ();
}

bool
WizardPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI)	&&
	CompPlugin::checkPluginABI ("mousepoll", COMPIZ_MOUSEPOLL_ABI))
	return true;

    return false;
}
