/*
 * Compiz wizard particle system plugin
 *
 * wizard.h
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

#include <core/core.h>
#include <composite/composite.h>
#include <opengl/opengl.h>
#include <mousepoll/mousepoll.h>

#include "wizard_options.h"
#include "wizard_tex.h"

extern const unsigned short CACHESIZE_FACTOR;
extern const unsigned short COORD_COMPONENTS;
extern const unsigned short VERTEX_COMPONENTS;
extern const unsigned short COLOR_COMPONENTS;

static float
rRange (float avg, float range)
{
    return avg + (float)((random () & 0xff)/127.5-1.)*range;
}

typedef enum
{
    TRIGGER_PERSISTENT = 0,
    TRIGGER_MOUSEMOVEMENT,
    TRIGGER_RANDOMSHOT,
    TRIGGER_RANDOMPERIOD
} TriggerType;

typedef enum
{
    MOVEMENT_MOUSEPOSITION = 0,
    MOVEMENT_FOLLOWMOUSE,
    MOVEMENT_BOUNCE,
    MOVEMENT_WRAP
} MovementType;

class GPoint
{
    public:

	float strength;		// Strength of this gravity source
	float x;			// X position
	float y;			// Y position
	float espeed;		// Speed of the gravity source
	float eangle;		// Angle for the movement of this gravity source
	int   movement;		// Type of movement of this gravity source
};

class Particle
{
    public:

	float c[3];			// Color
	float a;			// alpha value
	float x;			// X position
	float y;			// Y position
	float t;			// t position (age, born at 1, dies at 0)
	float phi;			// Orientation of texture
	float vx;			// X speed
	float vy;			// Y speed
	float vt;			// t speed (aging speed)
	float vphi;			// Rotation speed
	float s;			// size (side of the square)
	float snew;			// Size when born (reduced to s while new)
	float g;			// Gravity from this particle
};

class Emitter
{
    public:

	bool  set_active;		// Set to active in the settings
	bool  active;		// Currently active (differs from set_active for
				    //                   the random period trigger)
	int   trigger;		// When to generate particles
	int   count;		// Amount of particles to be generated
	float h;			// color hue (0..1)
	float dh;			// color hue range
	float l;			// color lightness (0..1)
	float dl;			// color lightness range
	float a;			// Alpha
	float da;			// Alpha range
	float x;			// X position
	float y;			// Y position
	float espeed;		// Speed of the emitter
	float eangle;		// Angle for the movement of this emitter
	int   movement;		// Type of movement of this emitter
	float dx;			// X range
	float dy;			// Y range
	float dcirc;		// Circular range
	float vx;			// X speed
	float vy;			// Y speed
	float vt;			// t speed (aging speed)
	float vphi;			// Rotation speed
	float dvx;			// X speed range
	float dvy;			// Y speed range
	float dvcirc;		// Circular speed range
	float dvt;			// t speed (aging speed) range
	float dvphi;		// Rotation speed range
	float s;			// size (side of the square)
	float ds;			// size (side of the square) range
	float snew;			// Size when born (reduced to s while new)
	float dsnew;		// Size when born (reduced to s while new) range
	float g;			// Gravity of particles
	float dg;			// Gravity range
	float gp;			// Part of particles that have gravity
};

class ParticleSystem
{
    public:

	ParticleSystem ();

	int      hardLimit;		// Not to be exceeded
	int      softLimit;		// If exceeded, old particles age faster
	int      lastCount;		// Living particle count to evaluate softLimit
	float    tnew;		// Particle is new if t > tnew
	float    told;		// Particle is old if t < told
	float    gx;		// Global gravity x
	float    gy;		// Global gravity y
	std::vector<Particle> particles;	// The actual particles
	GLuint   tex;		// Particle Texture
	bool     active, init;
	float    darken;		// Darken background
	GLuint   blendMode;
	std::vector<Emitter>  e;		// All emitters in here
	std::vector<GPoint>   g;		// All gravity point sources in here

	/* Cache used in drawParticles 
        It's here to avoid multiple mem allocation 
        during drawing */
	std::vector<GLfloat>  vertices_cache;
	std::vector<GLfloat>  coords_cache;
	std::vector<GLushort> colors_cache;
	std::vector<GLushort> dcolors_cache;

	void
	initParticles (int f_hardLimit, int f_softLimit);

	void
	drawParticles (const GLMatrix &transform);

	void
	updateParticles (float time);

	void
	genNewParticles (Emitter *e);

	void
	finiParticles ();

};

class WizardScreen :
    public PluginClassHandler <WizardScreen, CompScreen>,
    public WizardOptions,
    public ScreenInterface,
    public CompositeScreenInterface,
    public GLScreenInterface
{
    public:
	WizardScreen (CompScreen *screen);
	~WizardScreen ();

	CompositeScreen *cScreen;
	GLScreen	*gScreen;

	int mx, my; //Mouse Position from polling

	bool active;

	ParticleSystem ps;

	MousePoller pollHandle;

	void loadGPoints ();

	void loadEmitters ();

	void positionUpdate (const CompPoint &pos);

	void preparePaint (int time);

	void donePaint ();

	bool
	glPaintOutput (const GLScreenPaintAttrib	&sa,
		       const GLMatrix		&transform,
		       const CompRegion		&region,
		       CompOutput			*output,
		       unsigned int		mask);

	bool toggle ();

	void toggleFunctions(bool enabled);

	void
	optionChanged (CompOption	      *opt,
		       WizardOptions::Options num);
};

class WizardPluginVTable :
    public CompPlugin::VTableForScreen <WizardScreen>
{
    public:
	bool init ();
};

COMPIZ_PLUGIN_20090315 (wizard, WizardPluginVTable);
