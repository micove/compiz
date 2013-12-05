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

#include <sstream>
#include <boost/scoped_array.hpp>

#include "screenshot.h"

#include <dirent.h>

#if defined(HAVE_SCANDIR_POSIX)
  // POSIX (2008) defines the comparison function like this:
  #define scandir(a,b,c,d) scandir((a), (b), (c), (int(*)(const dirent **, const dirent **))(d));
#else
  #define scandir(a,b,c,d) scandir((a), (b), (c), (int(*)(const void*,const void*))(d));
#endif

COMPIZ_PLUGIN_20090315 (screenshot, ShotPluginVTable)

bool
ShotScreen::initiate (CompAction            *action,
		      CompAction::State     state,
		      CompOption::Vector    &options)
{
    Window xid = CompOption::getIntOptionNamed (options, "root");

    if (xid != ::screen->root () ||
	::screen->otherGrabExist ("screenshot", NULL))
	return false;

    if (!mGrabIndex)
    {
	mGrabIndex = ::screen->pushGrab (None, "screenshot");
	screen->handleEventSetEnabled (this, true);
    }

    if (state & CompAction::StateInitButton)
	action->setState (action->state () | CompAction::StateTermButton);

    /* Start selection screenshot rectangle */

    mX1 = mX2 = pointerX;
    mY1 = mY2 = pointerY;

    mGrab = true;

    gScreen->glPaintOutputSetEnabled (this, true);

    return true;
}

bool
ShotScreen::terminate (CompAction            *action,
		       CompAction::State     state,
		       CompOption::Vector    &options)
{
    Window xid = CompOption::getIntOptionNamed (options, "root");

    if (xid && xid != ::screen->root ())
	return false;

    if (mGrabIndex)
    {
	/* Enable screen capture */
	cScreen->paintSetEnabled (this, true);
	::screen->removeGrab (mGrabIndex, 0);
	mGrabIndex = 0;

	::screen->handleEventSetEnabled (this, false);

	if (state & CompAction::StateCancel)
	    mGrab = false;

	if (mX1 != mX2 && mY1 != mY2)
	{
	    int x1 = MIN (mX1, mX2) - 1;
	    int y1 = MIN (mY1, mY2) - 1;
	    int x2 = MAX (mX1, mX2) + 1;
	    int y2 = MAX (mY1, mY2) + 1;

	    cScreen->damageRegion (CompRegion (x1, y1, x2 - x1, y2 - y1));
	}
    }

    action->setState (action->state () & ~(CompAction::StateTermKey |
					   CompAction::StateTermButton));

    return false;
}

static int
shotFilter (const struct dirent *d)
{
    int number;

    if (sscanf (d->d_name, "screenshot%d.png", &number))
    {
	int nDigits = 0;

	for (; number > 0; number /= 10)
	    ++nDigits;

	/* Make sure there are no trailing characters in the name */
	if ((int) strlen (d->d_name) == 14 + nDigits)
	    return 1;
    }

    return 0;
}

static int
shotSort (const void *_a,
	  const void *_b)
{
    struct dirent **a = (struct dirent **) _a;
    struct dirent **b = (struct dirent **) _b;
    int		  al  = strlen ((*a)->d_name);
    int		  bl  = strlen ((*b)->d_name);

    if (al == bl)
	return strcoll ((*a)->d_name, (*b)->d_name);
    else
	return al - bl;
}

void
ShotScreen::paint (CompOutput::ptrList &outputs,
		   unsigned int        mask)
{
    if (mGrab)
    {
	if (!mGrabIndex)
	{
	    /* Taking screenshot, enable full paint on
	     * this frame */

	    outputs.clear ();
	    outputs.push_back (&screen->fullscreenOutput ());
	}
    }

    cScreen->paint (outputs, mask);
}

namespace
{
    bool paintSelectionRectangleFill (const CompRect &rect,
				      unsigned short *fillColor,
				      GLVertexBuffer *streamingBuffer,
				      const GLMatrix &transform)
    {
	GLfloat         vertexData[12];
	GLushort        colorData[4];

	int x1 = rect.x1 ();
	int y1 = rect.y1 ();
	int x2 = rect.x2 ();
	int y2 = rect.y2 ();

	const float MaxUShortFloat = std::numeric_limits <unsigned short>::max ();

	/* draw filled rectangle */
	float alpha = fillColor[3] / MaxUShortFloat;

	colorData[0] = alpha * fillColor[0];
	colorData[1] = alpha * fillColor[1];
	colorData[2] = alpha * fillColor[2];
	colorData[3] = alpha * MaxUShortFloat;

	vertexData[0]  = x1;
	vertexData[1]  = y1;
	vertexData[2]  = 0.0f;
	vertexData[3]  = x1;
	vertexData[4]  = y2;
	vertexData[5]  = 0.0f;
	vertexData[6]  = x2;
	vertexData[7]  = y1;
	vertexData[8]  = 0.0f;
	vertexData[9]  = x2;
	vertexData[10] = y2;
	vertexData[11] = 0.0f;

	streamingBuffer->begin (GL_TRIANGLE_STRIP);

	streamingBuffer->addColors (1, colorData);
	streamingBuffer->addVertices (4, vertexData);

	if (streamingBuffer->end ())
	{
	    glEnable (GL_BLEND);

	    streamingBuffer->render (transform);

	    glDisable (GL_BLEND);

	    return true;
	}

	return false;
    }

    bool paintSelectionRectangleOutline (const CompRect &rect,
					 unsigned short *outlineColor,
					 GLVertexBuffer *streamingBuffer,
					 const GLMatrix &transform)
    {
	GLfloat         vertexData[12];
	GLushort        colorData[4];

	int x1 = rect.x1 ();
	int y1 = rect.y1 ();
	int x2 = rect.x2 ();
	int y2 = rect.y2 ();

	const float MaxUShortFloat = std::numeric_limits <unsigned short>::max ();

	/* draw outline */
	float alpha = outlineColor[3] / MaxUShortFloat;

	colorData[0] = alpha * outlineColor[0];
	colorData[1] = alpha * outlineColor[1];
	colorData[2] = alpha * outlineColor[2];
	colorData[3] = alpha * MaxUShortFloat;

	vertexData[0]  = x1;
	vertexData[1]  = y1;
	vertexData[2]  = 0.0f;
	vertexData[3]  = x1;
	vertexData[4]  = y2;
	vertexData[5]  = 0.0f;
	vertexData[6]  = x2;
	vertexData[7]  = y2;
	vertexData[8]  = 0.0f;
	vertexData[9]  = x2;
	vertexData[10] = y1;
	vertexData[11] = 0.0f;

	streamingBuffer->begin (GL_LINE_LOOP);

	streamingBuffer->addColors (1, colorData);
	streamingBuffer->addVertices (4, vertexData);

	if (streamingBuffer->end ())
	{
	    glEnable (GL_BLEND);
	    glLineWidth (2.0);

	    streamingBuffer->render (transform);

	    glDisable (GL_BLEND);

	    return true;
	}

	return false;
    }

    void
    ensureDirectoryForImage (CompString &directory)
    {
	/* If dir is empty, use user's desktop directory instead */
	if (directory.length () == 0)
	    directory = getXDGUserDir (XDGUserDirDesktop);
    }

    int
    getImageNumberFromDirectory (const CompString &directory)
    {
	struct dirent **namelist;

	int n = scandir (directory.c_str (), &namelist, shotFilter, shotSort);

	if (n >= 0)
	{
	    int  number = 0;

	    if (n > 0)
		sscanf (namelist[n - 1]->d_name,
			"screenshot%d.png",
			&number);

	    ++number;

	    if (n)
		free (namelist);

	    return number;
	}
	else
	{
	    perror ("scandir");
	    return 0;
	}
    }

    CompString
    getImageAbsolutePath (const CompString &directory,
			  int              number)
    {
	std::stringstream ss;
	ss << directory << "/screenshot" << number << ".png";
	return ss.str ();
    }

    bool
    saveBuffer (const boost::scoped_array <GLubyte> &buffer,
		int                                 w,
		int                                 h,
		const CompString                    &path)
    {
	CompSize imageSize (w, h);

	if (!::screen->writeImageToFile (const_cast <CompString &> (path),
					 "png",
					 imageSize,
					 buffer.get ()))
	{
	    compLogMessage ("screenshot", CompLogLevelError,
			    "failed to write screenshot image");
	    return false;
	}

	return true;
    }

    bool
    launchApplicationAndTakeScreenshot (const CompString &app,
					const CompString &directory)
    {
	if (app.length () > 0)
	{
	    ::screen->runCommand (app + " " + directory);
	    return true;
	}

	return false;
    }

    bool
    readFromGPUBufferToCPUBuffer (const CompRect                &rect,
				  boost::scoped_array <GLubyte> &buffer)
    {
	int x1 = rect.x1 ();
	int y1 = rect.y1 ();
	int x2 = rect.x2 ();
	int y2 = rect.y2 ();

	int w = x2 - x1;
	int h = y2 - y1;

	if (w && h)
	{
	    size_t size = w * h * 4;
	    buffer.reset (new GLubyte[size]);

	    if (buffer.get ())
	    {
		GLint drawBinding = 0;
		GLint readBinding = 0;

		/* Bind the currently bound draw framebuffer to
		 * the read framebuffer and read from it */
		if (GL::fboEnabled)
		{
		    glGetIntegerv (GL::DRAW_FRAMEBUFFER_BINDING, &drawBinding);
		    glGetIntegerv (GL::READ_FRAMEBUFFER_BINDING, &readBinding);
		    (GL::bindFramebuffer) (GL::READ_FRAMEBUFFER, drawBinding);
		}

		glGetError ();
		glReadPixels (x1, ::screen->height () - y2, w, h,
			      GL_RGBA, GL_UNSIGNED_BYTE,
			      (GLvoid *) buffer.get ());

		if (GL::fboEnabled)
		    (GL::bindFramebuffer) (GL::READ_FRAMEBUFFER, readBinding);

		if (glGetError () != GL_NO_ERROR)
		    return false;

		return true;
	    }
	}

	return false;
    }

    /* We need to take directory by copy because
     * it may be modified later */
    bool saveScreenshot (CompRect         rect,
			 CompString       directory,
			 const CompString &alternativeApplication)
    {
	ensureDirectoryForImage (directory);

	int number = getImageNumberFromDirectory (directory);
	CompString path = getImageAbsolutePath (directory, number);

	boost::scoped_array <GLubyte> buffer;

	bool success = readFromGPUBufferToCPUBuffer (rect,
						     buffer);

	if (success)
	{
	    success = saveBuffer (buffer,
				  rect.width (),
				  rect.height (), path);
	}
	else
	{
	    compLogMessage ("screenshot", CompLogLevelWarn, "glReadPixels failed");
	}

	if (!success)
	    success =
		launchApplicationAndTakeScreenshot (alternativeApplication,
						    directory);

	return success;

    }
}

bool
ShotScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			   const GLMatrix            &matrix,
			   const CompRegion          &region,
			   CompOutput                *output,
			   unsigned int               mask)
{
    bool status = gScreen->glPaintOutput (attrib, matrix, region, output, mask);

    if (status && mGrab)
    {
	/* We just want to draw the screenshot selection box if
	 * we are grabbed, the size has changed and the CCSM
	 * option to draw it is enabled. */

	CompRect selectionRect (std::min (mX1, mX2),
				std::min (mY1, mY2),
				std::abs (mX2 - mX1),
				std::abs (mY2 - mY1));

	if (mGrabIndex &&
	    selectionSizeChanged &&
	    optionGetDrawSelectionIndicator ())
	{
	    GLMatrix        transform (matrix);
	    GLVertexBuffer *streamingBuffer (GLVertexBuffer::streamingBuffer ());
	    transform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	    paintSelectionRectangleFill (selectionRect,
					 optionGetSelectionFillColor (),
					 streamingBuffer,
					 transform);

	    paintSelectionRectangleOutline (selectionRect,
					    optionGetSelectionOutlineColor (),
					    streamingBuffer,
					    transform);

	    /* we finished painting the selection box,
	     * reset selectionSizeChanged now */
	    selectionSizeChanged = false;
	}
	else if (!mGrabIndex)
	{
	    /* Taking a screenshot */
	    saveScreenshot (selectionRect,
			    optionGetDirectory (),
			    optionGetLaunchApp ());
	    cScreen->paintSetEnabled (this, false);
	    gScreen->glPaintOutputSetEnabled (this, false);
	}
    }

    return status;
}

void
ShotScreen::handleMotionEvent (int xRoot,
			       int yRoot)
{
    /* update screenshot rectangle size */
    if (mGrabIndex &&
	(mX2 != xRoot || mY2 != yRoot))
    {
	/* the size has changed now */
	selectionSizeChanged = true;

	int x1 = MIN (mX1, mX2) - 1;
	int y1 = MIN (mY1, mY2) - 1;
	int x2 = MAX (mX1, mX2) + 1;
	int y2 = MAX (mY1, mY2) + 1;

	cScreen->damageRegion (CompRegion (x1, y1, x2 - x1, y2 - y1));

	mX2 = xRoot;
	mY2 = yRoot;

	x1 = MIN (mX1, mX2) - 1;
	y1 = MIN (mY1, mY2) - 1;
	x2 = MAX (mX1, mX2) + 1;
	y2 = MAX (mY1, mY2) + 1;

	cScreen->damageRegion (CompRegion (x1, y1, x2 - x1, y2 - y1));
    }
}

void
ShotScreen::handleEvent (XEvent *event)
{
    switch (event->type)
    {
	case MotionNotify:
	    if (event->xmotion.root == screen->root ())
		handleMotionEvent (pointerX, pointerY);
	    break;

	case EnterNotify:
	case LeaveNotify:
	    if (event->xcrossing.root == screen->root ())
		handleMotionEvent (pointerX, pointerY);
	    break;

	default:
	    break;
    }

    ::screen->handleEvent (event);
}

ShotScreen::ShotScreen (CompScreen *screen) :
    PluginClassHandler<ShotScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    mGrabIndex (0),
    mGrab (false),
    selectionSizeChanged (false)
{
    optionSetInitiateButtonInitiate (boost::bind (&ShotScreen::initiate, this,
						  _1, _2, _3));
    optionSetInitiateButtonTerminate (boost::bind (&ShotScreen::terminate, this,
						   _1, _2, _3));

    ScreenInterface::setHandler (screen, false);
    CompositeScreenInterface::setHandler (cScreen, false);
    GLScreenInterface::setHandler (gScreen, false);
}

bool
ShotPluginVTable::init ()
{
    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI)	&&
	CompPlugin::checkPluginABI ("compiztoolbox", COMPIZ_COMPIZTOOLBOX_ABI))
	return true;

    return false;
}
