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

#ifndef _COMPIZ_OPENGL_H
#define _COMPIZ_OPENGL_H

#include <GL/gl.h>
#include <GL/glx.h>

#include <opengl/matrix.h>
#include <opengl/texture.h>
#include <opengl/fragment.h>

#define COMPIZ_OPENGL_ABI 3

#include <core/pluginclasshandler.h>

/**
 * camera distance from screen, 0.5 * tan (FOV)
 */
#define DEFAULT_Z_CAMERA 0.866025404f

#define RED_SATURATION_WEIGHT   0.30f
#define GREEN_SATURATION_WEIGHT 0.59f
#define BLUE_SATURATION_WEIGHT  0.11f

class PrivateGLScreen;
class PrivateGLWindow;

extern GLushort   defaultColor[4];

#ifndef GLX_EXT_texture_from_pixmap
#define GLX_BIND_TO_TEXTURE_RGB_EXT        0x20D0
#define GLX_BIND_TO_TEXTURE_RGBA_EXT       0x20D1
#define GLX_BIND_TO_MIPMAP_TEXTURE_EXT     0x20D2
#define GLX_BIND_TO_TEXTURE_TARGETS_EXT    0x20D3
#define GLX_Y_INVERTED_EXT                 0x20D4
#define GLX_TEXTURE_FORMAT_EXT             0x20D5
#define GLX_TEXTURE_TARGET_EXT             0x20D6
#define GLX_MIPMAP_TEXTURE_EXT             0x20D7
#define GLX_TEXTURE_FORMAT_NONE_EXT        0x20D8
#define GLX_TEXTURE_FORMAT_RGB_EXT         0x20D9
#define GLX_TEXTURE_FORMAT_RGBA_EXT        0x20DA
#define GLX_TEXTURE_1D_BIT_EXT             0x00000001
#define GLX_TEXTURE_2D_BIT_EXT             0x00000002
#define GLX_TEXTURE_RECTANGLE_BIT_EXT      0x00000004
#define GLX_TEXTURE_1D_EXT                 0x20DB
#define GLX_TEXTURE_2D_EXT                 0x20DC
#define GLX_TEXTURE_RECTANGLE_EXT          0x20DD
#define GLX_FRONT_LEFT_EXT                 0x20DE
#endif

namespace GL {

    typedef void (*FuncPtr) (void);
    typedef FuncPtr (*GLXGetProcAddressProc) (const GLubyte *procName);

    typedef void    (*GLXBindTexImageProc)    (Display	 *display,
					       GLXDrawable	 drawable,
					       int		 buffer,
					       int		 *attribList);
    typedef void    (*GLXReleaseTexImageProc) (Display	 *display,
					       GLXDrawable	 drawable,
					       int		 buffer);
    typedef void    (*GLXQueryDrawableProc)   (Display	 *display,
					       GLXDrawable	 drawable,
					       int		 attribute,
					       unsigned int  *value);

    typedef void (*GLXCopySubBufferProc) (Display     *display,
					  GLXDrawable drawable,
					  int	  x,
					  int	  y,
					  int	  width,
					  int	  height);

    typedef int (*GLXGetVideoSyncProc)  (unsigned int *count);
    typedef int (*GLXWaitVideoSyncProc) (int	  divisor,
					 int	  remainder,
					 unsigned int *count);

    #ifndef GLX_VERSION_1_3
    typedef struct __GLXFBConfigRec *GLXFBConfig;
    #endif

    typedef GLXFBConfig *(*GLXGetFBConfigsProc) (Display *display,
						 int     screen,
						 int     *nElements);
    typedef int (*GLXGetFBConfigAttribProc) (Display     *display,
					     GLXFBConfig config,
					     int	     attribute,
					     int	     *value);
    typedef GLXPixmap (*GLXCreatePixmapProc) (Display     *display,
					      GLXFBConfig config,
					      Pixmap      pixmap,
					      const int   *attribList);
    typedef void      (*GLXDestroyPixmapProc) (Display *display,
    					       GLXPixmap pixmap);

    typedef void (*GLActiveTextureProc) (GLenum texture);
    typedef void (*GLClientActiveTextureProc) (GLenum texture);
    typedef void (*GLMultiTexCoord2fProc) (GLenum, GLfloat, GLfloat);

    typedef void (*GLGenProgramsProc) (GLsizei n,
				       GLuint  *programs);
    typedef void (*GLDeleteProgramsProc) (GLsizei n,
					  GLuint  *programs);
    typedef void (*GLBindProgramProc) (GLenum target,
				       GLuint program);
    typedef void (*GLProgramStringProc) (GLenum	  target,
					 GLenum	  format,
					 GLsizei	  len,
					 const GLvoid *string);
    typedef void (*GLProgramParameter4fProc) (GLenum  target,
					      GLuint  index,
					      GLfloat x,
					      GLfloat y,
					      GLfloat z,
					      GLfloat w);
    typedef void (*GLGetProgramivProc) (GLenum target,
					GLenum pname,
					int    *params);

    typedef void (*GLGenFramebuffersProc) (GLsizei n,
					   GLuint  *framebuffers);
    typedef void (*GLDeleteFramebuffersProc) (GLsizei n,
					      GLuint  *framebuffers);
    typedef void (*GLBindFramebufferProc) (GLenum target,
					   GLuint framebuffer);
    typedef GLenum (*GLCheckFramebufferStatusProc) (GLenum target);
    typedef void (*GLFramebufferTexture2DProc) (GLenum target,
						GLenum attachment,
						GLenum textarget,
						GLuint texture,
						GLint  level);
    typedef void (*GLGenerateMipmapProc) (GLenum target);

    extern GLXBindTexImageProc      bindTexImage;
    extern GLXReleaseTexImageProc   releaseTexImage;
    extern GLXQueryDrawableProc     queryDrawable;
    extern GLXCopySubBufferProc     copySubBuffer;
    extern GLXGetVideoSyncProc      getVideoSync;
    extern GLXWaitVideoSyncProc     waitVideoSync;
    extern GLXGetFBConfigsProc      getFBConfigs;
    extern GLXGetFBConfigAttribProc getFBConfigAttrib;
    extern GLXCreatePixmapProc      createPixmap;
    extern GLXDestroyPixmapProc     destroyPixmap;

    extern GLActiveTextureProc       activeTexture;
    extern GLClientActiveTextureProc clientActiveTexture;
    extern GLMultiTexCoord2fProc     multiTexCoord2f;

    extern GLGenProgramsProc        genPrograms;
    extern GLDeleteProgramsProc     deletePrograms;
    extern GLBindProgramProc        bindProgram;
    extern GLProgramStringProc      programString;
    extern GLProgramParameter4fProc programEnvParameter4f;
    extern GLProgramParameter4fProc programLocalParameter4f;
    extern GLGetProgramivProc       getProgramiv;

    extern GLGenFramebuffersProc        genFramebuffers;
    extern GLDeleteFramebuffersProc     deleteFramebuffers;
    extern GLBindFramebufferProc        bindFramebuffer;
    extern GLCheckFramebufferStatusProc checkFramebufferStatus;
    extern GLFramebufferTexture2DProc   framebufferTexture2D;
    extern GLGenerateMipmapProc         generateMipmap;

    extern bool  textureFromPixmap;
    extern bool  textureRectangle;
    extern bool  textureNonPowerOfTwo;
    extern bool  textureEnvCombine;
    extern bool  textureEnvCrossbar;
    extern bool  textureBorderClamp;
    extern bool  textureCompression;
    extern GLint maxTextureSize;
    extern bool  fbo;
    extern bool  fragmentProgram;
    extern GLint maxTextureUnits;

    extern bool canDoSaturated;
    extern bool canDoSlightlySaturated;
};

struct GLScreenPaintAttrib {
    GLfloat xRotate;
    GLfloat yRotate;
    GLfloat vRotate;
    GLfloat xTranslate;
    GLfloat yTranslate;
    GLfloat zTranslate;
    GLfloat zCamera;
};

#define MAX_DEPTH 32

struct GLFBConfig {
    GLXFBConfig fbConfig;
    int         yInverted;
    int         mipmap;
    int         textureFormat;
    int         textureTargets;
};

#define NOTHING_TRANS_FILTER 0
#define SCREEN_TRANS_FILTER  1
#define WINDOW_TRANS_FILTER  2


extern GLScreenPaintAttrib defaultScreenPaintAttrib;

class GLScreen;

class GLScreenInterface :
    public WrapableInterface<GLScreen, GLScreenInterface>
{
    public:

	/**
	 * Hookable function used for plugins to use openGL to draw on an output
	 *
	 * @param attrib Describes some basic drawing attribs for the screen
	 * including translation, rotation and scale
	 * @param matrix Describes a 4x4 3D modelview matrix for which this
	 * screen should be drawn in
	 * @param region Describes the region of the screen being redrawn
	 * @param output Describes the output being redrawn
	 * @param mask   Bitmask which describes how the screen is being redrawn'
	 */
	virtual bool glPaintOutput (const GLScreenPaintAttrib &attrib,
				    const GLMatrix 	      &matrix,
				    const CompRegion 	      &region,
				    CompOutput 		      *output,
				    unsigned int	      mask);


	/**
	 * Hookable function used for plugins to use openGL to draw on an output
	 * when the screen is transformed
	 *
	 * There is little difference between this and glPaintOutput, however
	 * this will be called when the entire screen is being transformed
	 * (eg cube)
	 *
	 * @param attrib Describes some basic drawing attribs for the screen
	 * including translation, rotation and scale
	 * @param matrix Describes a 4x4 3D modelview matrix for which this
	 * screen should be drawn in
	 * @param region Describes the region of the screen being redrawn
	 * @param output Describes the output being redrawn
	 * @param mask   Bitmask which describes how the screen is being redrawn'
	 */
	virtual void glPaintTransformedOutput (const GLScreenPaintAttrib &attrib,
					       const GLMatrix 		 &matrix,
					       const CompRegion 	 &region,
					       CompOutput 		 *output,
					       unsigned int		 mask);

	/**
	 * Hookable function to apply elements from a GLScreenPaintAttrib
	 * to a GLMatrix in the context of a CompOutput
	 *
	 * @param attrib Describes the basic drawing attribs of a screen
	 * including translation, rotation and scale to be applies to a matrix
	 * @param output Describes the output in which these operations take
	 * place
	 * @param matrix Pointer to a matrix where transformations will
	 * be applied
	 */
	virtual void glApplyTransform (const GLScreenPaintAttrib &attrib,
				       CompOutput 		 *output,
				       GLMatrix 		 *mask);

	virtual void glEnableOutputClipping (const GLMatrix &,
					     const CompRegion &,
					     CompOutput *);
	virtual void glDisableOutputClipping ();

};


class GLScreen :
    public WrapableHandler<GLScreenInterface, 5>,
    public PluginClassHandler<GLScreen, CompScreen, COMPIZ_OPENGL_ABI>,
    public CompOption::Class
{
    public:
	GLScreen (CompScreen *s);
	~GLScreen ();

	CompOption::Vector & getOptions ();
        bool setOption (const CompString &name, CompOption::Value &value);

	/**
	 * Returns the current compiz-wide openGL texture filter
	 */
	GLenum textureFilter ();

	/**
	 * Sets a new compiz-wide openGL texture filter
	 */
	void setTextureFilter (GLenum);

	void clearTargetOutput (unsigned int mask);

	/**
	 * Gets the libGL address of a particular openGL functor
	 */
	GL::FuncPtr getProcAddress (const char *name);

	void updateBackground ();

	/**
	 * Returns the current compiz-wide texture filter
	 */
	GLTexture::Filter filter (int);

	/**
	 * Sets a new compiz-wide texture filter
	 */
	void setFilter (int, GLTexture::Filter);

	GLFragment::Storage * fragmentStorage ();

	/**
	 * Sets a new compiz-wid openGL texture environment mode
	 */
	void setTexEnvMode (GLenum mode);

	/**
	 * Turns lighting on and off
	 */

	void setLighting (bool lighting);

	/**
	 * Returns true if lighting is enabled
	 */
	bool lighting ();

	void clearOutput (CompOutput *output, unsigned int mask);

	void setDefaultViewport ();

	GLTexture::BindPixmapHandle registerBindPixmap (GLTexture::BindPixmapProc);
	void unregisterBindPixmap (GLTexture::BindPixmapHandle);

	GLFBConfig * glxPixmapFBConfig (unsigned int depth);

	/**
	 * Returns a default icon texture
	 */
	GLTexture *defaultIcon ();

	void resetRasterPos ();

	/**
	 * Returns a 4x4 const float array which
	 * represents the current projection matrix
	 */
	const float * projectionMatrix ();

	WRAPABLE_HND (0, GLScreenInterface, bool, glPaintOutput,
		      const GLScreenPaintAttrib &, const GLMatrix &,
		      const CompRegion &, CompOutput *, unsigned int);
	WRAPABLE_HND (1, GLScreenInterface, void, glPaintTransformedOutput,
		      const GLScreenPaintAttrib &,
		      const GLMatrix &, const CompRegion &, CompOutput *,
		      unsigned int);
	WRAPABLE_HND (2, GLScreenInterface, void, glApplyTransform,
		      const GLScreenPaintAttrib &, CompOutput *, GLMatrix *);

	WRAPABLE_HND (3, GLScreenInterface, void, glEnableOutputClipping,
		      const GLMatrix &, const CompRegion &, CompOutput *);
	WRAPABLE_HND (4, GLScreenInterface, void, glDisableOutputClipping);

	friend class GLTexture;

    private:
	PrivateGLScreen *priv;
};

struct GLWindowPaintAttrib {
    GLushort opacity;
    GLushort brightness;
    GLushort saturation;
    GLfloat  xScale;
    GLfloat  yScale;
    GLfloat  xTranslate;
    GLfloat  yTranslate;
};

class GLWindow;

class GLWindowInterface :
    public WrapableInterface<GLWindow, GLWindowInterface>
{
    public:

	/**
	 * Hookable function to paint a window on-screen
	 *
	 * @param attrib Describes basic drawing attribs of this window;
	 * opacity, brightness, saturation
	 * @param matrix A 4x4 matrix which describes the transformation of
	 * this window
	 * @param region Describes the region of the window being drawn
	 * @param mask   Bitmask which describes how this window is drawn
	 */
	virtual bool glPaint (const GLWindowPaintAttrib &attrib,
			      const GLMatrix 		&matrix,
			      const CompRegion 		&region,
			      unsigned int		mask);

	/**
	 * Hookable function to draw a window on-screen
	 *
	 * Unlike glPaint, when glDraw is called, the window is
	 * drawn immediately
	 *
	 * @param matrix A 4x4 matrix which describes the transformation of
	 * this window
	 * @param attrib A Fragment attrib which describes the texture
	 * modification state of this window
	 * @param region Describes which region will be drawn
	 * @param mask   Bitmask which describes how this window is drawn
	 */
	virtual bool glDraw (const GLMatrix 	&matrix,
			     GLFragment::Attrib &attrib,
			     const CompRegion 	&region,
			     unsigned int	mask);

	/**
	 * Hookable function to add points to a window
	 * texture geometry
	 *
	 * This function adds rects to a window's texture geometry
	 * and modifies their points by the values in the GLTexture::MatrixList
	 *
	 * It is used for texture transformation to set points
	 * for where the texture should be skewed
	 *
	 * @param matrices Describes the matrices by which the texture exists
	 * @param region
	 * @param clipRegion
	 * @param min
	 * @param max
	 */
	virtual void glAddGeometry (const GLTexture::MatrixList &matrices,
				    const CompRegion 		&region,
				    const CompRegion 		&clipRegion,
				    unsigned int		min = MAXSHORT,
				    unsigned int		max = MAXSHORT);
	virtual void glDrawTexture (GLTexture *texture, GLFragment::Attrib &,
				    unsigned int);
	virtual void glDrawGeometry ();
};

class GLWindow :
    public WrapableHandler<GLWindowInterface, 5>,
    public PluginClassHandler<GLWindow, CompWindow, COMPIZ_OPENGL_ABI>
{
    public:

	/**
	 * Class which describes the texture geometry and transformation points
	 * of a window
	 */
	class Geometry {
	    public:
		Geometry ();
		~Geometry ();

		void reset ();

		/**
		 * Set the number of vertices in the texture geometry
		 */
		bool moreVertices (int newSize);

		/**
		 * Set the number of indices in the texture geometry
		 */
		bool moreIndices (int newSize);

	    public:
		GLfloat  *vertices;
		int      vertexSize;
		int      vertexStride;
		GLushort *indices;
		int      indexSize;
		int      vCount;
		int      texUnits;
		int      texCoordSize;
		int      indexCount;
	};

	static GLWindowPaintAttrib defaultPaintAttrib;
    public:

	GLWindow (CompWindow *w);
	~GLWindow ();

	const CompRegion & clip () const;

	/**
	 * Returns the current paint attributes for this window
	 */
	GLWindowPaintAttrib & paintAttrib ();

	/**
	 * Returns the last paint attributes for this window
	 */
	GLWindowPaintAttrib & lastPaintAttrib ();

	unsigned int lastMask () const;

	/**
	 * Binds this window to an openGL texture
	 */
	bool bind ();

	/**
	 * Releases this window from an openGL texture
	 */
	void release ();

	/**
	 * Returns the tiled textures for this window
	 */
	const GLTexture::List &       textures () const;

	/**
	 * Returns the matrices for the tiled textures for this windwo
	 */
	const GLTexture::MatrixList & matrices () const;

	void updatePaintAttribs ();

	/**
	 * Returns the window texture geometry
	 */
	Geometry & geometry ();

	GLTexture *getIcon (int width, int height);

	WRAPABLE_HND (0, GLWindowInterface, bool, glPaint,
		      const GLWindowPaintAttrib &, const GLMatrix &,
		      const CompRegion &, unsigned int);
	WRAPABLE_HND (1, GLWindowInterface, bool, glDraw, const GLMatrix &,
		      GLFragment::Attrib &, const CompRegion &, unsigned int);
	WRAPABLE_HND (2, GLWindowInterface, void, glAddGeometry,
		      const GLTexture::MatrixList &, const CompRegion &,
		      const CompRegion &,
		      unsigned int = MAXSHORT, unsigned int = MAXSHORT);
	WRAPABLE_HND (3, GLWindowInterface, void, glDrawTexture,
		      GLTexture *texture, GLFragment::Attrib &, unsigned int);
	WRAPABLE_HND (4, GLWindowInterface, void, glDrawGeometry);

	friend class GLScreen;
	friend class PrivateGLScreen;

    private:
	PrivateGLWindow *priv;
};

#endif
