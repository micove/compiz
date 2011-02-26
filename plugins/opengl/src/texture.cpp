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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <compiz.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <core/core.h>
#include <opengl/texture.h>
#include <privatetexture.h>
#include "privates.h"

std::map<Damage, TfpTexture*> boundPixmapTex;

static GLTexture::Matrix _identity_matrix = {
    1.0f, 0.0f,
    0.0f, 1.0f,
    0.0f, 0.0f
};

GLTexture::List::List () :
    std::vector<GLTexture *> (0)
{
}

GLTexture::List::List (unsigned int size) :
    std::vector<GLTexture *> (size)
{
    for (unsigned int i = 0; i < size; i++)
	at (i) = NULL;
}

GLTexture::List::List (const GLTexture::List &c) :
    std::vector<GLTexture *> (c.size ())
{
    for (unsigned int i = 0; i < c.size (); i++)
    {
	at (i) = c[i];
	GLTexture::incRef (c[i]);
    }
}

GLTexture::List::~List ()
{
    foreach (GLTexture *t, *this)
	if (t)
	    GLTexture::decRef (t);
}

GLTexture::List &
GLTexture::List::operator= (const GLTexture::List &c)
{
    this->clear ();
    resize (c.size ());
    for (unsigned int i = 0; i < c.size (); i++)
    {
	at (i) = c[i];
	GLTexture::incRef (c[i]);
    }
    return *this;
}

void
GLTexture::List::clear ()
{
    foreach (GLTexture *t, *this)
	if (t)
	    GLTexture::decRef (t);
    std::vector <GLTexture *>::clear ();
}

GLTexture::GLTexture () :
    CompRect (0, 0, 0, 0),
    priv (new PrivateTexture (this))
{
}

GLTexture::~GLTexture ()
{
    if (priv)
	delete priv;
}

PrivateTexture::PrivateTexture (GLTexture *texture) :
    texture (texture),
    name (0),
    target (GL_TEXTURE_2D),
    filter (GL_NEAREST),
    wrap   (GL_CLAMP_TO_EDGE),
    matrix (_identity_matrix),
    mipmap  (true),
    mipmapSupport (false),
    initial (true),
    refCount (1)
{
    glGenTextures (1, &name);
}

PrivateTexture::~PrivateTexture ()
{
    if (name)
    {
	glDeleteTextures (1, &name);
    }
}

GLuint
GLTexture::name () const
{
    return priv->name;
}

GLenum
GLTexture::target () const
{
    return priv->target;
}

const GLTexture::Matrix &
GLTexture::matrix () const
{
    return priv->matrix;
}

bool
GLTexture::mipmap () const
{
    return priv->mipmap & priv->mipmapSupport;
}

GLenum
GLTexture::filter () const
{
    return priv->filter;
}

void
GLTexture::enable (GLTexture::Filter filter)
{
    GLScreen *gs = GLScreen::get (screen);
    glEnable (priv->target);
    glBindTexture (priv->target, priv->name);

    if (filter == Fast)
    {
	if (priv->filter != GL_NEAREST)
	{
	    glTexParameteri (priv->target,
			     GL_TEXTURE_MIN_FILTER,
			     GL_NEAREST);
	    glTexParameteri (priv->target,
			     GL_TEXTURE_MAG_FILTER,
			     GL_NEAREST);

	    priv->filter = GL_NEAREST;
	}
    }
    else if (priv->filter != gs->textureFilter ())
    {
	if (gs->textureFilter () == GL_LINEAR_MIPMAP_LINEAR)
	{
	    if (GL::textureNonPowerOfTwo && GL::fbo && priv->mipmap)
	    {
		glTexParameteri (priv->target,
				 GL_TEXTURE_MIN_FILTER,
				 GL_LINEAR_MIPMAP_LINEAR);

		if (priv->filter != GL_LINEAR)
		    glTexParameteri (priv->target,
				     GL_TEXTURE_MAG_FILTER,
				     GL_LINEAR);

		priv->filter = GL_LINEAR_MIPMAP_LINEAR;
	    }
	    else if (priv->filter != GL_LINEAR)
	    {
		glTexParameteri (priv->target,
				 GL_TEXTURE_MIN_FILTER,
				 GL_LINEAR);
		glTexParameteri (priv->target,
				 GL_TEXTURE_MAG_FILTER,
				 GL_LINEAR);

		priv->filter = GL_LINEAR;
	    }
	}
	else
	{
	    glTexParameteri (priv->target,
			     GL_TEXTURE_MIN_FILTER,
			     gs->textureFilter ());
	    glTexParameteri (priv->target,
			     GL_TEXTURE_MAG_FILTER,
			     gs->textureFilter ());

	    priv->filter = gs->textureFilter ();
	}
    }

    if (priv->filter == GL_LINEAR_MIPMAP_LINEAR)
    {
	if (priv->initial)
	{
	    (*GL::generateMipmap) (priv->target);
	    priv->initial = false;
	}
    }
}

void
GLTexture::disable ()
{
    glBindTexture (priv->target, 0);
    glDisable (priv->target);
}

void
GLTexture::setData (GLenum target, Matrix &m, bool mipmap)
{
    priv->target = target;
    priv->matrix = m;
    priv->mipmapSupport = mipmap;
}

void
GLTexture::setMipmap (bool enable)
{
    priv->mipmap = enable;
}

void
GLTexture::setFilter (GLenum filter)
{
    glBindTexture (priv->target, priv->name);

    priv->filter = filter;

    glTexParameteri (priv->target, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri (priv->target, GL_TEXTURE_MAG_FILTER, filter);

    glBindTexture (priv->target, 0);
}

void
GLTexture::setWrap (GLenum wrap)
{
    glBindTexture (priv->target, priv->name);

    priv->wrap = GL_CLAMP_TO_EDGE;

    glTexParameteri (priv->target, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri (priv->target, GL_TEXTURE_WRAP_T, wrap);

    glBindTexture (priv->target, 0);
}

GLTexture::List
PrivateTexture::loadImageData (const char   *image,
			       unsigned int width,
			       unsigned int height,
			       GLenum       format,
			       GLenum       type)
{
#warning Add support for multiple textures
    if ((int) width > GL::maxTextureSize || (int) height > GL::maxTextureSize)
	return GLTexture::List ();

    GLTexture::List rv (1);
    GLTexture *t = new GLTexture ();
    rv[0] = t;

    GLTexture::Matrix matrix = _identity_matrix;
    CompOption        *opt;
    GLint             internalFormat;
    GLenum            target;
    bool              mipmap;


    if (GL::textureNonPowerOfTwo ||
	(POWER_OF_TWO (width) && POWER_OF_TWO (height)))
    {
	target = GL_TEXTURE_2D;
	matrix.xx = 1.0f / width;
	matrix.yy = 1.0f / height;
	matrix.y0 = 0.0f;
	mipmap = true;
    }
    else
    {
	target = GL_TEXTURE_RECTANGLE_NV;
	matrix.xx = 1.0f;
	matrix.yy = 1.0f;
	matrix.y0 = 0.0f;
	mipmap = false;
    }

    t->setData (target, matrix, mipmap);
    t->setGeometry (0, 0, width, height);

    glBindTexture (target, t->name ());

    opt = GLScreen::get (screen)->getOption ("texture_compression");
    if (opt->value ().b () && GL::textureCompression)
	internalFormat = GL_COMPRESSED_RGBA_ARB;
    else
	internalFormat =  GL_RGBA;

    glTexImage2D (target, 0, internalFormat, width, height, 0,
		  format, type, image);

    t->setFilter (GL_NEAREST);
    t->setWrap (GL_CLAMP_TO_EDGE);

    return rv;
}

GLTexture::List
GLTexture::imageBufferToTexture (const char *image,
				 CompSize   size)
{
#if IMAGE_BYTE_ORDER == MSBFirst
    return PrivateTexture::loadImageData (image, size.width (), size.height (),
					  GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
#else
    return PrivateTexture::loadImageData (image, size.width (), size.height (),
					  GL_BGRA, GL_UNSIGNED_BYTE);
#endif
}

GLTexture::List
GLTexture::imageDataToTexture (const char *image,
			       CompSize   size,
			       GLenum     format,
			       GLenum     type)
{
    return PrivateTexture::loadImageData (image, size.width (), size.height (),
					  format, type);
}


GLTexture::List
GLTexture::readImageToTexture (CompString &imageFileName,
			       CompString &pluginName,
			       CompSize   &size)
{
    void *image = NULL;

    if (!screen->readImageFromFile (imageFileName, pluginName, size, image) || !image)
	return GLTexture::List ();

    GLTexture::List rv =
	GLTexture::imageBufferToTexture ((char *)image, size);

    free (image);

    return rv;
}

void
GLTexture::decRef (GLTexture *tex)
{
    tex->priv->refCount--;
    if (tex->priv->refCount <= 0)
	delete tex;
}

void
GLTexture::incRef (GLTexture *tex)
{
    tex->priv->refCount++;
}

GLTexture::List
GLTexture::bindPixmapToTexture (Pixmap pixmap,
				int    width,
				int    height,
				int    depth)
{
    GLTexture::List rv;

    foreach (BindPixmapProc &proc, GLScreen::get (screen)->priv->bindPixmap)
    {
	if (!proc.empty ())
	    rv = proc (pixmap, width, height, depth);
	if (rv.size ())
	    return rv;
    }
    return GLTexture::List ();
}

TfpTexture::TfpTexture () :
    pixmap (0),
    damaged (true),
    damage (None),
    updateMipMap (true)
{
}

TfpTexture::~TfpTexture ()
{
    if (pixmap)
    {
	glEnable (target ());

	glBindTexture (target (), name ());

	(*GL::releaseTexImage) (screen->dpy (), pixmap, GLX_FRONT_LEFT_EXT);

	glBindTexture (target (), 0);
	glDisable (target ());

	GL::destroyPixmap (screen->dpy (), pixmap);

	boundPixmapTex.erase (damage);
	XDamageDestroy (screen->dpy (), damage);
    }
}

GLTexture::List
TfpTexture::bindPixmapToTexture (Pixmap pixmap,
				 int    width,
				 int    height,
				 int    depth)
{
    if ((int) width > GL::maxTextureSize || (int) height > GL::maxTextureSize ||
        !GL::textureFromPixmap)
	return GLTexture::List ();

    GLTexture::List   rv (1);
    TfpTexture        *tex = NULL;
    unsigned int      target = 0;
    GLenum            texTarget = GL_TEXTURE_2D;
    GLXPixmap         glxPixmap = None;
    GLTexture::Matrix matrix = _identity_matrix;
    bool              mipmap = false;
    GLFBConfig        *config =
	GLScreen::get (screen)->glxPixmapFBConfig (depth);
    int               attribs[7], i = 0;

    if (!config->fbConfig)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"No GLXFBConfig for depth %d",
			depth);

	return GLTexture::List ();
    }

    attribs[i++] = GLX_TEXTURE_FORMAT_EXT;
    attribs[i++] = config->textureFormat;
    attribs[i++] = GLX_MIPMAP_TEXTURE_EXT;
    attribs[i++] = config->mipmap;

    /* If no texture target is specified in the fbconfig, or only the
       TEXTURE_2D target is specified and GL_texture_non_power_of_two
       is not supported, then allow the server to choose the texture target. */
    if (config->textureTargets & GLX_TEXTURE_2D_BIT_EXT &&
       (GL::textureNonPowerOfTwo ||
       (POWER_OF_TWO (width) && POWER_OF_TWO (height))))
	target = GLX_TEXTURE_2D_EXT;
    else if (config->textureTargets & GLX_TEXTURE_RECTANGLE_BIT_EXT)
	target = GLX_TEXTURE_RECTANGLE_EXT;

    /* Workaround for broken texture from pixmap implementations,
       that don't advertise any texture target in the fbconfig. */
    if (!target)
    {
	if (!(config->textureTargets & GLX_TEXTURE_2D_BIT_EXT))
	    target = GLX_TEXTURE_RECTANGLE_EXT;
	else if (!(config->textureTargets & GLX_TEXTURE_RECTANGLE_BIT_EXT))
	    target = GLX_TEXTURE_2D_EXT;
    }

    if (target)
    {
	attribs[i++] = GLX_TEXTURE_TARGET_EXT;
	attribs[i++] = target;
    }

    attribs[i++] = None;

    glxPixmap = (*GL::createPixmap) (screen->dpy (), config->fbConfig,
				     pixmap, attribs);

    if (!glxPixmap)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"glXCreatePixmap failed");

	return GLTexture::List ();
    }

    if (!target)
	(*GL::queryDrawable) (screen->dpy (), glxPixmap,
			      GLX_TEXTURE_TARGET_EXT, &target);

    switch (target) {
	case GLX_TEXTURE_2D_EXT:
	    texTarget = GL_TEXTURE_2D;

	    matrix.xx = 1.0f / width;
	    if (config->yInverted)
	    {
		matrix.yy = 1.0f / height;
		matrix.y0 = 0.0f;
	    }
	    else
	    {
		matrix.yy = -1.0f / height;
		matrix.y0 = 1.0f;
	    }
	    mipmap = config->mipmap;
	    break;
	case GLX_TEXTURE_RECTANGLE_EXT:
	    texTarget = GL_TEXTURE_RECTANGLE_ARB;

	    matrix.xx = 1.0f;
	    if (config->yInverted)
	    {
		matrix.yy = 1.0f;
		matrix.y0 = 0;
	    }
	    else
	    {
		matrix.yy = -1.0f;
		matrix.y0 = height;
	    }
	    mipmap = false;
	    break;
	default:
	    compLogMessage ("core", CompLogLevelWarn,
			    "pixmap 0x%x can't be bound to texture",
			    (int) pixmap);

	    GL::destroyPixmap (screen->dpy (), glxPixmap);
	    glxPixmap = None;

	    return GLTexture::List ();
    }

    tex = new TfpTexture ();
    tex->setData (texTarget, matrix, mipmap);
    tex->setGeometry (0, 0, width, height);
    tex->pixmap = glxPixmap;

    rv[0] = tex;

    glBindTexture (texTarget, tex->name ());


    (*GL::bindTexImage) (screen->dpy (), glxPixmap, GLX_FRONT_LEFT_EXT, NULL);

    tex->setFilter (GL_NEAREST);
    tex->setWrap (GL_CLAMP_TO_EDGE);

    glBindTexture (texTarget, 0);

    tex->damage = XDamageCreate (screen->dpy (), pixmap,
			         XDamageReportRawRectangles);
    boundPixmapTex[tex->damage] = tex;

    return rv;
}

void
TfpTexture::enable (GLTexture::Filter filter)
{
    glEnable (target ());
    glBindTexture (target (), name ());

    if (damaged && pixmap)
    {
	(*GL::releaseTexImage) (screen->dpy (), pixmap, GLX_FRONT_LEFT_EXT);
	(*GL::bindTexImage) (screen->dpy (), pixmap, GLX_FRONT_LEFT_EXT, NULL);
    }

    GLTexture::enable (filter);
    
    if (damaged)
	updateMipMap = true;

    if (this->filter () == GL_LINEAR_MIPMAP_LINEAR && updateMipMap)
    {
	(*GL::generateMipmap) (target ());
	updateMipMap = false;
    }
    damaged = false;
}
