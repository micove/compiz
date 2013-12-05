/*
 *
 * Compiz workspace name display plugin
 *
 * workspacenames.cpp
 *
 * Copyright : (C) 2008 by Danny Baumann
 * E-mail    : maniac@compiz-fusion.org
 * 
 * Ported to Compiz 0.9.x
 * Copyright : (c) 2010 Scott Moreau <oreaus@gmail.com>
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

#include "workspacenames.h"

CompString
WSNamesScreen::getCurrentWSName ()
{
    CompString ret;

    CompOption::Value::Vector vpNumbers = optionGetViewports ();
    CompOption::Value::Vector names     = optionGetNames ();

    int currentVp = screen->vp ().y () * screen->vpSize ().width () +
		    screen->vp ().x () + 1;

    int listSize  = MIN (vpNumbers.size (), names.size ());

    for (int i = 0; i < listSize; ++i)
    {
	if (vpNumbers[i].i () == currentVp)
	    return names[i].s ();
    }

    return ret;
}

void
WSNamesScreen::renderNameText ()
{
    CompText::Attrib attrib;

    textData.clear ();

    CompString name = getCurrentWSName ();

    if (name.empty ())
	return;

    /* 75% of the output device as maximum width */
    attrib.maxWidth  = screen->getCurrentOutputExtents ().width () * 3 / 4;
    attrib.maxHeight = 100;

    attrib.family = "Sans";
    attrib.size = optionGetTextFontSize ();

    attrib.color[0] = optionGetFontColorRed ();
    attrib.color[1] = optionGetFontColorGreen ();
    attrib.color[2] = optionGetFontColorBlue ();
    attrib.color[3] = optionGetFontColorAlpha ();

    attrib.flags = CompText::WithBackground | CompText::Ellipsized;

    if (optionGetBoldText ())
	attrib.flags |= CompText::StyleBold;

    attrib.bgHMargin = 15;
    attrib.bgVMargin = 15;
    attrib.bgColor[0] = optionGetBackColorRed ();
    attrib.bgColor[1] = optionGetBackColorGreen ();
    attrib.bgColor[2] = optionGetBackColorBlue ();
    attrib.bgColor[3] = optionGetBackColorAlpha ();

    textData.renderText (name, attrib);
}

CompPoint
WSNamesScreen::getTextPlacementPosition ()
{
    CompRect oe = screen->getCurrentOutputExtents ();
    float x = oe.centerX () - textData.getWidth () / 2;
    float y = 0;
    unsigned short verticalOffset = optionGetVerticalOffset ();

    switch (optionGetTextPlacement ())
    {
	case WorkspacenamesOptions::TextPlacementCenteredOnScreen:
	    y = oe.centerY () + textData.getHeight () / 2;
	    break;

	case WorkspacenamesOptions::TextPlacementTopOfScreenMinusOffset:
	case WorkspacenamesOptions::TextPlacementBottomOfScreenPlusOffset:
	    {
		CompRect workArea = screen->currentOutputDev ().workArea ();

		if (optionGetTextPlacement () ==
		    WorkspacenamesOptions::TextPlacementTopOfScreenMinusOffset)
		    y = oe.y1 () + workArea.y () +
			verticalOffset + textData.getHeight ();
		else /* TextPlacementBottomOfScreenPlusOffset */
		    y = oe.y1 () + workArea.y () +
			workArea.height () - verticalOffset;
	    }
	    break;

	default:
	    return CompPoint (floor (x),
			      oe.centerY () - textData.getHeight () / 2);
	    break;
    }

    return CompPoint (floor (x), floor (y));
}

void
WSNamesScreen::damageTextArea ()
{
    const CompPoint pos (getTextPlacementPosition ());

    /* The placement position is from the lower corner, so we
     * need to move it back up by height */
    CompRect        area (pos.x (),
			  pos.y () - textData.getHeight (),
			  textData.getWidth (),
			  textData.getHeight ());

    cScreen->damageRegion (area);
}

void
WSNamesScreen::drawText (const GLMatrix &matrix)
{
    GLfloat  alpha = 0.0f;

    /* assign y (for the lower corner!) according to the setting */
    const CompPoint p = getTextPlacementPosition ();

    if (timer)
	alpha = timer / (optionGetFadeTime () * 1000.0f);
    else if (timeoutHandle.active ())
	alpha = 1.0f;

    textData.draw (matrix, p.x (), p.y (), alpha);
}

bool
WSNamesScreen::shouldDrawText ()
{
    return textData.getWidth () && textData.getHeight ();
}

bool
WSNamesScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			      const GLMatrix            &transform,
			      const CompRegion          &region,
			      CompOutput                *output,
			      unsigned int              mask)
{
    bool status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    if (shouldDrawText ())
    {
	GLMatrix sTransform (transform);

	sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	drawText (sTransform);
    }

    return status;
}

void
WSNamesScreen::preparePaint (int msSinceLastPaint)
{
    if (timer)
    {
	timer -= msSinceLastPaint;
	timer = MAX (timer, 0);
    }

    cScreen->preparePaint (msSinceLastPaint);
}

void
WSNamesScreen::donePaint ()
{
    /* Only damage when the */
    if (shouldDrawText ())
	damageTextArea ();
 
     cScreen->donePaint ();

    /* Clear text data if done with fadeout */
    if (!timer && !timeoutHandle.active ())
	textData.clear ();
}

bool
WSNamesScreen::hideTimeout ()
{
    timer = optionGetFadeTime () * 1000;

    /* Clear immediately if there is no fadeout */
    if (!timer)
	textData.clear ();
 
    damageTextArea ();

    timeoutHandle.stop ();

    return false;
}

void
WSNamesScreen::handleEvent (XEvent *event)
{
    screen->handleEvent (event);

    if (event->type != PropertyNotify)
	return;

    if (event->xproperty.atom == Atoms::desktopViewport)
    {
	int timeout = optionGetDisplayTime () * 1000;

	timer = 0;

	if (timeoutHandle.active ())
	    timeoutHandle.stop ();

	renderNameText ();
	timeoutHandle.start (timeout, timeout + 200);

	damageTextArea ();
    }
}

WSNamesScreen::WSNamesScreen (CompScreen *screen) :
    PluginClassHandler <WSNamesScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    timer (0)
{
    ScreenInterface::setHandler (screen, true);
    CompositeScreenInterface::setHandler (cScreen, true);
    GLScreenInterface::setHandler (gScreen, true);

    timeoutHandle.start (boost::bind (&WSNamesScreen::hideTimeout, this),
			 0, 0);
}

WSNamesScreen::~WSNamesScreen ()
{
}

bool
WorkspacenamesPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("text", COMPIZ_TEXT_ABI))
	compLogMessage ("workspacenames", CompLogLevelWarn,
			"No compatible text plugin loaded");

    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	return true;

    return false;
}
