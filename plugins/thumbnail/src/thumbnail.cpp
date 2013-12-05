/*
 *
 * Compiz thumbnail plugin
 *
 * thumbnail.cpp
 *
 * Copyright : (C) 2007 by Dennis Kasprzyk
 * E-mail    : onestone@beryl-project.org
 *
 * Ported to Compiz 0.9
 * Copyright : (C) 2009 by Sam Spilsbury
 * E-mail    : smspillaz@gmail.com
 *
 * Based on thumbnail.c:
 * Copyright : (C) 2007 Stjepan Glavina
 * E-mail    : stjepang@gmail.com
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

#include "thumbnail.h"
#include "thumbnail_tex.h"

COMPIZ_PLUGIN_20090315 (thumbnail, ThumbPluginVTable);

void
ThumbScreen::freeThumbText (Thumbnail *t)
{
    if (!t->text)
	return;

    delete t->text;
    t->text = NULL;
}

void
ThumbScreen::renderThumbText (Thumbnail *t,
			      bool      freeThumb)
{
    if (!textPluginLoaded)
	return;

    if (freeThumb || !t->text)
    {
	freeThumbText (t);
	t->text = new CompText ();
    }

    CompText::Attrib tA;

    tA.maxWidth   = t->width;
    tA.maxHeight  = 100;

    // text background
    tA.bgHMargin  = 4;
    tA.bgVMargin  = 4;
    tA.bgColor[0] = optionGetFontBackgroundColorRed ();
    tA.bgColor[1] = optionGetFontBackgroundColorGreen ();
    tA.bgColor[2] = optionGetFontBackgroundColorBlue ();
    tA.bgColor[3] = optionGetFontBackgroundColorAlpha ();

    tA.size       = optionGetFontSize ();
    tA.color[0]   = optionGetFontColorRed ();
    tA.color[1]   = optionGetFontColorGreen ();
    tA.color[2]   = optionGetFontColorBlue ();
    tA.color[3]   = optionGetFontColorAlpha ();
    tA.flags      = CompText::WithBackground | CompText::Ellipsized;

    if (optionGetFontBold ())
	tA.flags |= CompText::StyleBold;

    tA.family     = "Sans";

    t->textValid = t->text->renderWindowTitle (t->win->id (), false, tA);
}

void
ThumbScreen::damageThumbRegion (Thumbnail  *t)
{
    int x      = t->x      - t->offset;
    int y      = t->y      - t->offset;
    int width  = t->width  + t->offset * 2;
    int height = t->height + t->offset * 2;

    CompRect rect (x, y, width, height);

    if (t->text)
	rect.setHeight (rect.height () + t->text->getHeight () + optionGetTextDistance ());

    CompRegion region (rect);

    cScreen->damageRegion (region);
}

#define GET_DISTANCE(a,b) \
    (sqrt((((a)[0] - (b)[0]) * ((a)[0] - (b)[0])) + \
	  (((a)[1] - (b)[1]) * ((a)[1] - (b)[1]))))

void
ThumbScreen::thumbUpdateThumbnail ()
{
    if (thumb.win == pointedWin ||
	(thumb.opacity && oldThumb.opacity))
	return;

    if (thumb.win)
	damageThumbRegion (&thumb);

    freeThumbText (&oldThumb);

    ThumbWindow *tw;

    if (oldThumb.win)
    {
	tw = ThumbWindow::get (oldThumb.win);

	/* Disable painting on the old thumb */
	tw->cWindow->damageRectSetEnabled (tw, false);
	tw->gWindow->glPaintSetEnabled (tw, false);
	tw->window->resizeNotifySetEnabled (tw, false);
    }

    oldThumb   = thumb;
    thumb.text = NULL;
    thumb.win  = pointedWin;
    thumb.dock = dock;

    if (!thumb.win || !dock)
    {
	thumb.win  = NULL;
	thumb.dock = NULL;
	return;
    }

    CompWindow *w = thumb.win;
    tw = ThumbWindow::get (w);

    tw->cWindow->damageRectSetEnabled (tw, true);
    tw->gWindow->glPaintSetEnabled (tw, true);
    tw->window->resizeNotifySetEnabled (tw, true);

    float  maxSize = optionGetThumbSize ();
    double scale   = 1.0;

    int winWidth  = w->width ()  + w->border ().left + w->border ().right;
    int winHeight = w->height () + w->border ().top  + w->border ().bottom;

    /* do we need to scale the window down? */
    if (winWidth > maxSize || winHeight > maxSize)
    {
	if (winWidth >= winHeight)
	    scale = maxSize / winWidth;
	else
	    scale = maxSize / winHeight;
    }

    thumb.width  = winWidth  * scale;
    thumb.height = winHeight * scale;
    thumb.scale  = scale;

    if (optionGetTitleEnabled ())
	renderThumbText (&thumb, false);
    else
	freeThumbText (&thumb);

    int igMidPoint[2], tMidPoint[2];

    igMidPoint[0] = w->iconGeometry ().centerX ();
    igMidPoint[1] = w->iconGeometry ().centerY ();

    int off  = optionGetBorder ();
    int oDev = screen->outputDeviceForPoint (igMidPoint[0],
					     igMidPoint[1]);

    CompRect oGeom;

    if (screen->outputDevs ().size () == 1 ||
	(unsigned int) oDev > screen->outputDevs ().size ())
	oGeom.setGeometry (0, 0, screen->width (), screen->height ());
    else
	oGeom = screen->outputDevs ()[oDev];

    int tHeight = thumb.height;
    int tWidth  = thumb.width;

    if (thumb.text)
	tHeight += thumb.text->getHeight () + optionGetTextDistance ();

    int halfTWidth  = tWidth  / 2;
    int halfTHeight = tHeight / 2;
    int tPos[2], tmpPos[2];

    // failsave position
    tPos[0] = igMidPoint[0] - halfTWidth;

    if (w->iconGeometry ().y () - tHeight >= 0)
	tPos[1] = w->iconGeometry ().y () - tHeight;
    else
	tPos[1] = w->iconGeometry ().y () + w->iconGeometry ().height ();

    // above
    tmpPos[0] = igMidPoint[0] - halfTWidth;

    if (tmpPos[0] - off < oGeom.x1 ())
	tmpPos[0] = oGeom.x1 () + off;

    if (tmpPos[0] + off + tWidth > oGeom.x2 ())
    {
	if (tWidth + (2 * off) <= oGeom.width ())
	    tmpPos[0] = oGeom.x2 () - tWidth - off;
	else
	    tmpPos[0] = oGeom.x1 () + off;
    }

    tMidPoint[0] = tmpPos[0] + halfTWidth;

    int dockX      = dock->x () - dock->border ().left;
    int dockY      = dock->y () - dock->border ().top;
    int dockWidth  = dock->width ()  + dock->border ().left + dock->border ().right;
    int dockHeight = dock->height () + dock->border ().top  + dock->border ().bottom;

    tmpPos[1]    = dockY - tHeight - off;
    tMidPoint[1] = tmpPos[1] + halfTHeight;

    float distance = 1000000;

    if (tmpPos[1] > oGeom.y1 ())
    {
	tPos[0]  = tmpPos[0];
	tPos[1]  = tmpPos[1];
	distance = GET_DISTANCE (igMidPoint, tMidPoint);
    }

    // below
    tmpPos[1] = dockY + dockHeight + off;

    tMidPoint[1] = tmpPos[1] + halfTHeight;

    if (tmpPos[1] + tHeight + off < oGeom.y2 () &&
	GET_DISTANCE (igMidPoint, tMidPoint) < distance)
    {
	tPos[0]  = tmpPos[0];
	tPos[1]  = tmpPos[1];
	distance = GET_DISTANCE (igMidPoint, tMidPoint);
    }

    // left
    tmpPos[1] = igMidPoint[1] - halfTHeight;

    if (tmpPos[1] - off < oGeom.y1 ())
	tmpPos[1] = oGeom.y1 () + off;

    if (tmpPos[1] + off + tHeight > oGeom.y2 ())
    {
	if (tHeight + (2 * off) <= oGeom.height ())
	    tmpPos[1] = oGeom.y2 () - tHeight - off;
	else
	    tmpPos[1] = oGeom.y1 () + off;
    }

    tMidPoint[1] = tmpPos[1] + halfTHeight;
    tmpPos[0]    = dockX - tWidth - off;
    tMidPoint[0] = tmpPos[0] + halfTWidth;

    if (tmpPos[0] > oGeom.x1 () && GET_DISTANCE (igMidPoint, tMidPoint) < distance)
    {
	tPos[0]  = tmpPos[0];
	tPos[1]  = tmpPos[1];
	distance = GET_DISTANCE (igMidPoint, tMidPoint);
    }

    // right
    tmpPos[0]    = dockX + dockWidth + off;

    tMidPoint[0] = tmpPos[0] + halfTWidth;

    if (tmpPos[0] + tWidth + off < oGeom.x2 () &&
	GET_DISTANCE (igMidPoint, tMidPoint) < distance)
    {
	tPos[0]  = tmpPos[0];
	tPos[1]  = tmpPos[1];
    }

    thumb.x       = tPos[0];
    thumb.y       = tPos[1];
    thumb.offset  = off;
    thumb.opacity = 0.0;

    damageThumbRegion (&thumb);

    cScreen->preparePaintSetEnabled (this, true);
    cScreen->donePaintSetEnabled (this, true);
    gScreen->glPaintOutputSetEnabled (this, true);
}

bool
ThumbScreen::thumbShowThumbnail ()
{
    showingThumb = true;

    thumbUpdateThumbnail ();
    damageThumbRegion (&thumb);

    return false;
}

bool
ThumbScreen::checkPosition (CompWindow *w)
{
    if (optionGetCurrentViewport ())
	if (w->serverX () >= screen->width ()	    ||
	    w->serverX () + w->serverWidth ()  <= 0 ||
	    w->serverY () >= screen->height ()	    ||
	    w->serverY () + w->serverHeight () <= 0)
	    return false;

    return true;
}

void
ThumbScreen::positionUpdate (const CompPoint &p)
{
    CompWindow *found = NULL;

    foreach (CompWindow *cw, screen->windows ())
    {
	THUMB_WINDOW (cw);

	if (cw->destroyed ()				    ||
	    cw->iconGeometry ().isEmpty ()		    ||
	    !cw->isMapped ()				    ||
	    cw->state () & CompWindowStateSkipTaskbarMask   ||
	    cw->state () & CompWindowStateSkipPagerMask	    ||
	    !cw->managed ()				    ||
	    !tw->cWindow->pixmap ())
	    continue;

	if (cw->iconGeometry ().contains (p) &&
	    checkPosition (cw))
	{
	    found = cw;
	    break;
	}
    }

    if (found)
    {
	int showDelay = optionGetShowDelay ();

	if (!showingThumb &&
	    !(thumb.opacity != 0.0 && thumb.win == found))
	{
	    if (displayTimeout.active ())
	    {
		if (pointedWin != found)
		{
		    displayTimeout.stop ();
		    displayTimeout.start (boost::bind (&ThumbScreen::thumbShowThumbnail,
						       this), showDelay, showDelay + 500);
		}
	    }
	    else
	    {
		displayTimeout.stop ();
		displayTimeout.start (boost::bind (&ThumbScreen::thumbShowThumbnail,
						   this), showDelay, showDelay + 500);
	    }
	}

	pointedWin = found;
	thumbUpdateThumbnail ();
    }
    else
    {
	if (displayTimeout.active ())
	    displayTimeout.stop ();

	pointedWin   = NULL;
	showingThumb = false;

	cScreen->preparePaintSetEnabled (this, true);
	cScreen->donePaintSetEnabled (this, true);
    }
}

void
ThumbWindow::resizeNotify (int dx,
			   int dy,
			   int dwidth,
			   int dheight)
{
    THUMB_SCREEN (screen);

    ts->thumbUpdateThumbnail ();

    window->resizeNotify (dx, dy, dwidth, dheight);
}

void
ThumbScreen::handleEvent (XEvent *event)
{
    screen->handleEvent (event);

    CompWindow *w;

    switch (event->type)
    {
	case PropertyNotify:
	    if (event->xproperty.atom == Atoms::wmName)
	    {
		w = screen->findWindow (event->xproperty.window);

		if (w && thumb.win == w && optionGetTitleEnabled ())
		    renderThumbText (&thumb, true);
	    }

	    break;

	case ButtonPress:
	{
	    if (displayTimeout.active ())
		displayTimeout.stop ();

	    pointedWin   = NULL;
	    showingThumb = false;
	}
	    break;

	case EnterNotify:
	    w = screen->findWindow (event->xcrossing.window);

	    if (w)
	    {
		if (w->wmType () & CompWindowTypeDockMask)
		{
		    if (dock != w)
		    {
			dock = w;

			if (displayTimeout.active ())
			    displayTimeout.stop ();

			pointedWin   = NULL;
			showingThumb = false;
		    }

		    if (!poller.active ())
			poller.start ();
		}
		else
		{
		    dock = NULL;

		    if (displayTimeout.active ())
			displayTimeout.stop ();

		    pointedWin   = NULL;
		    showingThumb = false;

		    if (poller.active ())
			poller.stop ();
		}
	    }

	    break;

	case LeaveNotify:
	    w = screen->findWindow (event->xcrossing.window);

	    if (w && (w->wmType () & CompWindowTypeDockMask))
	    {
		dock = NULL;

		if (displayTimeout.active ())
		    displayTimeout.stop ();

		pointedWin   = NULL;
		showingThumb = false;

		cScreen->preparePaintSetEnabled (this, true);
		cScreen->donePaintSetEnabled (this, true);

		if (poller.active ())
		    poller.stop ();
	    }

	    break;

	default:
	    break;
    }
}

void
ThumbScreen::paintTexture (const GLMatrix &transform,
			   GLushort       *color,
			   int            wx,
			   int            wy,
			   int            width,
			   int            height,
			   int            off)
{
    GLfloat textureData[8];
    GLfloat vertexData[12];

    GLfloat wxPlusWidth    = wx + width;
    GLfloat wyPlusHeight   = wy + height;
    GLfloat wxPlusWPlusOff = wxPlusWidth  + off;
    GLfloat wyPlusHPlusOff = wyPlusHeight + off;
    GLfloat wxMinusOff     = wx - off;
    GLfloat wyMinusOff     = wy - off;

    GLVertexBuffer *streamingBuffer = GLVertexBuffer::streamingBuffer ();

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 1;
    textureData[1] = 1;

    vertexData[0]  = wx;
    vertexData[1]  = wy;
    vertexData[2]  = 0;
    vertexData[3]  = wx;
    vertexData[4]  = wyPlusHeight;
    vertexData[5]  = 0;
    vertexData[6]  = wxPlusWidth;
    vertexData[7]  = wy;
    vertexData[8]  = 0;
    vertexData[9]  = wxPlusWidth;
    vertexData[10] = wyPlusHeight;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 1, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 0;
    textureData[1] = 0;
    textureData[2] = 0;
    textureData[3] = 1;
    textureData[4] = 1;
    textureData[5] = 0;
    textureData[6] = 1;
    textureData[7] = 1;

    vertexData[0]  = wxMinusOff;
    vertexData[1]  = wyMinusOff;
    vertexData[2]  = 0;
    vertexData[3]  = wxMinusOff;
    vertexData[4]  = wy;
    vertexData[5]  = 0;
    vertexData[6]  = wx;
    vertexData[7]  = wyMinusOff;
    vertexData[8]  = 0;
    vertexData[9]  = wx;
    vertexData[10] = wy;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 1;
    textureData[1] = 0;
    textureData[2] = 1;
    textureData[3] = 1;
    textureData[4] = 0;
    textureData[5] = 0;
    textureData[6] = 0;
    textureData[7] = 1;

    vertexData[0]  = wxPlusWidth;
    vertexData[1]  = wyMinusOff;
    vertexData[2]  = 0;
    vertexData[3]  = wxPlusWidth;
    vertexData[4]  = wy;
    vertexData[5]  = 0;
    vertexData[6]  = wxPlusWPlusOff;
    vertexData[7]  = wyMinusOff;
    vertexData[8]  = 0;
    vertexData[9]  = wxPlusWPlusOff;
    vertexData[10] = wy;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 0;
    textureData[1] = 1;
    textureData[2] = 0;
    textureData[3] = 0;
    textureData[4] = 1;
    textureData[5] = 1;
    textureData[6] = 1;
    textureData[7] = 0;

    vertexData[0]  = wxMinusOff;
    vertexData[1]  = wyPlusHeight;
    vertexData[2]  = 0;
    vertexData[3]  = wxMinusOff;
    vertexData[4]  = wyPlusHPlusOff;
    vertexData[5]  = 0;
    vertexData[6]  = wx;
    vertexData[7]  = wyPlusHeight;
    vertexData[8]  = 0;
    vertexData[9]  = wx;
    vertexData[10] = wyPlusHPlusOff;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 1;
    textureData[1] = 1;
    textureData[2] = 1;
    textureData[3] = 0;
    textureData[4] = 0;
    textureData[5] = 1;
    textureData[6] = 0;
    textureData[7] = 0;

    vertexData[0]  = wxPlusWidth;
    vertexData[1]  = wyPlusHeight;
    vertexData[2]  = 0;
    vertexData[3]  = wxPlusWidth;
    vertexData[4]  = wyPlusHPlusOff;
    vertexData[5]  = 0;
    vertexData[6]  = wxPlusWPlusOff;
    vertexData[7]  = wyPlusHeight;
    vertexData[8]  = 0;
    vertexData[9]  = wxPlusWPlusOff;
    vertexData[10] = wyPlusHPlusOff;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 1;
    textureData[1] = 0;
    textureData[2] = 1;
    textureData[3] = 1;
    textureData[4] = 1;
    textureData[5] = 0;
    textureData[6] = 1;
    textureData[7] = 1;

    vertexData[0]  = wx;
    vertexData[1]  = wyMinusOff;
    vertexData[2]  = 0;
    vertexData[3]  = wx;
    vertexData[4]  = wy;
    vertexData[5]  = 0;
    vertexData[6]  = wxPlusWidth;
    vertexData[7]  = wyMinusOff;
    vertexData[8]  = 0;
    vertexData[9]  = wxPlusWidth;
    vertexData[10] = wy;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 1;
    textureData[1] = 1;
    textureData[2] = 1;
    textureData[3] = 0;
    textureData[4] = 1;
    textureData[5] = 1;
    textureData[6] = 1;
    textureData[7] = 0;

    vertexData[0]  = wx;
    vertexData[1]  = wyPlusHeight;
    vertexData[2]  = 0;
    vertexData[3]  = wx;
    vertexData[4]  = wyPlusHPlusOff;
    vertexData[5]  = 0;
    vertexData[6]  = wxPlusWidth;
    vertexData[7]  = wyPlusHeight;
    vertexData[8]  = 0;
    vertexData[9]  = wxPlusWidth;
    vertexData[10] = wyPlusHPlusOff;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 0;
    textureData[1] = 1;
    textureData[2] = 0;
    textureData[3] = 1;
    textureData[4] = 1;
    textureData[5] = 1;
    textureData[6] = 1;
    textureData[7] = 1;

    vertexData[0]  = wxMinusOff;
    vertexData[1]  = wy;
    vertexData[2]  = 0;
    vertexData[3]  = wxMinusOff;
    vertexData[4]  = wyPlusHeight;
    vertexData[5]  = 0;
    vertexData[6]  = wx;
    vertexData[7]  = wy;
    vertexData[8]  = 0;
    vertexData[9]  = wx;
    vertexData[10] = wyPlusHeight;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);

    streamingBuffer->begin (GL_TRIANGLE_STRIP);

    textureData[0] = 1;
    textureData[1] = 1;
    textureData[2] = 1;
    textureData[3] = 1;
    textureData[4] = 0;
    textureData[5] = 1;
    textureData[6] = 0;
    textureData[7] = 1;

    vertexData[0]  = wxPlusWidth;
    vertexData[1]  = wy;
    vertexData[2]  = 0;
    vertexData[3]  = wxPlusWidth;
    vertexData[4]  = wyPlusHeight;
    vertexData[5]  = 0;
    vertexData[6]  = wxPlusWPlusOff;
    vertexData[7]  = wy;
    vertexData[8]  = 0;
    vertexData[9]  = wxPlusWPlusOff;
    vertexData[10] = wyPlusHeight;
    vertexData[11] = 0;

    streamingBuffer->addTexCoords (0, 4, textureData);
    streamingBuffer->addVertices (4, vertexData);
    streamingBuffer->addColors (1, color);

    streamingBuffer->end ();
    streamingBuffer->render (transform);
}

void
ThumbScreen::thumbPaintThumb (Thumbnail      *t,
		 	      const GLMatrix *transform)
{
    CompWindow *w = t->win;

    if (!w)
	return;

    GLWindow *gWindow = GLWindow::get (w);

    GLushort color[4];

    int wx = t->x;
    int wy = t->y;

    GLWindowPaintAttrib sAttrib;
    unsigned int mask = PAINT_WINDOW_TRANSFORMED_MASK |
			PAINT_WINDOW_TRANSLUCENT_MASK;

    sAttrib = gWindow->paintAttrib ();

    /* Wrap drawWindowGeometry to make sure the general
       drawWindowGeometry function is used */
    unsigned int addWindowGeometryIndex = gWindow->glAddGeometryGetCurrentIndex ();

    if (!gWindow->textures ().empty ())
    {
	GLMatrix  wTransform (*transform);
	GLboolean glBlendEnabled = glIsEnabled (GL_BLEND);

	/* just enable blending if it is currently disabled */
	if (!glBlendEnabled)
	    glEnable (GL_BLEND);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int   off        = t->offset;
	float backheight = t->height;	// background/glow height
	float width      = t->width;

	if (optionGetWindowLike ())
	{
	    color[0] = 1;
	    color[1] = 1;
	    color[2] = 1;
	    color[3] = t->opacity * 65535;

	    foreach (GLTexture *tex, windowTexture)
	    {
		tex->enable (GLTexture::Good);
		paintTexture (*transform, color, wx, wy, width, backheight, off);
		tex->disable ();
	    }
	}
	else
	{
	    color[0] = optionGetThumbColorRed ();
	    color[1] = optionGetThumbColorGreen ();
	    color[2] = optionGetThumbColorBlue ();
	    color[3] = optionGetThumbColorAlpha () * t->opacity;

	    foreach (GLTexture *tex, glowTexture)
	    {
		tex->enable (GLTexture::Good);
		paintTexture (*transform, color, wx, wy, width, backheight, off);
		tex->disable ();
	    }
	}

	glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	/* we disable blending only, if it was disabled before */
	if (!glBlendEnabled)
	    glDisable (GL_BLEND);

	if (t->text)
	{
	    float ox     = 0.0f;
	    float height = backheight + t->text->getHeight () + optionGetTextDistance ();

	    if (t->text->getWidth () < width)
		ox = (width - t->text->getWidth ()) / 2.0;

	    t->text->draw (*transform, wx + ox, wy + height, t->opacity);
	}

	gScreen->setTexEnvMode (GL_REPLACE);

	sAttrib.opacity *= t->opacity;
	sAttrib.yScale   = t->scale;
	sAttrib.xScale   = t->scale;

	sAttrib.xTranslate = wx - w->x () + w->border ().left * sAttrib.xScale;
	sAttrib.yTranslate = wy - w->y () + w->border ().top  * sAttrib.yScale;

	GLenum filter = gScreen->textureFilter ();

	/* we just need to change the texture filter, if
	 * thumbnail mipmapping is enabled */
	if (optionGetMipmap ())
	    gScreen->setTextureFilter (GL_LINEAR_MIPMAP_LINEAR);

	wTransform.translate (w->x (), w->y (), 0.0f);
	wTransform.scale (sAttrib.xScale, sAttrib.yScale, 1.0f);
	wTransform.translate (sAttrib.xTranslate / sAttrib.xScale - w->x (),
			      sAttrib.yTranslate / sAttrib.yScale - w->y (),
			      0.0f);

	/* XXX: replacing the addWindowGeometry function like this is
	   very ugly but necessary until the vertex stage has been made
	   fully pluggable. */
	gWindow->glAddGeometrySetCurrentIndex (MAXSHORT);
	gWindow->glDraw (wTransform, sAttrib, infiniteRegion, mask);

	gScreen->setTextureFilter (filter);
    }

    gWindow->glAddGeometrySetCurrentIndex (addWindowGeometryIndex);
}

/* From here onwards */

void
ThumbScreen::preparePaint (int ms)
{
    float val = ms;

    val /= 1000;
    val /= optionGetFadeSpeed ();

    /*if (screen->otherGrabExist ("")) // shouldn't there be a s->grabs.empty () or something?
    {
	dock = NULL;

	if (displayTimeout.active ())
	{
	    displayTimeout.stop ();
	}

	pointedWin   = 0;
	showingThumb = false;
    }*/

    if (showingThumb && thumb.win == pointedWin)
	thumb.opacity = MIN (1.0, thumb.opacity + val);

    if (!showingThumb || thumb.win != pointedWin)
    {
	thumb.opacity = MAX (0.0, thumb.opacity - val);

	if (thumb.opacity == 0.0)
	    thumb.win = NULL;
    }

    if (oldThumb.opacity > 0.0f)
    {
	oldThumb.opacity = MAX (0.0, oldThumb.opacity - val);

	if (oldThumb.opacity == 0.0)
	{
	    damageThumbRegion (&oldThumb);
	    freeThumbText (&oldThumb);
	    oldThumb.win = NULL;
	}
    }

    if (oldThumb.win == NULL && thumb.win == NULL)
    {
	cScreen->preparePaintSetEnabled (this, false);
	cScreen->donePaintSetEnabled (this, false);
	gScreen->glPaintOutputSetEnabled (this, false);
    }

    cScreen->preparePaint (ms);
}

void
ThumbScreen::donePaint ()
{
    std::vector <Thumbnail *> damageThumbs;

    if (thumb.opacity)
	damageThumbs.push_back (&thumb);

    if (oldThumb.opacity)
	damageThumbs.push_back (&oldThumb);

    if (!damageThumbs.empty ())
    {
	foreach (Thumbnail *t, damageThumbs)
	    damageThumbRegion (t);
    }
    else
    {
	cScreen->preparePaintSetEnabled (this, false);
	cScreen->donePaintSetEnabled (this, false);
    }

    cScreen->donePaint ();
}

bool
ThumbScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			    const GLMatrix            &transform,
			    const CompRegion          &region,
			    CompOutput                *output,
			    unsigned int              mask)
{
    unsigned int newMask = mask;

    painted = false;

    x = screen->vp ().x ();
    y = screen->vp ().y ();

    if ((oldThumb.opacity > 0.0 && oldThumb.win) ||
	(thumb.opacity > 0.0 && thumb.win))
	newMask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

    bool status = gScreen->glPaintOutput (attrib, transform, region, output, newMask);

    if (optionGetAlwaysOnTop () && !painted)
    {
	if (oldThumb.opacity > 0.0 && oldThumb.win)
	{
	    GLMatrix sTransform = transform;

	    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);
	    thumbPaintThumb (&oldThumb, &sTransform);
	}

	if (thumb.opacity > 0.0 && thumb.win)
	{
	    GLMatrix sTransform = transform;

	    sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);
	    thumbPaintThumb (&thumb, &sTransform);
	}
    }

    return status;
}

void
ThumbScreen::glPaintTransformedOutput (const GLScreenPaintAttrib &attrib,
				       const GLMatrix            &transform,
				       const CompRegion          &region,
				       CompOutput                *output,
				       unsigned int              mask)
{
    gScreen->glPaintTransformedOutput (attrib, transform, region, output, mask);

    if (optionGetAlwaysOnTop () && x == screen->vp ().x () &&
				   y == screen->vp ().y ())
    {
	painted = true;

	if (oldThumb.opacity && oldThumb.win)
	{
	    GLMatrix sTransform = transform;

	    gScreen->glApplyTransform (attrib, output, &sTransform);
	    sTransform.toScreenSpace(output, -attrib.zTranslate);
	    thumbPaintThumb (&oldThumb, &sTransform);
	}

	if (thumb.opacity && thumb.win)
	{
	    GLMatrix sTransform = transform;

	    gScreen->glApplyTransform (attrib, output, &sTransform);
	    sTransform.toScreenSpace(output, -attrib.zTranslate);
	    thumbPaintThumb (&thumb, &sTransform);
	}
    }
}

bool
ThumbWindow::glPaint (const GLWindowPaintAttrib &attrib,
		      const GLMatrix            &transform,
		      const CompRegion          &region,
		      unsigned int              mask)
{
    THUMB_SCREEN (screen);

    bool status = gWindow->glPaint (attrib, transform, region, mask);

    if (!ts->optionGetAlwaysOnTop ()	&&
	ts->x == screen->vp ().x ()	&&
	ts->y == screen->vp ().y ())
    {
	GLMatrix sTransform = transform;

	if (ts->oldThumb.opacity    &&
	    ts->oldThumb.win	    &&
	    ts->oldThumb.dock == window)
	    ts->thumbPaintThumb (&ts->oldThumb, &sTransform);

	if (ts->thumb.opacity	&&
	    ts->thumb.win	&&
	    ts->thumb.dock == window)
	    ts->thumbPaintThumb (&ts->thumb, &sTransform);
    }

    return status;
}

bool
ThumbWindow::damageRect (bool           initial,
			 const CompRect &rect)
{
    THUMB_SCREEN (screen);

    if (ts->thumb.win == window && ts->thumb.opacity)
	ts->damageThumbRegion (&ts->thumb);

    if (ts->oldThumb.win == window && ts->oldThumb.opacity)
	ts->damageThumbRegion (&ts->oldThumb);

    return cWindow->damageRect (initial, rect);
}

ThumbScreen::ThumbScreen (CompScreen *screen) :
    PluginClassHandler <ThumbScreen, CompScreen> (screen),
    gScreen (GLScreen::get (screen)),
    cScreen (CompositeScreen::get (screen)),
    dock (NULL),
    pointedWin (NULL),
    showingThumb (false),
    painted (false),
    glowTexture (GLTexture::imageDataToTexture
		 (glowTex, CompSize (32, 32), GL_RGBA, GL_UNSIGNED_BYTE)),
    windowTexture (GLTexture::imageDataToTexture
		   (windowTex, CompSize (32, 32), GL_RGBA, GL_UNSIGNED_BYTE)),
    x (0),
    y (0)
{
    ScreenInterface::setHandler (screen);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);

    thumb.win    = NULL;
    oldThumb.win = NULL;

    thumb.text    = NULL;
    oldThumb.text = NULL;

    thumb.opacity    = 0.0f;
    oldThumb.opacity = 0.0f;

    poller.setCallback (boost::bind (&ThumbScreen::positionUpdate, this, _1));
}

ThumbScreen::~ThumbScreen ()
{
    poller.stop ();
    displayTimeout.stop ();

    freeThumbText (&thumb);
    freeThumbText (&oldThumb);
}

ThumbWindow::ThumbWindow (CompWindow *window) :
    PluginClassHandler <ThumbWindow, CompWindow> (window),
    window (window),
    cWindow (CompositeWindow::get (window)),
    gWindow (GLWindow::get (window))
{
    WindowInterface::setHandler (window, false);
    CompositeWindowInterface::setHandler (cWindow, false);
    GLWindowInterface::setHandler (gWindow, false);
}

ThumbWindow::~ThumbWindow ()
{
    THUMB_SCREEN (screen);

    if (ts->thumb.win == window)
    {
	ts->damageThumbRegion (&ts->thumb);
	ts->thumb.win     = NULL;
	ts->thumb.opacity = 0;
    }

    if (ts->oldThumb.win == window)
    {
	ts->damageThumbRegion (&ts->oldThumb);
	ts->oldThumb.win     = NULL;
	ts->oldThumb.opacity = 0;
    }

    if (ts->pointedWin == window)
	ts->pointedWin = NULL;
}

bool
ThumbPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("text", COMPIZ_TEXT_ABI))
	textPluginLoaded = true;
    else
	textPluginLoaded = false;

    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI)	&&
	CompPlugin::checkPluginABI ("mousepoll", COMPIZ_MOUSEPOLL_ABI))
	return true;

    return false;
}
