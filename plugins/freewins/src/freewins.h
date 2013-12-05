/**
 * Compiz Fusion Freewins plugin
 *
 * freewins.h
 *
 * Copyright (C) 2007  Rodolfo Granata <warlock.cc@gmail.com>
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
 * Author(s):
 * Rodolfo Granata <warlock.cc@gmail.com>
 *
 * Button binding support and Reset added by:
 * enigma_0Z <enigma.0ZA@gmail.com>
 *
 * Scaling, Animation, Input-Shaping, Snapping
 * and Key-Based Transformation added by:
 * Sam Spilsbury <smspillaz@gmail.com>
 *
 * Most of the input handling here is based on
 * the shelf plugin by
 *        : Kristian Lyngstøl <kristian@bohemians.org>
 *        : Danny Baumann <maniac@opencompositing.org>
 *
 * Description:
 *
 * This plugin allows you to freely transform the texture of a window,
 * whether that be rotation or scaling to make better use of screen space
 * or just as a toy.
 *
 * Todo:
 *  - Fully implement an input redirection system by
 *    finding an inverse matrix, multiplying by it,
 *    translating to the actual window co-ords and
 *    XSendEvent() the co-ords to the actual window.
 *  - Code could be cleaner
 *  - Add timestep and speed options to animation
 *  - Add window hover-over info via paintOutput : i.e
 *    - Resize borders
 *    - 'Reset' Button
 *    - 'Scale' Button
 *    - 'Rotate' Button
 */

#include <core/core.h>
#include <core/pluginclasshandler.h>
#include <composite/composite.h>
#include <opengl/opengl.h>
#include <core/atoms.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>

#include <GL/glu.h>
#include <GL/gl.h>

#include "freewins_options.h"

/* #define ABS(x) ((x)>0?(x):-(x)) */
#define D2R(x) ((x) * (M_PI / 180.0))
#define R2D(x) ((x) * (180 / M_PI))

/* ------ Macros ---------------------------------------------------------*/

#define WIN_OUTPUT_X(w) (w->x () - w->output ().left)
#define WIN_OUTPUT_Y(w) (w->y () - w->output ().top)

#define WIN_OUTPUT_W(w) (w->width () + w->output ().left + w->output ().right)
#define WIN_OUTPUT_H(w) (w->height () + w->output ().top + w->output ().bottom)

#define WIN_REAL_X(w) (w->x () - w->border ().left)
#define WIN_REAL_Y(w) (w->y () - w->border ().top)

#define WIN_REAL_W(w) (w->width () + w->border ().left + w->border ().right)
#define WIN_REAL_H(w) (w->height () + w->border ().top + w->border ().bottom)

#define WIN_CORNER1(w) GLVector ic1 = GLVector (WIN_REAL_X (w), WIN_REAL_Y (w), 0.0f, 1.0f);
#define WIN_CORNER2(w) GLVector ic2 = GLVector (WIN_REAL_X (w) + WIN_REAL_W (w), WIN_REAL_Y (w), 0.0f, 1.0f);
#define WIN_CORNER3(w) GLVector ic3 = GLVector (WIN_REAL_X (w), WIN_REAL_Y (w) + WIN_REAL_H (w), 0.0f, 1.0f);
#define WIN_CORNER4(w) GLVector ic4 = GLVector (WIN_REAL_X (w) + WIN_REAL_W (w), WIN_REAL_Y (w) + WIN_REAL_H (w), 0.0f, 1.0f);

#define WIN_OCORNER1(w) GLVector oc1 = GLVector (WIN_OUTPUT_X (w), WIN_OUTPUT_Y (w), 0.0f, 1.0f);
#define WIN_OCORNER2(w) GLVector oc2 = GLVector (WIN_OUTPUT_X (w) + WIN_OUTPUT_W (w), WIN_OUTPUT_Y (w), 0.0f, 1.0f);
#define WIN_OCORNER3(w) GLVector oc3 = GLVector (WIN_OUTPUT_X (w), WIN_OUTPUT_Y (w) + WIN_OUTPUT_H (w), 0.0f, 1.0f);
#define WIN_OCORNER4(w) GLVector oc4 = GLVector ( WIN_OUTPUT_X (w) + WIN_OUTPUT_W (w), WIN_OUTPUT_Y (w) + WIN_OUTPUT_H (w), 0.0f, 1.0f);

/* ------ Structures and Enums ------------------------------------------*/

/* Enums */
typedef enum _StartCorner
{
    CornerTopLeft = 0,
    CornerTopRight = 1,
    CornerBottomLeft = 2,
    CornerBottomRight = 3
} StartCorner;

typedef enum _FWGrabType
{
    grabNone = 0,
    grabRotate,
    grabScale,
    grabMove,
    grabResize
} FWGrabType;

typedef enum _Direction
{
    UpDown = 0,
    LeftRight = 1
} Direction;

typedef enum _FWAxisType
{
    axisX = 0,
    axisY,
    axisZ,
    axisXY,
    axisXZ,
} FWAxisType;

/* Shape info / restoration */
class FWWindowInputInfo
{
    public:

	FWWindowInputInfo (CompWindow *);
	~FWWindowInputInfo ();

    public:
	CompWindow                *w;

	Window     ipw;

	XRectangle *inputRects;
	int        nInputRects;
	int        inputRectOrdering;

	XRectangle *frameInputRects;
	int        frameNInputRects;
	int        frameInputRectOrdering;
};

class FWWindowOutputInfo
{
    public:
	float shapex1;
	float shapex2;
	float shapex3;
	float shapex4;
	float shapey1;
	float shapey2;
	float shapey3;
	float shapey4;
}; 

/* Trackball */

/*typedef struct _FWTrackball
{
    CompVector mouseX;
    CompVector mouse0;
    CompVector tr_axis;
    float tr_ang;
    float tr_radius;

} FWTrackball;*/

/* Transformation info */
class FWTransformedWindowInfo
{
    public:
	FWTransformedWindowInfo () :
	    angX (0),
	    angY (0),
	    angZ (0),
	    scaleX (1.0f),
	    scaleY (1.0f),
	    unsnapAngX (0),
	    unsnapAngY (0),
	    unsnapAngZ (0),
	    unsnapScaleX (0),
	    unsnapScaleY (0) {}

	//FWTrackball trackball;

	float angX;
	float angY;
	float angZ;

	float scaleX;
	float scaleY;

	// Window transformation

	/* Used for snapping */

	float unsnapAngX;
	float unsnapAngY;
	float unsnapAngZ;

	float unsnapScaleX;
	float unsnapScaleY;
};

class FWAnimationInfo
{
    public:

	FWAnimationInfo () :
	    oldAngX (0),
	    oldAngY (0),
	    oldAngZ (0),
	    oldScaleX (1.0f),
	    oldScaleY (1.0f),
	    destAngX (0),
	    destAngY (0),
	    destAngZ (0),
	    destScaleX (1.0f),
	    destScaleY (1.0f),
	    steps (0) {}

	// Old values to animate from
	float oldAngX;
	float oldAngY;
	float oldAngZ;

	float oldScaleX;
	float oldScaleY;

	// New values to animate to
	float destAngX;
	float destAngY;
	float destAngZ;

	float destScaleX;
	float destScaleY;

	// For animation
	float steps;
};

class FWScreen :
	public PluginClassHandler <FWScreen, CompScreen>,
	public ScreenInterface,
	public CompositeScreenInterface,
	public GLScreenInterface,
	public FreewinsOptions
{
    public:

	FWScreen (CompScreen *screen);

	CompositeScreen *cScreen;
	GLScreen	*gScreen;

	std::list <FWWindowInputInfo *> mTransformedWindows;

	int mClick_root_x;
	int mClick_root_y;

	CompWindow *mGrabWindow;
	CompWindow *mHoverWindow;
	CompWindow *mLastGrabWindow;

	bool mAxisHelp;
	bool mSnap;
	bool mInvert;
	int  mSnapMask;
	int  mInvertMask;

	Cursor mRotateCursor;

	CompScreen::GrabHandle mGrabIndex;

	void preparePaint (int);
	bool glPaintOutput (const GLScreenPaintAttrib &,
			    const GLMatrix	      &,
			    const CompRegion	      &,
			    CompOutput		      *,
			    unsigned int		);
	void donePaint ();

	void handleEvent (XEvent *);

	void
	optionChanged (CompOption *option,
		       FreewinsOptions::Options num);

	void
	reloadSnapKeys ();

	bool
	initiateFWRotate (CompAction         *action,
			  CompAction::State  state,
			  CompOption::Vector options);

	bool
	terminateFWRotate (CompAction          *action,
			   CompAction::State   state,
			   CompOption::Vector  options);

	bool
	initiateFWScale (CompAction         *action,
			 CompAction::State  state,
			 CompOption::Vector options);

	bool
	terminateFWScale (CompAction         *action,
			  CompAction::State  state,
			  CompOption::Vector options);

	bool
	rotate (CompAction         *action,
		CompAction::State  state,
		CompOption::Vector options, int dx, int dy, int dz);

	bool
	scale (CompAction          *action,
	       CompAction::State   state,
	       CompOption::Vector  options,
	       int		     scale);
	bool
	resetFWTransform (CompAction         *action,
			  CompAction::State  state,
			  CompOption::Vector options);

	bool
	rotateAction (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options);

	bool
	incrementRotateAction (CompAction         *action,
			       CompAction::State  state,
			       CompOption::Vector options);

	bool
	scaleAction (CompAction         *action,
		     CompAction::State  state,
		     CompOption::Vector options);

	bool
	toggleFWAxis (CompAction         *action,
		      CompAction::State  state,
		      CompOption::Vector options);

	void
	addWindowToList (FWWindowInputInfo *info);

	void
	removeWindowFromList (FWWindowInputInfo *info);

	void
	adjustIPWStacking ();

	void
	rotateProjectVector (GLVector &vector,
			     GLMatrix &transform,
			     GLdouble *resultX,
			     GLdouble *resultY,
			     GLdouble *resultZ);

	void
	perspectiveDistortAndResetZ (GLMatrix &transform);

	void
	modifyMatrix  (GLMatrix &transform,
		       float angX, float angY, float angZ,
		       float tX, float tY, float tZ,
		       float scX, float scY, float scZ,
		       float adjustX, float adjustY, bool paint);

	CompRect
	createSizedRect (float xScreen1,
			 float xScreen2,
			 float xScreen3,
			 float xScreen4,
			 float yScreen1,
			 float yScreen2,
			 float yScreen3,
			 float yScreen4);

	CompWindow *
	getRealWindow (CompWindow *w);
};

/* Freewins Window Structure */
class FWWindow :
	public PluginClassHandler <FWWindow, CompWindow>,
	public WindowInterface,
	public CompositeWindowInterface,
	public GLWindowInterface
{
    public:

	FWWindow (CompWindow *w);
	~FWWindow ();

	CompWindow *window;
	CompositeWindow *cWindow;
	GLWindow	*gWindow;

	bool glPaint (const GLWindowPaintAttrib &,
		      const GLMatrix		&,
		      const CompRegion		&,
		      unsigned int		  );

	bool damageRect (bool initial,
			 const CompRect &);

	void moveNotify (int, int, bool);
	void resizeNotify (int, int, int, int);

	float mIMidX;
	float mIMidY;

	float mOMidX; /* These will be removed */
	float mOMidY;

	float mAdjustX;
	float mAdjustY;

	float mRadius;

	// Used for determining window movement

	int mOldWinX;
	int mOldWinY;

	// Used for resize

	int mWinH;
	int mWinW;

	Direction mDirection;

	// Used to determine starting point
	StartCorner mCorner;

	// Transformation info
	FWTransformedWindowInfo mTransform;

	// Animation Info
	FWAnimationInfo mAnimate;

	// Input Info
	FWWindowInputInfo *mInput;

	//Output Info
	FWWindowOutputInfo mOutput;

	CompRect mOutputRect;
	CompRect mInputRect;

	// Used to determine whether to animate the window
	bool mResetting;
	bool mIsAnimating;

	// Used to determine whether rotating on X and Y axis, or just on Z
	bool mCan2D; // These need to be removed
	bool mCan3D;

	bool mTransformed; // So does this in favor of FWWindowInputInfo

	FWGrabType mGrab;

	void
	shapeIPW ();

	void
	saveInputShape (XRectangle **retRects,
			int       *retCount,
			int       *retOrdering);

	void
	adjustIPW ();

	void
	createIPW ();

	bool
	handleWindowInputInfo ();

	void
	shapeInput ();

	void
	unshapeInput ();

	void
	handleIPWResizeInitiate ();

	void
	handleIPWMoveInitiate ();

	void
	handleIPWMoveMotionEvent (unsigned int x,
				  unsigned int y);

	void
	handleIPWResizeMotionEvent (unsigned int x,
				    unsigned int y);

	void
	handleRotateMotionEvent (float dx,
				 float dy,
				 int x,
				 int y);

	void
	handleScaleMotionEvent (float dx,
				float dy,
				int x,
				int y);

	void
	handleButtonReleaseEvent ();

	void
	handleEnterNotify (XEvent *xev);

	void
	handleLeaveNotify (XEvent *xev);

	void
	damageArea ();

	void
	setPrepareRotation (float dx,
			    float dy,
			    float dz,
			    float dsu,
			    float dsd);

	void
	calculateInputOrigin (float x, float y);

	void
	calculateOutputOrigin (float x, float y);

	void
	calculateOutputRect ();

	void
	calculateInputRect ();

	CompRect
	calculateWindowRect (GLVector c1,
			     GLVector c2,
			     GLVector c3,
			     GLVector c4);

	void
	determineZAxisClick (int px,
			     int py,
			     bool motion);
	bool
	canShape ();

	void
	handleSnap ();
};

#define FREEWINS_SCREEN(screen)						       \
    FWScreen *fws = FWScreen::get (screen);

#define FREEWINS_WINDOW(window)						       \
    FWWindow *fww = FWWindow::get (window);

class FWPluginVTable :
	public CompPlugin::VTableForScreenAndWindow <FWScreen, FWWindow>
{
    public:

	bool init ();
};
