/*
 * Copyright © 2006 Novell, Inc.
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

#include "water.h"

#include <math.h>


COMPIZ_PLUGIN_20090315 (water, WaterPluginVTable)

static int waterLastPointerX = 0;
static int waterLastPointerY = 0;

static const char *waterFpString =
    "!!ARBfp1.0"

    "PARAM param = program.local[0];"
    "ATTRIB t11  = fragment.texcoord[0];"

    "TEMP t01, t21, t10, t12;"
    "TEMP c11, c01, c21, c10, c12;"
    "TEMP prev, v, temp, accel;"

    "TEX prev, t11, texture[0], %s;"
    "TEX c11,  t11, texture[1], %s;"

    /* sample offsets */
    "ADD t01, t11, { - %f, 0.0, 0.0, 0.0 };"
    "ADD t21, t11, {   %f, 0.0, 0.0, 0.0 };"
    "ADD t10, t11, { 0.0, - %f, 0.0, 0.0 };"
    "ADD t12, t11, { 0.0,   %f, 0.0, 0.0 };"

    /* fetch nesseccary samples */
    "TEX c01, t01, texture[1], %s;"
    "TEX c21, t21, texture[1], %s;"
    "TEX c10, t10, texture[1], %s;"
    "TEX c12, t12, texture[1], %s;"

    /* x/y normals from height */
    "MOV v, { 0.0, 0.0, 0.75, 0.0 };"
    "SUB v.x, c12.w, c10.w;"
    "SUB v.y, c01.w, c21.w;"

    /* bumpiness */
    "MUL v, v, 1.5;"

    /* normalize */
    "MAD temp, v.x, v.x, 1.0;"
    "MAD temp, v.y, v.y, temp;"
    "RSQ temp, temp.x;"
    "MUL v, v, temp;"

    /* add scale and bias to normal */
    "MAD v, v, 0.5, 0.5;"

    /* done with computing the normal, continue with computing the next
       height value */
    "ADD accel, c10, c12;"
    "ADD accel, c01, accel;"
    "ADD accel, c21, accel;"
    "MAD accel, -4.0, c11, accel;"

    /* store new height in alpha component */
    "MAD v.w, 2.0, c11, -prev.w;"
    "MAD v.w, accel, param.x, v.w;"

    /* fade out height */
    "MUL v.w, v.w, param.y;"

    "MOV result.color, v;"

    "END";

static bool
loadFragmentProgram (GLuint	*program,
		     const char *string)
{
    GLint errorPos;

    /* clear errors */
    glGetError ();

    if (!*program)
	GL::genPrograms (1, program);

    GL::bindProgram (GL_FRAGMENT_PROGRAM_ARB, *program);
    GL::programString (GL_FRAGMENT_PROGRAM_ARB,
		       GL_PROGRAM_FORMAT_ASCII_ARB,
		       strlen (string), string);

    glGetIntegerv (GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    if (glGetError () != GL_NO_ERROR || errorPos != -1)
    {
	compLogMessage ("water", CompLogLevelError,
			"failed to load bump map program");

	GL::deletePrograms (1, program);
	*program = 0;

	return false;
    }

    return true;
}

static int
loadWaterProgram ()
{
    char buffer[1024];

    WATER_SCREEN (screen);

    if (ws->target == GL_TEXTURE_2D)
	sprintf (buffer, waterFpString,
		 "2D", "2D",
		 1.0f / ws->width,  1.0f / ws->width,
		 1.0f / ws->height, 1.0f / ws->height,
		 "2D", "2D", "2D", "2D");
    else
	sprintf (buffer, waterFpString,
		 "RECT", "RECT",
		 1.0f, 1.0f, 1.0f, 1.0f,
		 "RECT", "RECT", "RECT", "RECT");

    return loadFragmentProgram (&ws->program, buffer);
}

GLFragment::FunctionId
WaterScreen::getBumpMapFragmentFunction (GLTexture *texture,
					 int       unit,
					 int       param)
{
    GLFragment::FunctionData data;
    int                      target;
    WaterFunction            function;

    if (texture->target () == GL_TEXTURE_2D)
	target = COMP_FETCH_TARGET_2D;
    else
	target = COMP_FETCH_TARGET_RECT;

    foreach (WaterFunction &f, bumpMapFunctions)
    {
	if (f.param  == param &&
	    f.unit   == unit  &&
	    f.target == target)
	    return f.id;
    }

    static const char *temp[] = { "normal", "temp", "total", "bump", "offset" };

    for (unsigned int i = 0; i < sizeof (temp) / sizeof (temp[0]); i++)
	data.addTempHeaderOp (temp[i]);

    data.addDataOp (
	/* get normal from normal map */
	"TEX normal, fragment.texcoord[%d], texture[%d], %s;"

	/* save height */
	"MOV offset, normal;"

	/* remove scale and bias from normal */
	"MAD normal, normal, 2.0, -1.0;"

	/* normalize the normal map */
	"DP3 temp, normal, normal;"
	"RSQ temp, temp.x;"
	"MUL normal, normal, temp;"

	/* scale down normal by height and constant and use as
	    offset in texture */
	"MUL offset, normal, offset.w;"
	"MUL offset, offset, program.env[%d];",

	unit, unit,
	(this->target == GL_TEXTURE_2D) ? "2D" : "RECT",
	param);

    data.addFetchOp ("output", "offset.yxzz", target);

    data.addDataOp (
	/* normal dot lightdir, this should eventually be
	    changed to a real light vector */
	"DP3 bump, normal, { 0.707, 0.707, 0.0, 0.0 };"
	"MUL bump, bump, state.light[0].diffuse;");


    data.addColorOp ("output", "output");

    data.addDataOp (
	/* diffuse per-vertex lighting, opacity and brightness
	    and add lightsource bump color */
	"ADD output, output, bump;");

    if (!data.status ())
	return 0;


    function.id = data.createFragmentFunction ("water");

    function.target = target;
    function.param  = param;
    function.unit   = unit;

    bumpMapFunctions.push_back (function);

    return function.id;
}

void
WaterScreen::allocTexture (int index)
{
    glGenTextures (1, &texture[index]);
    glBindTexture (target, texture[index]);

    glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D (target, 0, GL_RGBA, width, height, 0, GL_BGRA,
#if IMAGE_BYTE_ORDER == MSBFirst
		  GL_UNSIGNED_INT_8_8_8_8_REV,
#else
		  GL_UNSIGNED_BYTE,
#endif
		  t0);

    glBindTexture (target, 0);
}

bool
WaterScreen::fboPrologue (int tIndex)
{
    if (!fbo)
	return false;

    if (!texture[tIndex])
	allocTexture (tIndex);

    GL::bindFramebuffer (GL_FRAMEBUFFER_EXT, fbo);

    GL::framebufferTexture2D (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			      target, texture[tIndex], 0);

    glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
    glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);

    /* check status the first time */
    if (!fboStatus)
    {
	fboStatus = GL::checkFramebufferStatus (GL_FRAMEBUFFER_EXT);
	if (fboStatus != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
	    compLogMessage ("water", CompLogLevelError,
			    "framebuffer incomplete");

	    GL::bindFramebuffer (GL_FRAMEBUFFER_EXT, 0);
	    GL::deleteFramebuffers (1, &fbo);

	    glDrawBuffer (GL_BACK);
	    glReadBuffer (GL_BACK);

	    fbo = 0;

	    return false;
	}
    }

    glViewport (0, 0, width, height);
    glMatrixMode (GL_PROJECTION);
    glPushMatrix ();
    glLoadIdentity ();
    glOrtho (0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glLoadIdentity ();

    return true;
}

void
WaterScreen::fboEpilogue ()
{
    GL::bindFramebuffer (GL_FRAMEBUFFER_EXT, 0);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    glDepthRange (0, 1);
    glViewport (-1, -1, 2, 2);

    gScreen->resetRasterPos ();

    gScreen->setDefaultViewport ();

    glMatrixMode (GL_PROJECTION);
    glPopMatrix ();
    glMatrixMode (GL_MODELVIEW);
    glPopMatrix ();

    glDrawBuffer (GL_BACK);
    glReadBuffer (GL_BACK);
}

bool
WaterScreen::fboUpdate (float dt, float fade)
{
    if (!fboPrologue (TINDEX (this, 1)))
	return false;

    if (!texture[TINDEX (this, 2)])
	allocTexture (TINDEX (this, 2));

    if (!texture[TINDEX (this, 0)])
	allocTexture (TINDEX (this, 0));

    glEnable (target);

    GL::activeTexture (GL_TEXTURE0_ARB);
    glBindTexture (target, texture[TINDEX (this, 2)]);

    glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL::activeTexture (GL_TEXTURE1_ARB);
    glBindTexture (target, texture[TINDEX (this, 0)]);
    glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnable (GL_FRAGMENT_PROGRAM_ARB);
    GL::bindProgram (GL_FRAGMENT_PROGRAM_ARB, program);

    GL::programLocalParameter4f (GL_FRAGMENT_PROGRAM_ARB, 0,
				 dt * K, fade, 1.0f, 1.0f);

    glBegin (GL_QUADS);

    glTexCoord2f (0.0f, 0.0f);
    glVertex2f   (0.0f, 0.0f);
    glTexCoord2f (tx, 0.0f);
    glVertex2f   (1.0f, 0.0f);
    glTexCoord2f (tx, ty);
    glVertex2f   (1.0f, 1.0f);
    glTexCoord2f (0.0f, ty);
    glVertex2f   (0.0f, 1.0f);

    glEnd ();

    glDisable (GL_FRAGMENT_PROGRAM_ARB);

    glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture (target, 0);
    GL::activeTexture (GL_TEXTURE0_ARB);
    glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture (target, 0);

    glDisable (target);

    fboEpilogue ();

    /* increment texture index */
    tIndex = TINDEX (this, 1);

    return true;
}

bool
WaterScreen::fboVertices (GLenum type,
			  XPoint *p,
			  int    n,
			  float  v)
{
    if (!fboPrologue (TINDEX (this, 0)))
	return false;

    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glColor4f (0.0f, 0.0f, 0.0f, v);

    glPointSize (3.0f);
    glLineWidth (1.0f);

    glScalef (1.0f / width, 1.0f / height, 1.0);
    glTranslatef (0.5f, 0.5f, 0.0f);

    glBegin (type);

    while (n--)
    {
	glVertex2i (p->x, p->y);
	p++;
    }

    glEnd ();

    glColor4usv (defaultColor);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    fboEpilogue ();

    return true;
}

void
WaterScreen::softwareUpdate (float dt, float fade)
{
    float         *dTmp;
    int           i, j;
    float         v0, v1, inv;
    float         accel, value;
    unsigned char *t0, *t;
    int           dWidth, dHeight;
    float         *d01, *d10, *d11, *d12;

    if (!texture[TINDEX (this, 0)])
	allocTexture (TINDEX (this, 0));

    dt *= K * 2.0f;
    fade *= 0.99f;

    dWidth  = width  + 2;
    dHeight = height + 2;

#define D(d, j) (*((d) + (j)))

    d01 = d0 + dWidth;
    d10 = d1;
    d11 = d10 + dWidth;
    d12 = d11 + dWidth;

    for (i = 1; i < dHeight - 1; i++)
    {
	for (j = 1; j < dWidth - 1; j++)
	{
	    accel = dt * (D (d10, j)     +
			  D (d12, j)     +
			  D (d11, j - 1) +
			  D (d11, j + 1) - 4.0f * D (d11, j));

	    value = (2.0f * D (d11, j) - D (d01, j) + accel) * fade;

	    value = CLAMP (value, 0.0f, 1.0f);

	    D (d01, j) = value;
	}

	d01 += dWidth;
	d10 += dWidth;
	d11 += dWidth;
	d12 += dWidth;
    }

    /* update border */
    memcpy (d0, d0 + dWidth, dWidth * sizeof (GLfloat));
    memcpy (d0 + dWidth * (dHeight - 1),
	    d0 + dWidth * (dHeight - 2),
	    dWidth * sizeof (GLfloat));

    d01 = d0 + dWidth;

    for (i = 1; i < dHeight - 1; i++)
    {
	D (d01, 0)	    = D (d01, 1);
	D (d01, dWidth - 1) = D (d01, dWidth - 2);

	d01 += dWidth;
    }

    d10 = d1;
    d11 = d10 + dWidth;
    d12 = d11 + dWidth;

    t0 = this->t0;

    /* update texture */
    for (i = 0; i < height; i++)
    {
	for (j = 0; j < width; j++)
	{
	    v0 = (D (d12, j)     - D (d10, j))     * 1.5f;
	    v1 = (D (d11, j - 1) - D (d11, j + 1)) * 1.5f;

	    /* 0.5 for scale */
	    inv = 0.5f / sqrtf (v0 * v0 + v1 * v1 + 1.0f);

	    /* add scale and bias to normal */
	    v0 = v0 * inv + 0.5f;
	    v1 = v1 * inv + 0.5f;

	    /* store normal map in RGB components */
	    t = t0 + (j * 4);
	    t[0] = (unsigned char) ((inv + 0.5f) * 255.0f);
	    t[1] = (unsigned char) (v1 * 255.0f);
	    t[2] = (unsigned char) (v0 * 255.0f);

	    /* store height in A component */
	    t[3] = (unsigned char) (D (d11, j) * 255.0f);
	}

	d10 += dWidth;
	d11 += dWidth;
	d12 += dWidth;

	t0 += width * 4;
    }

#undef D

    /* swap height maps */
    dTmp   = d0;
    d0 = d1;
    d1 = dTmp;

    if (texture[TINDEX (this, 0)])
    {
	glBindTexture (target, texture[TINDEX (this, 0)]);
	glTexImage2D (target, 0, GL_RGBA, width, height, 0, GL_BGRA,
#if IMAGE_BYTE_ORDER == MSBFirst
		      GL_UNSIGNED_INT_8_8_8_8_REV,
#else
		      GL_UNSIGNED_BYTE,
#endif
		      this->t0);
    }
}


#define SET(x, y, v) *((d1) + (width + 2) * (y + 1) + (x + 1)) = (v)

void
WaterScreen::softwarePoints (XPoint *p,
			     int     n,
			     float   add)
{
    while (n--)
    {
	SET (p->x - 1, p->y - 1, add);
	SET (p->x, p->y - 1, add);
	SET (p->x + 1, p->y - 1, add);

	SET (p->x - 1, p->y, add);
	SET (p->x, p->y, add);
	SET (p->x + 1, p->y, add);

	SET (p->x - 1, p->y + 1, add);
	SET (p->x, p->y + 1, add);
	SET (p->x + 1, p->y + 1, add);

	p++;
    }
}

/* bresenham */
void
WaterScreen::softwareLines (XPoint *p,
			    int    n,
			    float  v)
{
    int	 x1, y1, x2, y2;
    bool steep;
    int  tmp;
    int  deltaX, deltaY;
    int  error = 0;
    int  yStep;
    int  x, y;

#define SWAP(v0, v1) \
    tmp = v0;	     \
    v0 = v1;	     \
    v1 = tmp

    while (n > 1)
    {
	x1 = p->x;
	y1 = p->y;

	p++;
	n--;

	x2 = p->x;
	y2 = p->y;

	p++;
	n--;

	steep = abs (y2 - y1) > abs (x2 - x1);
	if (steep)
	{
	    SWAP (x1, y1);
	    SWAP (x2, y2);
	}

	if (x1 > x2)
	{
	    SWAP (x1, x2);
	    SWAP (y1, y2);
	}

#undef SWAP

	deltaX = x2 - x1;
	deltaY = abs (y2 - y1);

	y = y1;
	if (y1 < y2)
	    yStep = 1;
	else
	    yStep = -1;

	for (x = x1; x <= x2; x++)
	{
	    if (steep)
	    {
		SET (y, x, v);
	    }
	    else
	    {
		SET (x, y, v);
	    }

	    error += deltaY;
	    if (2 * error >= deltaX)
	    {
		y += yStep;
		error -= deltaX;
	    }
	}
    }
}

#undef SET

void
WaterScreen::softwareVertices (GLenum type,
			       XPoint *p,
			       int    n,
			       float  v)
{
    switch (type) {
	case GL_POINTS:
	    softwarePoints (p, n, v);
	    break;
	case GL_LINES:
	    softwareLines (p, n, v);
	    break;
    }
}

void
WaterScreen::waterUpdate (float dt)
{
    GLfloat fade = 1.0f;

    if (count < 1000)
    {
	if (count > 1)
	    fade = 0.90f + count / 10000.0f;
	else
	    fade = 0.0f;
    }

    if (!fboUpdate (dt, fade))
	softwareUpdate (dt, fade);
}

void
WaterScreen::scaleVertices (XPoint *p, int n)
{
    while (n--)
    {
	p[n].x = (width  * p[n].x) / screen->width ();
	p[n].y = (height * p[n].y) / screen->height ();
    }
}

void
WaterScreen::waterVertices (GLenum type,
			    XPoint *p,
			    int    n,
			    float  v)
{
    if (!GL::fragmentProgram)
	return;

    scaleVertices (p, n);

    if (!fboVertices (type, p, n, v))
	softwareVertices (type, p, n, v);

    if (count <= 0)
    {
	WaterWindow *ww;

	cScreen->preparePaintSetEnabled (this, true);
	cScreen->donePaintSetEnabled (this, true);
	foreach (CompWindow *w, screen->windows ())
	{
	    ww = WaterWindow::get (w);
	    ww->gWindow->glDrawTextureSetEnabled (ww, true);
	}
    }

    if (count < 3000)
	count = 3000;
}

bool
WaterScreen::rainTimeout ()
{
    XPoint     p;

    p.x = (int) (screen->width ()  * (rand () / (float) RAND_MAX));
    p.y = (int) (screen->height () * (rand () / (float) RAND_MAX));

    waterVertices (GL_POINTS, &p, 1, 0.8f * (rand () / (float) RAND_MAX));

    cScreen->damageScreen ();

    return true;
}

bool
WaterScreen::wiperTimeout ()
{
    if (count)
    {
	if (wiperAngle == 0.0f)
	    wiperSpeed = 2.5f;
	else if (wiperAngle == 180.0f)
	    wiperSpeed = -2.5f;
    }

    return true;
}

void
WaterScreen::waterReset ()
{
    int size, i, j;

    height = TEXTURE_SIZE;
    width  = (height * screen->width ()) / screen->height ();

    if (GL::textureNonPowerOfTwo ||
	(POWER_OF_TWO (width) && POWER_OF_TWO (height)))
    {
	target = GL_TEXTURE_2D;
	tx = ty = 1.0f;
    }
    else
    {
	target = GL_TEXTURE_RECTANGLE_NV;
	tx = width;
	ty = height;
    }

    if (!GL::fragmentProgram)
	return;

    if (GL::fbo)
    {
	loadWaterProgram ();
	if (!fbo)
	    GL::genFramebuffers (1, &fbo);
    }

    fboStatus = 0;

    for (i = 0; i < TEXTURE_NUM; i++)
    {
	if (texture[i])
	{
	    glDeleteTextures (1, &texture[i]);
	    texture[i] = 0;
	}
    }

    if (data)
	free (data);

    size = (width + 2) * (height + 2);

    data = calloc (1, (sizeof (float) * size * 2) +
		   (sizeof (GLubyte) * width * height * 4));
    if (!data)
	return;

    d0 = (float *)data;
    d1 = (d0 + (size));
    t0 = (unsigned char *) (d1 + (size));

    for (i = 0; i < height; i++)
    {
	for (j = 0; j < width; j++)
	{
	    (t0 + (width * 4 * i + j * 4))[0] = 0xff;
	}
    }
}

void
WaterWindow::glDrawTexture (GLTexture          *texture,
			    GLFragment::Attrib &attrib,
			    unsigned int       mask)
{
    if (wScreen->count)
    {
	GLFragment::Attrib     fa (attrib);
	bool                   lighting = wScreen->gScreen->lighting ();
	int                    param, unit;
	GLFragment::FunctionId function;
	GLfloat                plane[4];

	param = fa.allocParameters (1);
	unit  = fa.allocTextureUnits (1);

	function = wScreen->getBumpMapFragmentFunction (texture, unit, param);
	if (function)
	{
	    fa.addFunction (function);

	    gScreen->setLighting (true);

	    GL::activeTexture (GL_TEXTURE0_ARB + unit);

	    glBindTexture (wScreen->target,
			   wScreen->texture[TINDEX (wScreen, 0)]);

	    plane[1] = plane[2] = 0.0f;
	    plane[0] = wScreen->tx / (GLfloat) screen->width ();
	    plane[3] = 0.0f;

	    glTexGeni (GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	    glTexGenfv (GL_S, GL_EYE_PLANE, plane);
	    glEnable (GL_TEXTURE_GEN_S);

	    plane[0] = plane[2] = 0.0f;
	    plane[1] = wScreen->ty / (GLfloat) screen->height ();
	    plane[3] = 0.0f;

	    glTexGeni (GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR);
	    glTexGenfv (GL_T, GL_EYE_PLANE, plane);
	    glEnable (GL_TEXTURE_GEN_T);

	    GL::activeTexture (GL_TEXTURE0_ARB);

	    GL::programEnvParameter4f (GL_FRAGMENT_PROGRAM_ARB, param,
				       texture->matrix ().yy *
				       wScreen->offsetScale,
				       -texture->matrix ().xx *
				       wScreen->offsetScale,
				       0.0f, 0.0f);
	}

	/* to get appropriate filtering of texture */
	mask |= PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;

	gWindow->glDrawTexture (texture, fa, mask);

	if (function)
	{
	    GL::activeTexture (GL_TEXTURE0_ARB + unit);
	    glDisable (GL_TEXTURE_GEN_T);
	    glDisable (GL_TEXTURE_GEN_S);
	    glBindTexture (wScreen->target, 0);
	    GL::activeTexture (GL_TEXTURE0_ARB);

	    gScreen->setLighting (lighting);
	}
    }
    else
    {
	gWindow->glDrawTexture (texture, attrib, mask);
    }
}

/* TODO: a way to control the speed */
void
WaterScreen::preparePaint (int msSinceLastPaint)
{
    if (count)
    {
	count -= 10;
	if (count < 0)
	    count = 0;

	if (wiperTimer.active ())
	{
	    float  step, angle0, angle1;
	    bool   wipe = false;
	    XPoint p[3];

	    p[1].x = screen->width () / 2;
	    p[1].y = screen->height ();

	    step = wiperSpeed * msSinceLastPaint / 20.0f;

	    if (wiperSpeed > 0.0f)
	    {
		if (wiperAngle < 180.0f)
		{
		    angle0 = wiperAngle;

		    wiperAngle += step;
		    wiperAngle = MIN (wiperAngle, 180.0f);

		    angle1 = wiperAngle;

		    wipe = true;
		}
	    }
	    else
	    {
		if (wiperAngle > 0.0f)
		{
		    angle1 = wiperAngle;

		    wiperAngle += step;
		    wiperAngle = MAX (wiperAngle, 0.0f);

		    angle0 = wiperAngle;

		    wipe = true;
		}
	    }

#define TAN(a) (tanf ((a) * (M_PI / 180.0f)))

	    if (wipe)
	    {
		if (angle0 > 0.0f)
		{
		    p[2].x = screen->width () / 2 -
			     screen->height () / TAN (angle0);
		    p[2].y = 0;
		}
		else
		{
		    p[2].x = 0;
		    p[2].y = screen->height ();
		}

		if (angle1 < 180.0f)
		{
		    p[0].x = screen->width () / 2 -
			     screen->height () / TAN (angle1);
		    p[0].y = 0;
		}
		else
		{
		    p[0].x = screen->width ();
		    p[0].y = screen->height ();
		}

		/* software rasterizer doesn't support triangles yet so wiper
		   effect will only work with FBOs right now */
		waterVertices (GL_TRIANGLES, p, 3, 0.0f);
	    }

#undef TAN

	}

	waterUpdate (0.8f);
    }

    cScreen->preparePaint (msSinceLastPaint);
}

void
WaterScreen::donePaint ()
{
    if (count)
	cScreen->damageScreen ();
    else
    {
	WaterWindow *ww;

	cScreen->preparePaintSetEnabled (this, false);
	cScreen->donePaintSetEnabled (this, false);
	foreach (CompWindow *w, screen->windows ())
	{
	    ww = WaterWindow::get (w);
	    ww->gWindow->glDrawTextureSetEnabled (ww, false);
	}
    }

    cScreen->donePaint ();
}

void
WaterScreen::handleMotionEvent ()
{
    if (grabIndex)
    {
	XPoint p[2];

	p[0].x = waterLastPointerX;
	p[0].y = waterLastPointerY;

	p[1].x = waterLastPointerX = pointerX;
	p[1].y = waterLastPointerY = pointerY;

	waterVertices (GL_LINES, p, 2, 0.2f);

	cScreen->damageScreen ();
    }

}

static bool
waterInitiate (CompAction         *action,
	       CompAction::State  state,
	       CompOption::Vector &options)
{
    unsigned int ui;
    Window	 root, child;
    int	         xRoot, yRoot, i;

    WATER_SCREEN (screen);

    if (!screen->otherGrabExist ("water", NULL))
    {
	if (!ws->grabIndex)
	{
	    ws->grabIndex = screen->pushGrab (None, "water");
	    screen->handleEventSetEnabled (ws, true);
	}

	if (XQueryPointer (screen->dpy (), screen->root (), &root, &child,
			   &xRoot, &yRoot, &i, &i, &ui))
	{
	    XPoint p;

	    p.x = waterLastPointerX = xRoot;
	    p.y = waterLastPointerY = yRoot;

	    ws->waterVertices (GL_POINTS, &p, 1, 0.8f);

	    ws->cScreen->damageScreen ();
	}
    }

    if (state & CompAction::StateInitButton)
	action->setState (action->state () | CompAction::StateTermButton);

    if (state & CompAction::StateInitKey)
	action->setState (action->state () | CompAction::StateTermKey);

    return false;
}

static bool
waterTerminate (CompAction         *action,
	        CompAction::State  state,
	        CompOption::Vector &options)
{
    WATER_SCREEN (screen);

    if (ws->grabIndex)
    {
	screen->removeGrab (ws->grabIndex, 0);
	ws->grabIndex = 0;
	screen->handleEventSetEnabled (ws, false);
    }

    return false;
}

static bool
waterToggleRain (CompAction         *action,
		 CompAction::State  state,
		 CompOption::Vector &options)
{
    /* Remember StateCancel and StateCommit will be broadcast to all actions
       so we need to verify that we are actually being toggled... */
    if (!(state & CompAction::StateTermKey))
        return false;
    /* And only respond to key taps */
    if (!(state & CompAction::StateTermTapped))
        return false;

    WATER_SCREEN (screen);

    if (!ws->rainTimer.active ())
    {
	int delay;

	delay = ws->optionGetRainDelay ();
	ws->rainTimer.start (delay, (float) delay * 1.2);
    }
    else
    {
	ws->rainTimer.stop ();
    }

    return false;
}

static bool
waterToggleWiper (CompAction         *action,
		  CompAction::State  state,
		  CompOption::Vector &options)
{
    WATER_SCREEN (screen);

    if (!ws->wiperTimer.active ())
    {
	ws->wiperTimer.start (2000, 2400);
    }
    else
    {
	ws->wiperTimer.stop ();
    }

    return false;
}

static bool
waterTitleWave (CompAction         *action,
		CompAction::State  state,
		CompOption::Vector &options)
{
    CompWindow *w;
    int	       xid;

    WATER_SCREEN (screen);

    xid = CompOption::getIntOptionNamed (options, "window",
					 screen->activeWindow ());

    w = screen->findWindow (xid);
    if (w)
    {
	CompWindow::Geometry &g = w->geometry ();
	XPoint p[2];

	p[0].x = g.x () - w->border ().left;
	p[0].y = g.y () - w->border ().top / 2;

	p[1].x = g.x () + g.width () + w->border ().right;
	p[1].y = p[0].y;

	ws->waterVertices (GL_LINES, p, 2, 0.15f);

	ws->cScreen->damageScreen ();
    }

    return false;
}

static bool
waterPoint (CompAction         *action,
	    CompAction::State  state,
	    CompOption::Vector &options)
{
    XPoint p;
    float  amp;

    WATER_SCREEN (screen);

    p.x = CompOption::getIntOptionNamed (options, "x",
					 screen->width () / 2);
    p.y = CompOption::getIntOptionNamed (options, "y",
					 screen->height () / 2);

    amp = CompOption::getFloatOptionNamed (options, "amplitude", 0.5f);

    ws->waterVertices (GL_POINTS, &p, 1, amp);

    ws->cScreen->damageScreen ();

    return false;
}

static bool
waterLine (CompAction         *action,
	   CompAction::State  state,
	   CompOption::Vector &options)
{
    XPoint p[2];
    float  amp;

    WATER_SCREEN (screen);

    p[0].x = CompOption::getIntOptionNamed (options, "x0",
					    screen->width () / 4);
    p[0].y = CompOption::getIntOptionNamed (options, "y0",
					    screen->height () / 2);

    p[1].x = CompOption::getIntOptionNamed (options, "x1",
					    screen->width () -
					    screen->width () / 4);
    p[1].y = CompOption::getIntOptionNamed (options, "y1",
					    screen->height () / 2);

    amp = CompOption::getFloatOptionNamed (options, "amplitude", 0.25f);

    ws->waterVertices (GL_LINES, p, 2, amp);

    ws->cScreen->damageScreen ();

    return false;
}

void
WaterScreen::handleEvent (XEvent *event)
{
    switch (event->type) {
	case ButtonPress:
	    if (event->xbutton.root == screen->root () && grabIndex)
	    {
		XPoint p;

		p.x = pointerX;
		p.y = pointerY;

		waterVertices (GL_POINTS, &p, 1, 0.8f);
		cScreen->damageScreen ();
	    }
	    break;
	case EnterNotify:
	case LeaveNotify:
	    if (event->xcrossing.root == screen->root () && grabIndex)
		handleMotionEvent ();
	    break;
	case MotionNotify:
	    if (event->xmotion.root == screen->root () && grabIndex)
		handleMotionEvent ();
	default:
	    break;
    }

    screen->handleEvent (event);
}

void
WaterScreen::optionChange (WaterOptions::Options num)
{
    switch (num) {
	case WaterOptions::OffsetScale:
	    offsetScale = optionGetOffsetScale () * 50.0f;
	    break;
	case WaterOptions::RainDelay:
	    if (rainTimer.active ())
	    {
		rainTimer.setTimes (optionGetRainDelay (),
				    (float)optionGetRainDelay () * 1.2);
	    }
	    break;
	default:
	    break;
    }
}

WaterScreen::WaterScreen (CompScreen *screen) :
    PluginClassHandler<WaterScreen,CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    grabIndex (0),
    width (0),
    height (0),

    program (0),

    tIndex (0),
    target (0),
    tx (0),
    ty (0),

    count (0),

    fbo (0),
    fboStatus (0),

    data (NULL),
    d0 (NULL),
    d1 (NULL),
    t0 (NULL),

    wiperAngle (0),
    wiperSpeed (0),

    bumpMapFunctions ()
{
    offsetScale = optionGetOffsetScale () * 50.0f;

    memset (texture, 0, sizeof (GLuint) * TEXTURE_NUM);

    wiperTimer.setCallback (boost::bind (&WaterScreen::wiperTimeout, this));
    rainTimer.setCallback (boost::bind (&WaterScreen::rainTimeout, this));

    waterReset ();

    optionSetOffsetScaleNotify (boost::bind (&WaterScreen::optionChange, this, _2));
    optionSetRainDelayNotify (boost::bind (&WaterScreen::optionChange, this, _2));

    optionSetInitiateKeyInitiate (waterInitiate);
    optionSetInitiateKeyTerminate (waterTerminate);
    optionSetToggleRainKeyTerminate (waterToggleRain);
    optionSetToggleWiperKeyInitiate (waterToggleWiper);
    optionSetTitleWaveInitiate (waterTitleWave);
    optionSetPointInitiate (waterPoint);
    optionSetLineInitiate (waterLine);

    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
}

WaterScreen::~WaterScreen ()
{
    if (fbo)
	GL::deleteFramebuffers (1, &fbo);

    for (unsigned int i = 0; i < TEXTURE_NUM; i++)
    {
	if (texture[i])
	    glDeleteTextures (1, &texture[i]);
    }

    if (program)
	GL::deletePrograms (1, &program);

    if (data)
	free (data);

    foreach (WaterFunction &f, bumpMapFunctions)
    {
	GLFragment::destroyFragmentFunction (f.id);
    }
}

bool
WaterPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION) |
        !CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI) |
        !CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	 return false;

    return true;
}

