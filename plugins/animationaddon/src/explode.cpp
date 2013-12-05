/*
 * Animation plugin for compiz/beryl
 *
 * explode.cpp
 *
 * Copyright : (C) 2006 Erkin Bahceci
 * E-mail    : erkinbah@gmail.com
 *
 * Based on Wobbly and Minimize plugins by
 *           : David Reveman
 * E-mail    : davidr@novell.com>
 *
 * Particle system added by : (C) 2006 Dennis Kasprzyk
 * E-mail                   : onestone@beryl-project.org
 *
 * Beam-Up added by : Florencio Guimaraes
 * E-mail           : florencio@nexcorp.com.br
 *
 * Hexagon tessellator added by : Mike Slegeir
 * E-mail                       : mikeslegeir@mail.utexas.edu>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "private.h"

// =====================  Effect: Explode  =========================

const float ExplodeAnim::kDurationFactor = 1.43;

ExplodeAnim::ExplodeAnim (CompWindow *w,
                          WindowEvent curWindowEvent,
                          float duration,
                          const AnimEffect info,
                          const CompRect &icon) :
    Animation::Animation (w, curWindowEvent, kDurationFactor * duration, info,
			  icon),
    PolygonAnim::PolygonAnim (w, curWindowEvent, kDurationFactor * duration,
			      info, icon)
{
    mAllFadeDuration = 0.3f;
    mDoDepthTest = true;
    mDoLighting = true;
    mCorrectPerspective = CorrectPerspectivePolygon;
    mBackAndSidesFadeDur = 0.2f;
}

void
ExplodeAnim::init ()
{
    switch (optValI (AnimationaddonOptions::ExplodeTessellation))
    {
    case AnimationaddonOptions::ExplodeTessellationRectangular:
	if (!tessellateIntoRectangles (optValI (AnimationaddonOptions::ExplodeGridx),
				       optValI (AnimationaddonOptions::ExplodeGridy),
				       optValF (AnimationaddonOptions::ExplodeThickness)))
	    return;
	break;
    case AnimationaddonOptions::ExplodeTessellationHexagonal:
	if (!tessellateIntoHexagons (optValI (AnimationaddonOptions::ExplodeGridx),
				     optValI (AnimationaddonOptions::ExplodeGridy),
				     optValF (AnimationaddonOptions::ExplodeThickness)))
	    return;
	break;
    case AnimationaddonOptions::ExplodeTessellationGlass:
	if (!tessellateIntoGlass (optValI (AnimationaddonOptions::ExplodeSpokes),
	                          optValI (AnimationaddonOptions::ExplodeTiers),
	                          optValF (AnimationaddonOptions::ExplodeThickness)))
	    return;
        break;
    default:
	return;
    }

    double sqrt2 = sqrt (2);
    float screenSizeFactor = (0.8 * DEFAULT_Z_CAMERA * ::screen->width ());

    foreach (PolygonObject *p, mPolygons)
    {
	p->rotAxis.set (RAND_FLOAT (), RAND_FLOAT (), RAND_FLOAT ());

	float speed = screenSizeFactor / 10 * (0.2 + RAND_FLOAT ());

	float xx = 2 * (p->centerRelPos.x () - 0.5);
	float yy = 2 * (p->centerRelPos.y () - 0.5);

	float x = speed * 2 * (xx + 0.5 * (RAND_FLOAT () - 0.5));
	float y = speed * 2 * (yy + 0.5 * (RAND_FLOAT () - 0.5));

	float distToCenter = sqrt (xx * xx + yy * yy) / sqrt2;
	float moveMult = 1 - distToCenter;
	moveMult = moveMult < 0 ? 0 : moveMult;
	float zbias = 0.1;
	float z = speed * 10 *
	    (zbias + RAND_FLOAT () *
	     pow (moveMult, 0.5));

	p->finalRelPos.set (x, y, z);
	p->finalRotAng = RAND_FLOAT () * 540 - 270;
    }
}
