/*
 *
 * Compiz show repainted regions plugin
 *
 * showrepainted.c
 *
 * Copyright : (C) 2008 by Dennis Kasprzyk
 * E-mail    : onestone@compiz-fusion.org
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

#include "showrepaint.h"

COMPIZ_PLUGIN_20090315 (showrepaint, ShowrepaintPluginVTable);

bool
ShowrepaintScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
				  const GLMatrix            &transform,
				  const CompRegion          &region,
				  CompOutput                *output,
				  unsigned int               mask)
{
    bool           status;
    GLMatrix       sTransform; // initially identity matrix
    unsigned short color[4];

    status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    tmpRegion = region.intersected (*output);

    if (tmpRegion.isEmpty ())
	return status;

    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

    color[3] = optionGetIntensity () * 0xffff / 100;
    color[0] = (rand () & 7) * color[3] / 8;
    color[1] = (rand () & 7) * color[3] / 8;
    color[2] = (rand () & 7) * color[3] / 8;

    GLboolean glBlendEnabled = glIsEnabled (GL_BLEND);

    if (!glBlendEnabled)
	glEnable (GL_BLEND);

    std::vector<GLfloat> vertices;
    /* for each rectangle, use two triangles to display it */
    foreach (const CompRect &box, tmpRegion.rects ())
    {
	//first triangle
	vertices.push_back (box.x1 ());
	vertices.push_back (box.y1 ());
	vertices.push_back (0.0f);

	vertices.push_back (box.x1 ());
	vertices.push_back (box.y2 ());
	vertices.push_back (0.0f);

	vertices.push_back (box.x2 ());
	vertices.push_back (box.y2 ());
	vertices.push_back (0.0f);

	//second triangle
	vertices.push_back (box.x2 ());
	vertices.push_back (box.y2 ());
	vertices.push_back (0.0f);

	vertices.push_back (box.x2 ());
	vertices.push_back (box.y1 ());
	vertices.push_back (0.0f);

	vertices.push_back (box.x1 ());
	vertices.push_back (box.y1 ());
	vertices.push_back (0.0f);
    }

    GLVertexBuffer *stream = GLVertexBuffer::streamingBuffer ();

    stream->begin (GL_TRIANGLES);
    stream->color4f ((float)color[0] / 65535.0f, (float)color[1] / 65535.0f, (float)color[2] / 65535.0f, (float)color[3] / 65535.0f);
    stream->addVertices (vertices.size () / 3, &vertices[0]);
    if (stream->end ())
	stream->render (sTransform);

    stream->colorDefault ();

    /* only disable blending if it was disabled before */
    if (!glBlendEnabled)
	glDisable (GL_BLEND);

    return status;
}

bool
ShowrepaintScreen::toggle (CompAction         *action,
			   CompAction::State  state,
			   CompOption::Vector options)
{
    active = !active;
    gScreen->glPaintOutputSetEnabled (this, active);

    if (!active)
    {
	// Turning off show-repaint mode, so request the screen to be repainted
	cScreen->damageScreen ();
    }

    return true;
}

ShowrepaintScreen::ShowrepaintScreen (CompScreen *screen) :
    PluginClassHandler <ShowrepaintScreen, CompScreen> (screen),
    ShowrepaintOptions (),
    active (false),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen))
{
    GLScreenInterface::setHandler (gScreen, false);

    optionSetToggleKeyInitiate (boost::bind (&ShowrepaintScreen::toggle,  \
					     this, _1, _2, _3));
}

ShowrepaintScreen::~ShowrepaintScreen ()
{
    // Request the screen to be repainted on exit
    cScreen->damageScreen ();
}

bool
ShowrepaintPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	return true;

    return false;
}
