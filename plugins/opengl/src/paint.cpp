/*
 * Copyright © 2005 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <core/core.h>
#include <opengl/opengl.h>

#include "privates.h"


GLScreenPaintAttrib defaultScreenPaintAttrib = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -DEFAULT_Z_CAMERA
};

GLWindowPaintAttrib GLWindow::defaultPaintAttrib = {
    OPAQUE, BRIGHT, COLOR, 1.0f, 1.0f, 0.0f, 0.0f
};

void
GLScreen::glApplyTransform (const GLScreenPaintAttrib &sAttrib,
			    CompOutput                *output,
			    GLMatrix                  *transform)
{
    WRAPABLE_HND_FUNC (2, glApplyTransform, sAttrib, output, transform)

    transform->translate (sAttrib.xTranslate,
			  sAttrib.yTranslate,
			  sAttrib.zTranslate + sAttrib.zCamera);
    transform->rotate (sAttrib.xRotate, 0.0f, 1.0f, 0.0f);
    transform->rotate (sAttrib.vRotate,
		       cosf (sAttrib.xRotate * DEG2RAD),
		       0.0f,
		       sinf (sAttrib.xRotate * DEG2RAD));
    transform->rotate (sAttrib.yRotate, 0.0f, 1.0f, 0.0f);
}

void
PrivateGLScreen::paintBackground (const CompRegion &region,
				  bool             transformed)
{
    BoxPtr    pBox = const_cast <Region> (region.handle ())->rects;
    int	      n, nBox = const_cast <Region> (region.handle ())->numRects;
    GLfloat   *d, *data;

    if (!nBox)
	return;

    if (screen->desktopWindowCount ())
    {
	if (!backgroundTextures.empty ())
	{
	    backgroundTextures.clear ();
	}

	backgroundLoaded = false;

	return;
    }
    else
    {
	if (!backgroundLoaded)
	    updateScreenBackground ();

	backgroundLoaded = true;
    }

    data = new GLfloat [nBox * 16];
    if (!data)
	return;

    d = data;
    n = nBox;

    if (backgroundTextures.empty ())
    {
	while (n--)
	{
	    *d++ = pBox->x1;
	    *d++ = pBox->y2;

	    *d++ = pBox->x2;
	    *d++ = pBox->y2;

	    *d++ = pBox->x2;
	    *d++ = pBox->y1;

	    *d++ = pBox->x1;
	    *d++ = pBox->y1;

	    pBox++;
	}

	glVertexPointer (2, GL_FLOAT, sizeof (GLfloat) * 2, data + 2);

	glColor4us (0, 0, 0, 0);
	glDrawArrays (GL_QUADS, 0, nBox * 4);
	glColor4usv (defaultColor);
    }
    else
    {
	for (unsigned int i = 0; i < backgroundTextures.size (); i++)
	{
	    GLTexture *bg = backgroundTextures[i];
	    CompRegion r = region & *bg;

	    pBox = const_cast <Region> (r.handle ())->rects;
	    nBox = const_cast <Region> (r.handle ())->numRects;
	    d = data;
	    n = nBox;

	    while (n--)
	    {
		*d++ = COMP_TEX_COORD_X (bg->matrix (), pBox->x1);
		*d++ = COMP_TEX_COORD_Y (bg->matrix (), pBox->y2);

		*d++ = pBox->x1;
		*d++ = pBox->y2;

		*d++ = COMP_TEX_COORD_X (bg->matrix (), pBox->x2);
		*d++ = COMP_TEX_COORD_Y (bg->matrix (), pBox->y2);

		*d++ = pBox->x2;
		*d++ = pBox->y2;

		*d++ = COMP_TEX_COORD_X (bg->matrix (), pBox->x2);
		*d++ = COMP_TEX_COORD_Y (bg->matrix (), pBox->y1);

		*d++ = pBox->x2;
		*d++ = pBox->y1;

		*d++ = COMP_TEX_COORD_X (bg->matrix (), pBox->x1);
		*d++ = COMP_TEX_COORD_Y (bg->matrix (), pBox->y1);

		*d++ = pBox->x1;
		*d++ = pBox->y1;

		pBox++;
	    }

	    glTexCoordPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data);
	    glVertexPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data + 2);

	    if (bg->name ())
	    {
		if (transformed)
		    bg->enable (GLTexture::Good);
		else
		    bg->enable (GLTexture::Fast);

		glDrawArrays (GL_QUADS, 0, nBox * 4);

		bg->disable ();
	    }
	}
    }

    delete [] data;
}


/* This function currently always performs occlusion detection to
   minimize paint regions. OpenGL precision requirements are no good
   enough to guarantee that the results from using occlusion detection
   is the same as without. It's likely not possible to see any
   difference with most hardware but occlusion detection in the
   transformed screen case should be made optional for those who do
   see a difference. */
void
PrivateGLScreen::paintOutputRegion (const GLMatrix   &transform,
				    const CompRegion &region,
				    CompOutput       *output,
				    unsigned int     mask)
{
    CompRegion    tmpRegion (region);
    CompWindow    *w;
    GLWindow      *gw;
    int		  count, windowMask, odMask;
    CompWindow	  *fullscreenWindow = NULL;
    bool          status, unredirectFS;
    bool          withOffset = false;
    GLMatrix      vTransform;
    CompPoint     offXY;

    CompWindowList                   pl;
    CompWindowList::reverse_iterator rit;

    unredirectFS = CompositeScreen::get (screen)->
	getOption ("unredirect_fullscreen_windows")->value ().b ();

    if (mask & PAINT_SCREEN_TRANSFORMED_MASK)
    {
	windowMask     = PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;
	count	       = 1;
    }
    else
    {
	windowMask     = 0;
	count	       = 0;
    }

    pl = cScreen->getWindowPaintList ();

    if (!(mask & PAINT_SCREEN_NO_OCCLUSION_DETECTION_MASK))
    {
	/* detect occlusions */
	for (rit = pl.rbegin (); rit != pl.rend (); rit++)
	{
	    w = (*rit);
	    gw = GLWindow::get (w);

	    if (w->destroyed ())
		continue;

	    if (!w->shaded ())
	    {
		if (!w->isViewable () ||
		    !CompositeWindow::get (w)->damaged ())
		    continue;
	    }

	    /* copy region */
	    gw->priv->clip = tmpRegion;

	    odMask = PAINT_WINDOW_OCCLUSION_DETECTION_MASK;

	    if ((cScreen->windowPaintOffset ().x () != 0 ||
		 cScreen->windowPaintOffset ().y () != 0) &&
		!w->onAllViewports ())
	    {
		withOffset = true;

		offXY = w->getMovementForOffset (cScreen->windowPaintOffset ());

		vTransform = transform;
		vTransform.translate (offXY.x (), offXY.y (), 0);

		gw->priv->clip.translate (-offXY.x (), -offXY. y ());

		odMask |= PAINT_WINDOW_WITH_OFFSET_MASK;
		status = gw->glPaint (gw->paintAttrib (), vTransform,
				      tmpRegion, odMask);
	    }
	    else
	    {
		withOffset = false;
		status = gw->glPaint (gw->paintAttrib (), transform, tmpRegion,
				      odMask);
	    }

	    if (status)
	    {
		if (withOffset)
		{
		    tmpRegion -= w->region ().translated (offXY);
		}
		else
		    tmpRegion -= w->region ();

		/* unredirect top most fullscreen windows. */
		if (count == 0 && unredirectFS)
		{
		    if (w->region () == screen->region () &&
			tmpRegion.isEmpty ())
		    {
			fullscreenWindow = w;
		    }
		    else
		    {
			foreach (CompOutput &o, screen->outputDevs ())
			    if (w->region () == CompRegion (o))
				fullscreenWindow = w;
		    }
		}
	    }

	    count++;
	}
    }

    if (fullscreenWindow)
	CompositeWindow::get (fullscreenWindow)->unredirect ();

    if (!(mask & PAINT_SCREEN_NO_BACKGROUND_MASK))
	paintBackground (tmpRegion, (mask & PAINT_SCREEN_TRANSFORMED_MASK));

    /* paint all windows from bottom to top */
    foreach (w, pl)
    {
	if (w->destroyed ())
	    continue;

	if (w == fullscreenWindow)
	    continue;

	if (!w->shaded ())
	{
	    if (!w->isViewable () ||
		!CompositeWindow::get (w)->damaged ())
		continue;
	}

	gw = GLWindow::get (w);

	const CompRegion &clip =
	    (!(mask & PAINT_SCREEN_NO_OCCLUSION_DETECTION_MASK)) ?
	    gw->clip () : region;

	if ((cScreen->windowPaintOffset ().x () != 0 ||
	     cScreen->windowPaintOffset ().y () != 0) &&
	    !w->onAllViewports ())
	{
	    offXY = w->getMovementForOffset (cScreen->windowPaintOffset ());

	    vTransform = transform;
	    vTransform.translate (offXY.x (), offXY.y (), 0);
	    gw->glPaint (gw->paintAttrib (), vTransform, clip,
		         windowMask | PAINT_WINDOW_WITH_OFFSET_MASK);
	}
	else
	{
	    gw->glPaint (gw->paintAttrib (), transform, clip, windowMask);
	}
    }
}

void
GLScreen::glEnableOutputClipping (const GLMatrix   &transform,
				  const CompRegion &region,
				  CompOutput       *output)
{
    WRAPABLE_HND_FUNC (3, glEnableOutputClipping, transform, region, output)

    GLdouble h = screen->height ();

    GLdouble p1[2] = { region.handle ()->extents.x1,
		       h - region.handle ()->extents.y2 };
    GLdouble p2[2] = { region.handle ()->extents.x2,
		       h - region.handle ()->extents.y1 };

    GLdouble halfW = output->width () / 2.0;
    GLdouble halfH = output->height () / 2.0;

    GLdouble cx = output->x1 () + halfW;
    GLdouble cy = (h - output->y2 ()) + halfH;

    GLdouble top[4]    = { 0.0, halfH / (cy - p1[1]), 0.0, 0.5 };
    GLdouble bottom[4] = { 0.0, halfH / (cy - p2[1]), 0.0, 0.5 };
    GLdouble left[4]   = { halfW / (cx - p1[0]), 0.0, 0.0, 0.5 };
    GLdouble right[4]  = { halfW / (cx - p2[0]), 0.0, 0.0, 0.5 };

    glPushMatrix ();
    glLoadMatrixf (transform.getMatrix ());

    glClipPlane (GL_CLIP_PLANE0, top);
    glClipPlane (GL_CLIP_PLANE1, bottom);
    glClipPlane (GL_CLIP_PLANE2, left);
    glClipPlane (GL_CLIP_PLANE3, right);

    glEnable (GL_CLIP_PLANE0);
    glEnable (GL_CLIP_PLANE1);
    glEnable (GL_CLIP_PLANE2);
    glEnable (GL_CLIP_PLANE3);

    glPopMatrix ();
}

void
GLScreen::glDisableOutputClipping ()
{
    WRAPABLE_HND_FUNC (4, glDisableOutputClipping)

    glDisable (GL_CLIP_PLANE0);
    glDisable (GL_CLIP_PLANE1);
    glDisable (GL_CLIP_PLANE2);
    glDisable (GL_CLIP_PLANE3);
}

#define CLIP_PLANE_MASK (PAINT_SCREEN_TRANSFORMED_MASK | \
			 PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK)

void
GLScreen::glPaintTransformedOutput (const GLScreenPaintAttrib &sAttrib,
				    const GLMatrix            &transform,
				    const CompRegion          &region,
				    CompOutput                *output,
				    unsigned int              mask)
{
    WRAPABLE_HND_FUNC (1, glPaintTransformedOutput, sAttrib, transform,
		       region, output, mask)

    GLMatrix sTransform = transform;

    if (mask & PAINT_SCREEN_CLEAR_MASK)
	clearTargetOutput (GL_COLOR_BUFFER_BIT);

    setLighting (true);

    glApplyTransform (sAttrib, output, &sTransform);

    if ((mask & CLIP_PLANE_MASK) == CLIP_PLANE_MASK)
    {
	glEnableOutputClipping (sTransform, region, output);

	sTransform.toScreenSpace (output, -sAttrib.zTranslate);

	glPushMatrix ();
	glLoadMatrixf (sTransform.getMatrix ());

	priv->paintOutputRegion (sTransform, region, output, mask);

	glPopMatrix ();

	glDisableOutputClipping ();
    }
    else
    {
	sTransform.toScreenSpace (output, -sAttrib.zTranslate);

	glPushMatrix ();
	glLoadMatrixf (sTransform.getMatrix ());

	priv->paintOutputRegion (sTransform, region, output, mask);

	glPopMatrix ();
    }
}

bool
GLScreen::glPaintOutput (const GLScreenPaintAttrib &sAttrib,
			 const GLMatrix            &transform,
			 const CompRegion          &region,
			 CompOutput                *output,
			 unsigned int              mask)
{
    WRAPABLE_HND_FUNC_RETURN (0, bool, glPaintOutput, sAttrib, transform,
			      region, output, mask)

    GLMatrix sTransform = transform;

    if (mask & PAINT_SCREEN_REGION_MASK)
    {
	if (mask & PAINT_SCREEN_TRANSFORMED_MASK)
	{
	    if (mask & PAINT_SCREEN_FULL_MASK)
	    {
		glPaintTransformedOutput (sAttrib, sTransform,
					  CompRegion (*output), output, mask);

		return true;
	    }

	    return false;
	}

	setLighting (false);

	sTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	glPushMatrix ();
	glLoadMatrixf (sTransform.getMatrix ());

	priv->paintOutputRegion (sTransform, region, output, mask);

	glPopMatrix ();

	return true;
    }
    else if (mask & PAINT_SCREEN_FULL_MASK)
    {
	glPaintTransformedOutput (sAttrib, sTransform, CompRegion (*output),
				  output, mask);

	return true;
    }
    else
    {
	return false;
    }
}

#define ADD_RECT(data, m, n, x1, y1, x2, y2)	   \
    for (it = 0; it < n; it++)			   \
    {						   \
        const GLTexture::Matrix &mat = m[it];	   \
	*(data)++ = COMP_TEX_COORD_X (mat, x1);    \
	*(data)++ = COMP_TEX_COORD_Y (mat, y1);    \
    }						   \
    *(data)++ = (x1);				   \
    *(data)++ = (y1);				   \
    *(data)++ = 0.0;				   \
    for (it = 0; it < n; it++)			   \
    {						   \
        const GLTexture::Matrix &mat = m[it];	   \
	*(data)++ = COMP_TEX_COORD_X (mat, x1);    \
	*(data)++ = COMP_TEX_COORD_Y (mat, y2);    \
    }						   \
    *(data)++ = (x1);				   \
    *(data)++ = (y2);				   \
    *(data)++ = 0.0;				   \
    for (it = 0; it < n; it++)			   \
    {						   \
        const GLTexture::Matrix &mat = m[it];	   \
	*(data)++ = COMP_TEX_COORD_X (mat, x2);    \
	*(data)++ = COMP_TEX_COORD_Y (mat, y2);    \
    }						   \
    *(data)++ = (x2);				   \
    *(data)++ = (y2);				   \
    *(data)++ = 0.0;				   \
    for (it = 0; it < n; it++)			   \
    {						   \
        const GLTexture::Matrix &mat = m[it];	   \
	*(data)++ = COMP_TEX_COORD_X (mat, x2);    \
	*(data)++ = COMP_TEX_COORD_Y (mat, y1);    \
    }						   \
    *(data)++ = (x2);				   \
    *(data)++ = (y1);				   \
    *(data)++ = 0.0

#define ADD_QUAD(data, m, n, x1, y1, x2, y2)		\
    for (it = 0; it < n; it++)				\
    {							\
        const GLTexture::Matrix &mat = m[it];		\
	*(data)++ = COMP_TEX_COORD_XY (mat, x1, y1);	\
	*(data)++ = COMP_TEX_COORD_YX (mat, x1, y1);	\
    }							\
    *(data)++ = (x1);					\
    *(data)++ = (y1);					\
    *(data)++ = 0.0;					\
    for (it = 0; it < n; it++)				\
    {							\
        const GLTexture::Matrix &mat = m[it];		\
	*(data)++ = COMP_TEX_COORD_XY (mat, x1, y2);	\
	*(data)++ = COMP_TEX_COORD_YX (mat, x1, y2);	\
    }							\
    *(data)++ = (x1);					\
    *(data)++ = (y2);					\
    *(data)++ = 0.0;					\
    for (it = 0; it < n; it++)				\
    {							\
        const GLTexture::Matrix &mat = m[it];	        \
	*(data)++ = COMP_TEX_COORD_XY (mat, x2, y2);	\
	*(data)++ = COMP_TEX_COORD_YX (mat, x2, y2);	\
    }							\
    *(data)++ = (x2);					\
    *(data)++ = (y2);					\
    *(data)++ = 0.0;					\
    for (it = 0; it < n; it++)				\
    {							\
        const GLTexture::Matrix &mat = m[it];	        \
	*(data)++ = COMP_TEX_COORD_XY (mat, x2, y1);	\
	*(data)++ = COMP_TEX_COORD_YX (mat, x2, y1);	\
    }							\
    *(data)++ = (x2);					\
    *(data)++ = (y1);					\
    *(data)++ = 0.0;

void
GLWindow::glDrawGeometry ()
{
    WRAPABLE_HND_FUNC (4, glDrawGeometry)

    int     texUnit = priv->geometry.texUnits;
    int     currentTexUnit = 0;
    int     stride = priv->geometry.vertexStride;
    GLfloat *vertices = priv->geometry.vertices + (stride - 3);

    stride *= sizeof (GLfloat);

    glVertexPointer (3, GL_FLOAT, stride, vertices);

    while (texUnit--)
    {
	if (texUnit != currentTexUnit)
	{
	    (*GL::clientActiveTexture) (GL_TEXTURE0_ARB + texUnit);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	    currentTexUnit = texUnit;
	}
	vertices -= priv->geometry.texCoordSize;
	glTexCoordPointer (priv->geometry.texCoordSize,
			   GL_FLOAT, stride, vertices);
    }

    glDrawArrays (GL_QUADS, 0, priv->geometry.vCount);

    /* disable all texture coordinate arrays except 0 */
    texUnit = priv->geometry.texUnits;
    if (texUnit > 1)
    {
	while (--texUnit)
	{
	    (*GL::clientActiveTexture) (GL_TEXTURE0_ARB + texUnit);
	    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	}

	(*GL::clientActiveTexture) (GL_TEXTURE0_ARB);
    }
}

static inline void
addSingleQuad (GLfloat      *&d,
	       const        GLTexture::MatrixList &matrix,
	       unsigned int nMatrix,
	       int          x1,
	       int          y1,
	       int          x2,
	       int          y2,
	       int          &n,
	       bool         rect)
{
    unsigned int it;

    if (rect)
    {
	ADD_RECT (d, matrix, nMatrix, x1, y1, x2, y2);
    }
    else
    {
	ADD_QUAD (d, matrix, nMatrix, x1, y1, x2, y2);
    }
    n++;
}

static inline void
addQuads (GLfloat      *&d,
	  const        GLTexture::MatrixList &matrix,
	  unsigned int nMatrix,
	  int          x1,
	  int          y1,
	  int          x2,
	  int          y2,
	  int          &n,
	  int          vSize,
	  bool         rect,
	  GLWindow::Geometry &geometry,
	  unsigned int maxGridWidth,
	  unsigned int maxGridHeight)
{
    int nQuadsX = (maxGridWidth == MAXSHORT) ? 1 :
	1 + (x2 - x1 - 1) / (int) maxGridWidth;  // ceil. division
    int nQuadsY = (maxGridHeight == MAXSHORT) ? 1 :
	1 + (y2 - y1 - 1) / (int) maxGridHeight;
    int newVertexSize = (n + nQuadsX * nQuadsY) * vSize * 4;

    // Make sure enough vertices are allocated for nQuadsX * nQuadsY more quads
    if (newVertexSize > geometry.vertexSize)
    {
	if (!geometry.moreVertices (newVertexSize))
	    return;

	d = geometry.vertices + (n * vSize * 4);
    }

    if (nQuadsX == 1 && nQuadsY == 1)
    {
	addSingleQuad (d, matrix, nMatrix, x1, y1, x2, y2, n, rect);
    }
    else
    {
	int quadWidth  = 1 + (x2 - x1 - 1) / nQuadsX;  // ceil. division
	int quadHeight = 1 + (y2 - y1 - 1) / nQuadsY;
	int nx1, ny1, nx2, ny2;

	for (ny1 = y1; ny1 < y2; ny1 = ny2)
	{
	    ny2 = MIN (ny1 + (int) quadHeight, y2);

	    for (nx1 = x1; nx1 < x2; nx1 = nx2)
	    {
		nx2 = MIN (nx1 + (int) quadWidth, x2);

		addSingleQuad (d, matrix, nMatrix, nx1, ny1, nx2, ny2, n, rect);
	    }
	}
    }
}

void
GLWindow::glAddGeometry (const GLTexture::MatrixList &matrix,
			 const CompRegion            &region,
			 const CompRegion            &clip,
			 unsigned int                maxGridWidth,
			 unsigned int                maxGridHeight)
{
    WRAPABLE_HND_FUNC (2, glAddGeometry, matrix, region, clip)

    BoxRec full;
    int    nMatrix = matrix.size ();

    priv->geometry.texUnits = nMatrix;

    full = clip.handle ()->extents;
    if (region.handle ()->extents.x1 > full.x1)
	full.x1 = region.handle ()->extents.x1;
    if (region.handle ()->extents.y1 > full.y1)
	full.y1 = region.handle ()->extents.y1;
    if (region.handle ()->extents.x2 < full.x2)
	full.x2 = region.handle ()->extents.x2;
    if (region.handle ()->extents.y2 < full.y2)
	full.y2 = region.handle ()->extents.y2;

    if (full.x1 < full.x2 && full.y1 < full.y2)
    {
	BoxPtr  pBox;
	int     nBox;
	BoxPtr  pClip;
	int     nClip;
	BoxRec  cbox;
	int     vSize;
	int     n, it, x1, y1, x2, y2;
	GLfloat *d;
	bool    rect = true;

	for (it = 0; it < nMatrix; it++)
	{
	    if (matrix[it].xy != 0.0f || matrix[it].yx != 0.0f)
	    {
		rect = false;
		break;
	    }
	}

	pBox = const_cast <Region> (region.handle ())->rects;
	nBox = const_cast <Region> (region.handle ())->numRects;

	vSize = 3 + nMatrix * 2;

	n = priv->geometry.vCount / 4;

	if ((n + nBox) * vSize * 4 > priv->geometry.vertexSize)
	{
	    if (!priv->geometry.moreVertices ((n + nBox) * vSize * 4))
		return;
	}

	d = priv->geometry.vertices + (priv->geometry.vCount * vSize);

	while (nBox--)
	{
	    x1 = pBox->x1;
	    y1 = pBox->y1;
	    x2 = pBox->x2;
	    y2 = pBox->y2;

	    pBox++;

	    if (x1 < full.x1)
		x1 = full.x1;
	    if (y1 < full.y1)
		y1 = full.y1;
	    if (x2 > full.x2)
		x2 = full.x2;
	    if (y2 > full.y2)
		y2 = full.y2;

	    if (x1 < x2 && y1 < y2)
	    {
		nClip = const_cast <Region> (clip.handle ())->numRects;

		if (nClip == 1)
		{
		    addQuads (d, matrix, nMatrix,
			      x1, y1, x2, y2,
			      n, vSize, rect, priv->geometry,
			      maxGridWidth, maxGridHeight);
		}
		else
		{
		    pClip = const_cast <Region> (clip.handle ())->rects;

		    if (((n + nClip) * vSize * 4) > priv->geometry.vertexSize)
		    {
			if (!priv->geometry.moreVertices ((n + nClip) *
							  vSize * 4))
			    return;

			d = priv->geometry.vertices + (n * vSize * 4);
		    }

		    while (nClip--)
		    {
			cbox = *pClip;

			pClip++;

			if (cbox.x1 < x1)
			    cbox.x1 = x1;
			if (cbox.y1 < y1)
			    cbox.y1 = y1;
			if (cbox.x2 > x2)
			    cbox.x2 = x2;
			if (cbox.y2 > y2)
			    cbox.y2 = y2;

			if (cbox.x1 < cbox.x2 && cbox.y1 < cbox.y2)
			{
			    addQuads (d, matrix, nMatrix,
				      cbox.x1, cbox.y1, cbox.x2, cbox.y2,
				      n, vSize, rect, priv->geometry,
				      maxGridWidth, maxGridHeight);
			}
		    }
		}
	    }
	}

	priv->geometry.vCount       = n * 4;
	priv->geometry.vertexStride = vSize;
	priv->geometry.texCoordSize = 2;
    }
}

static bool
enableFragmentProgramAndDrawGeometry (GLScreen	         *gs,
				      GLWindow           *w,
				      GLTexture          *texture,
				      GLFragment::Attrib &attrib,
				      GLTexture::Filter  filter,
				      unsigned int       mask)
{
    GLFragment::Attrib fa (attrib);
    bool               blending;

    if (GL::canDoSaturated && attrib.getSaturation () != COLOR)
    {
	int param, function;

	param    = fa.allocParameters (1);
	function =
	    GLFragment::getSaturateFragmentFunction (texture, param);

	fa.addFunction (function);

	(*GL::programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param,
				      RED_SATURATION_WEIGHT,
				      GREEN_SATURATION_WEIGHT,
				      BLUE_SATURATION_WEIGHT,
				      attrib.getSaturation () / 65535.0f);
    }

    if (!fa.enable (&blending))
	return false;

    texture->enable (filter);

    if (mask & PAINT_WINDOW_BLEND_MASK)
    {
	if (blending)
	    glEnable (GL_BLEND);

	if (attrib.getOpacity () != OPAQUE || attrib.getBrightness () != BRIGHT)
	{
	    GLushort color;

	    color = (attrib.getOpacity () * attrib.getBrightness ()) >> 16;

	    gs->setTexEnvMode (GL_MODULATE);
	    glColor4us (color, color, color, attrib.getOpacity ());

	    w->glDrawGeometry ();

	    glColor4usv (defaultColor);
	    gs->setTexEnvMode (GL_REPLACE);
	}
	else
	{
	    w->glDrawGeometry ();
	}

	if (blending)
	    glDisable (GL_BLEND);
    }
    else if (attrib.getBrightness () != BRIGHT)
    {
	gs->setTexEnvMode (GL_MODULATE);
	glColor4us (attrib.getBrightness (), attrib.getBrightness (),
		    attrib.getBrightness (), BRIGHT);

	w->glDrawGeometry ();

	glColor4usv (defaultColor);
	gs->setTexEnvMode (GL_REPLACE);
    }
    else
    {
	w->glDrawGeometry ();
    }

    texture->disable ();

    fa.disable ();

    return true;
}

static void
enableFragmentOperationsAndDrawGeometry (GLScreen	    *gs,
					 GLWindow	    *w,
					 GLTexture	    *texture,
					 GLFragment::Attrib &attrib,
					 GLTexture::Filter  filter,
					 unsigned int	    mask)
{
    if (GL::canDoSaturated && attrib.getSaturation () != COLOR)
    {
	GLfloat constant[4];

	if (mask & PAINT_WINDOW_BLEND_MASK)
	    glEnable (GL_BLEND);

	texture->enable (filter);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_PRIMARY_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);

	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	glColor4f (1.0f, 1.0f, 1.0f, 0.5f);

	GL::activeTexture (GL_TEXTURE1_ARB);

	texture->enable (filter);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

	if (GL::canDoSlightlySaturated && attrib.getSaturation () > 0)
	{
	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	    constant[0] = 0.5f + 0.5f * RED_SATURATION_WEIGHT;
	    constant[1] = 0.5f + 0.5f * GREEN_SATURATION_WEIGHT;
	    constant[2] = 0.5f + 0.5f * BLUE_SATURATION_WEIGHT;
	    constant[3] = 1.0;

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

	    GL::activeTexture (GL_TEXTURE2_ARB);

	    texture->enable (filter);

	    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);

	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	    constant[3] = attrib.getSaturation () / 65535.0f;

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

	    if (attrib.getOpacity () < OPAQUE ||
		attrib.getBrightness () != BRIGHT)
	    {
		GL::activeTexture (GL_TEXTURE3_ARB);

		texture->enable (filter);

		constant[3] = attrib.getOpacity () / 65535.0f;
		constant[0] = constant[1] = constant[2] = constant[3] *
		    attrib.getBrightness () / 65535.0f;

		glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_CONSTANT);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

		w->glDrawGeometry ();

		texture->disable ();

		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		GL::activeTexture (GL_TEXTURE2_ARB);
	    }
	    else
	    {
		w->glDrawGeometry ();
	    }

	    texture->disable ();

	    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	    GL::activeTexture (GL_TEXTURE1_ARB);
	}
	else
	{
	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_CONSTANT);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

	    constant[3] = attrib.getOpacity () / 65535.0f;
	    constant[0] = constant[1] = constant[2] = constant[3] *
			  attrib.getBrightness ()/ 65535.0f;

	    constant[0] = 0.5f + 0.5f * RED_SATURATION_WEIGHT   * constant[0];
	    constant[1] = 0.5f + 0.5f * GREEN_SATURATION_WEIGHT * constant[1];
	    constant[2] = 0.5f + 0.5f * BLUE_SATURATION_WEIGHT  * constant[2];

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

	    w->glDrawGeometry ();
	}

	texture->disable ();

	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	GL::activeTexture (GL_TEXTURE0_ARB);

	texture->disable ();

	glColor4usv (defaultColor);
	gs->setTexEnvMode (GL_REPLACE);

	if (mask & PAINT_WINDOW_BLEND_MASK)
	    glDisable (GL_BLEND);
    }
    else
    {
	texture->enable (filter);

	if (mask & PAINT_WINDOW_BLEND_MASK)
	{
	    glEnable (GL_BLEND);
	    if (attrib.getOpacity ()!= OPAQUE ||
		attrib.getBrightness () != BRIGHT)
	    {
		GLushort color;

		color = (attrib.getOpacity () * attrib.getBrightness ()) >> 16;

		gs->setTexEnvMode (GL_MODULATE);
		glColor4us (color, color, color, attrib.getOpacity ());

		w->glDrawGeometry ();

		glColor4usv (defaultColor);
		gs->setTexEnvMode (GL_REPLACE);
	    }
	    else
	    {
		w->glDrawGeometry ();
	    }

	    glDisable (GL_BLEND);
	}
	else if (attrib.getBrightness () != BRIGHT)
	{
	    gs->setTexEnvMode (GL_MODULATE);
	    glColor4us (attrib.getBrightness (), attrib.getBrightness (),
			attrib.getBrightness (), BRIGHT);

	    w->glDrawGeometry ();

	    glColor4usv (defaultColor);
	    gs->setTexEnvMode (GL_REPLACE);
	}
	else
	{
	    w->glDrawGeometry ();
	}

	texture->disable ();
    }
}

void
GLWindow::glDrawTexture (GLTexture          *texture,
			 GLFragment::Attrib &attrib,
			 unsigned int       mask)
{
    WRAPABLE_HND_FUNC (3, glDrawTexture, texture, attrib, mask)

    GLTexture::Filter filter;

    if (mask & (PAINT_WINDOW_TRANSFORMED_MASK |
		PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK))
	filter = priv->gScreen->filter (SCREEN_TRANS_FILTER);
    else
	filter = priv->gScreen->filter (NOTHING_TRANS_FILTER);

    if ((!attrib.hasFunctions () && (!priv->gScreen->lighting () ||
	 attrib.getSaturation () == COLOR || attrib.getSaturation () == 0)) ||
	!enableFragmentProgramAndDrawGeometry (priv->gScreen, this, texture,
					       attrib, filter, mask))
    {
	enableFragmentOperationsAndDrawGeometry (priv->gScreen, this, texture,
						 attrib, filter, mask);
    }
}

bool
GLWindow::glDraw (const GLMatrix     &transform,
		  GLFragment::Attrib &fragment,
		  const CompRegion   &region,
		  unsigned int       mask)
{
    WRAPABLE_HND_FUNC_RETURN (1, bool, glDraw, transform,
			      fragment, region, mask)

    const CompRegion reg = (mask & PAINT_WINDOW_TRANSFORMED_MASK) ?
	                   infiniteRegion : region;

    if (reg.isEmpty ())
	return true;

    if (!priv->window->isViewable ())
	return true;

    if (priv->textures.empty () && !bind ())
	return false;

    if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	mask |= PAINT_WINDOW_BLEND_MASK;

    GLTexture::MatrixList ml (1);

    if (priv->textures.size () == 1)
    {
	ml[0] = priv->matrices[0];
	priv->geometry.reset ();
	glAddGeometry (ml, priv->window->region (), reg);
	if (priv->geometry.vCount)
	    glDrawTexture (priv->textures[0], fragment, mask);
    }
    else
    {
	if (priv->updateReg)
	    priv->updateWindowRegions ();
	for (unsigned int i = 0; i < priv->textures.size (); i++)
	{
	    ml[0] = priv->matrices[i];
	    priv->geometry.reset ();
	    glAddGeometry (ml, priv->regions[i], reg);
	    if (priv->geometry.vCount)
		glDrawTexture (priv->textures[i], fragment, mask);
	}
    }

    return true;
}

bool
GLWindow::glPaint (const GLWindowPaintAttrib &attrib,
		   const GLMatrix            &transform,
		   const CompRegion          &region,
		   unsigned int              mask)
{
    WRAPABLE_HND_FUNC_RETURN (0, bool, glPaint, attrib, transform, region, mask)

    GLFragment::Attrib fragment (attrib);
    bool               status;

    priv->lastPaint = attrib;

    if (priv->window->alpha () || attrib.opacity != OPAQUE)
	mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

    priv->lastMask = mask;

    if (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK)
    {
	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    return false;

	if (mask & PAINT_WINDOW_NO_CORE_INSTANCE_MASK)
	    return false;

	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	    return false;

	if (priv->window->shaded ())
	    return false;

	return true;
    }

    if (mask & PAINT_WINDOW_NO_CORE_INSTANCE_MASK)
	return true;

    if (mask & PAINT_WINDOW_TRANSFORMED_MASK ||
        mask & PAINT_WINDOW_WITH_OFFSET_MASK)
    {
	glPushMatrix ();
	glLoadMatrixf (transform.getMatrix ());
    }

    status = glDraw (transform, fragment, region, mask);

    if (mask & PAINT_WINDOW_TRANSFORMED_MASK ||
        mask & PAINT_WINDOW_WITH_OFFSET_MASK)
	glPopMatrix ();

    return status;
}
