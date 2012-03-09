/*
 * Copyright © 2008 Dennis Kasprzyk
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Dennis Kasprzyk not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Dennis Kasprzyk makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DENNIS KASPRZYK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Dennis Kasprzyk <onestone@compiz-fusion.org>
 *          David Reveman <davidr@novell.com>
 */

#include "privates.h"

GLWindow::GLWindow (CompWindow *w) :
    PluginClassHandler<GLWindow, CompWindow, COMPIZ_OPENGL_ABI> (w),
    priv (new PrivateGLWindow (w, this))
{
    CompositeWindow *cw = CompositeWindow::get (w);

    priv->paint.opacity    = cw->opacity ();
    priv->paint.brightness = cw->brightness ();
    priv->paint.saturation = cw->saturation ();

    priv->lastPaint = priv->paint;

}

GLWindow::~GLWindow ()
{
    delete priv;
}

PrivateGLWindow::PrivateGLWindow (CompWindow *w,
				  GLWindow   *gw) :
    window (w),
    gWindow (gw),
    cWindow (CompositeWindow::get (w)),
    gScreen (GLScreen::get (screen)),
    textures (),
    regions (),
    updateReg (true),
    clip (),
    bindFailed (false),
    geometry (),
    icons ()
{
    paint.xScale	= 1.0f;
    paint.yScale	= 1.0f;
    paint.xTranslate	= 0.0f;
    paint.yTranslate	= 0.0f;

    WindowInterface::setHandler (w);
    CompositeWindowInterface::setHandler (cWindow);
}

PrivateGLWindow::~PrivateGLWindow ()
{
}

void
PrivateGLWindow::setWindowMatrix ()
{
    CompRect input (window->inputRect ());

    if (textures.size () != matrices.size ())
	matrices.resize (textures.size ());

    for (unsigned int i = 0; i < textures.size (); i++)
    {
	matrices[i] = textures[i]->matrix ();
	matrices[i].x0 -= (input.x () * matrices[i].xx);
	matrices[i].y0 -= (input.y () * matrices[i].yy);
    }
}

bool
GLWindow::bind ()
{
    if ((!priv->cWindow->pixmap () && !priv->cWindow->bind ()))
	return false;

    priv->textures =
	GLTexture::bindPixmapToTexture (priv->cWindow->pixmap (),
					priv->cWindow->size ().width (),
					priv->cWindow->size ().height (),
					priv->window->depth ());
    if (priv->textures.empty ())
    {
	compLogMessage ("opengl", CompLogLevelInfo,
			"Couldn't bind redirected window 0x%x to "
			"texture\n", (int) priv->window->id ());
    }

    priv->setWindowMatrix ();
    priv->updateReg = true;

    return true;
}

void
GLWindow::release ()
{
    priv->textures.clear ();

    if (priv->cWindow->pixmap ())
    {
	priv->cWindow->release ();
    }
}

bool
GLWindowInterface::glPaint (const GLWindowPaintAttrib &attrib,
			    const GLMatrix            &transform,
			    const CompRegion          &region,
			    unsigned int              mask)
    WRAPABLE_DEF (glPaint, attrib, transform, region, mask)

bool
GLWindowInterface::glDraw (const GLMatrix     &transform,
			   GLFragment::Attrib &fragment,
			   const CompRegion   &region,
			   unsigned int       mask)
    WRAPABLE_DEF (glDraw, transform, fragment, region, mask)

void
GLWindowInterface::glAddGeometry (const GLTexture::MatrixList &matrix,
				  const CompRegion            &region,
				  const CompRegion            &clip,
				  unsigned int                maxGridWidth,
				  unsigned int                maxGridHeight)
    WRAPABLE_DEF (glAddGeometry, matrix, region, clip,
		  maxGridWidth, maxGridHeight)

void
GLWindowInterface::glDrawTexture (GLTexture          *texture,
				  GLFragment::Attrib &fragment,
				  unsigned int       mask)
    WRAPABLE_DEF (glDrawTexture, texture, fragment, mask)

void
GLWindowInterface::glDrawGeometry ()
    WRAPABLE_DEF (glDrawGeometry)

const CompRegion &
GLWindow::clip () const
{
    return priv->clip;
}

GLWindowPaintAttrib &
GLWindow::paintAttrib ()
{
    return priv->paint;
}

GLWindowPaintAttrib &
GLWindow::lastPaintAttrib ()
{
    return priv->lastPaint;
}


void
PrivateGLWindow::resizeNotify (int dx, int dy, int dwidth, int dheight)
{
    window->resizeNotify (dx, dy, dwidth, dheight);
    setWindowMatrix ();
    updateReg = true;
    if (!window->hasUnmapReference ())
	gWindow->release ();
}

void
PrivateGLWindow::moveNotify (int dx, int dy, bool now)
{
    window->moveNotify (dx, dy, now);
    updateReg = true;
    setWindowMatrix ();
}

void
PrivateGLWindow::windowNotify (CompWindowNotify n)
{
    switch (n)
    {
	case CompWindowNotifyUnmap:
	case CompWindowNotifyReparent:
	case CompWindowNotifyUnreparent:
	case CompWindowNotifyFrameUpdate:
	    if (!window->hasUnmapReference ())
		gWindow->release ();
	    break;
	default:
	    break;
    }

    window->windowNotify (n);
}

void
GLWindow::updatePaintAttribs ()
{
    CompositeWindow *cw = CompositeWindow::get (priv->window);

    priv->paint.opacity    = cw->opacity ();
    priv->paint.brightness = cw->brightness ();
    priv->paint.saturation = cw->saturation ();
}

GLWindow::Geometry &
GLWindow::geometry ()
{
    return priv->geometry;
}

GLWindow::Geometry::Geometry () :
    vertices (NULL),
    vertexSize (0),
    vertexStride (0),
    indices (NULL),
    indexSize (0),
    vCount (0),
    texUnits (0),
    texCoordSize (0),
    indexCount (0)
{
}

GLWindow::Geometry::~Geometry ()
{
    if (vertices)
	free (vertices);

    if (indices)
	free (indices);
}

void
GLWindow::Geometry::reset ()
{
    vCount = indexCount = 0;
}

bool
GLWindow::Geometry::moreVertices (int newSize)
{
    if (newSize > vertexSize)
    {
	GLfloat *nVertices;

	nVertices = (GLfloat *)
	    realloc (vertices, sizeof (GLfloat) * newSize);
	if (!nVertices)
	    return false;

	vertices = nVertices;
	vertexSize = newSize;
    }

    return true;
}

bool
GLWindow::Geometry::moreIndices (int newSize)
{
    if (newSize > indexSize)
    {
	GLushort *nIndices;

	nIndices = (GLushort *)
	    realloc (indices, sizeof (GLushort) * newSize);
	if (!nIndices)
	    return false;

	indices = nIndices;
	indexSize = newSize;
    }

    return true;
}

const GLTexture::List &
GLWindow::textures () const
{
    return priv->textures;
}

const GLTexture::MatrixList &
GLWindow::matrices () const
{
    return priv->matrices;
}

GLTexture *
GLWindow::getIcon (int width, int height)
{
    GLIcon   icon;
    CompIcon *i = priv->window->getIcon (width, height);

    if (!i)
	return NULL;

    if (!i->width () || !i->height ())
	return NULL;

    foreach (GLIcon &icon, priv->icons)
	if (icon.icon == i)
	    return icon.textures[0];

    icon.icon = i;
    icon.textures = GLTexture::imageBufferToTexture ((char *) i->data (), *i);

    if (icon.textures.size () > 1 || icon.textures.size () == 0)
	return NULL;

    priv->icons.push_back (icon);

    return icon.textures[0];
}

void
PrivateGLWindow::updateFrameRegion (CompRegion &region)
{
    window->updateFrameRegion (region);
    updateReg = true;
}

void
PrivateGLWindow::updateWindowRegions ()
{
    CompRect input (window->inputRect ());

    if (regions.size () != textures.size ())
	regions.resize (textures.size ());
    for (unsigned int i = 0; i < textures.size (); i++)
    {
	regions[i] = CompRegion (*textures[i]);
	regions[i].translate (input.x (), input.y ());
	regions[i] &= window->region ();
    }
    updateReg = false;
}

unsigned int
GLWindow::lastMask () const
{
    return priv->lastMask;
}
