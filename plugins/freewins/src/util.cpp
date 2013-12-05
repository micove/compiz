/**
 * Compiz Fusion Freewins plugin
 *
 * util.cpp
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
 * Sam Spilsbury <smspillaz@gmail.com>
 *
 * Button binding support and Reset added by:
 * enigma_0Z <enigma.0ZA@gmail.com>
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

#include "freewins.h"


/* ------ Utility Functions ---------------------------------------------*/

/* Rotate and project individual vectors */
void
FWScreen::rotateProjectVector (GLVector &vector,
			       GLMatrix &transform,
			       GLdouble *resultX,
			       GLdouble *resultY,
			       GLdouble *resultZ)
{
    vector = transform * vector;

    GLint viewport[4];		// Viewport
    GLdouble modelview[16];	// Modelview Matrix
    GLdouble projection[16];	// Projection Matrix

    glGetIntegerv (GL_VIEWPORT, viewport);
    glGetDoublev (GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev (GL_PROJECTION_MATRIX, projection);

    gluProject (vector[GLVector::x], vector[GLVector::y], vector[GLVector::z],
		modelview, projection, viewport,
		resultX, resultY, resultZ);

    /* Y must be negated */
    *resultY = screen->height () - *resultY;
}

/* Scales z by 0 and does perspective distortion so that it
 * looks the same wherever on the screen
 *
 * This code taken from animation.c,
 * Copyright (c) 2006 Erkin Bahceci
 */
void
FWScreen::perspectiveDistortAndResetZ (GLMatrix &transform)
{
    float v = -1.0 / ::screen->width ();
    /*
      This does
      transform = M * transform, where M is
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 0, v,
      0, 0, 0, 1
    */

    transform[8] = v * transform[12];
    transform[9] = v * transform[13];
    transform[10] = v * transform[14];
    transform[11] = v * transform[15];
}

void
FWScreen::modifyMatrix  (GLMatrix &transform,
			 float angX, float angY, float angZ,
			 float tX, float tY, float tZ,
			 float scX, float scY, float scZ,
			 float adjustX, float adjustY, bool paint)
{
    /* Create our transformation Matrix */
    transform.translate (tX, tY, 0.0);
    if (paint)
	perspectiveDistortAndResetZ (transform);
    else
	transform.scale (1.0f, 1.0f, 1.0f / screen->width ());
    transform.rotate (angX, 1.0f, 0.0f, 0.0f);
    transform.rotate (angY, 0.0f, 1.0f, 0.0f);
    transform.rotate (angZ, 0.0f, 0.0f, 1.0f);
    transform.scale (scX, 1.0f, 0.0f);
    transform.scale (1.0f, scY, 0.0f);
    transform.translate (-(tX), -(tY), 0.0f);
}

/*
static float det3(float m00, float m01, float m02,
		 float m10, float m11, float m12,
		 float m20, float m21, float m22)
{
    float ret = 0.0;

    ret += m00 * m11 * m22 - m21 * m12 * m00;
    ret += m01 * m12 * m20 - m22 * m10 * m01;
    ret += m02 * m10 * m21 - m20 * m11 * m02;

    return ret;
}

static void FWFindInverseMatrix(CompTransform *m, CompTransform *r)
{
    float *mm = m->m;
    float d, c[16];

    d = mm[0] * det3(mm[5], mm[6], mm[7],
			mm[9], mm[10], mm[11],
			 mm[13], mm[14], mm[15]) -

	  mm[1] * det3(mm[4], mm[6], mm[7],
			mm[8], mm[10], mm[11],
			 mm[12], mm[14], mm[15]) +

	  mm[2] * det3(mm[4], mm[5], mm[7],
			mm[8], mm[9], mm[11],
			 mm[12], mm[13], mm[15]) -

	  mm[3] * det3(mm[4], mm[5], mm[6],
			mm[8], mm[9], mm[10],
			 mm[12], mm[13], mm[14]);

    c[0] = det3(mm[5], mm[6], mm[7],
		  mm[9], mm[10], mm[11],
		  mm[13], mm[14], mm[15]);
    c[1] = -det3(mm[4], mm[6], mm[7],
		  mm[8], mm[10], mm[11],
		  mm[12], mm[14], mm[15]);
    c[2] = det3(mm[4], mm[5], mm[7],
		  mm[8], mm[9], mm[11],
		  mm[12], mm[13], mm[15]);
    c[3] = -det3(mm[4], mm[5], mm[6],
		  mm[8], mm[9], mm[10],
		  mm[12], mm[13], mm[14]);

    c[4] = -det3(mm[1], mm[2], mm[3],
		  mm[9], mm[10], mm[11],
		  mm[13], mm[14], mm[15]);
    c[5] = det3(mm[0], mm[2], mm[3],
		  mm[8], mm[10], mm[11],
		  mm[12], mm[14], mm[15]);
    c[6] = -det3(mm[0], mm[1], mm[3],
		  mm[8], mm[9], mm[11],
		  mm[12], mm[13], mm[15]);
    c[7] = det3(mm[0], mm[1], mm[2],
		  mm[8], mm[9], mm[10],
		  mm[12], mm[13], mm[14]);

    c[8] = det3(mm[1], mm[2], mm[3],
		  mm[5], mm[6], mm[7],
		  mm[13], mm[14], mm[15]);
    c[9] = -det3(mm[0], mm[2], mm[3],
		  mm[4], mm[6], mm[7],
		  mm[12], mm[14], mm[15]);
    c[10] = det3(mm[0], mm[1], mm[3],
		  mm[4], mm[5], mm[7],
		  mm[12], mm[13], mm[15]);
    c[11] = -det3(mm[0], mm[1], mm[2],
		  mm[4], mm[5], mm[6],
		  mm[12], mm[13], mm[14]);

    c[12] = -det3(mm[1], mm[2], mm[3],
		  mm[5], mm[6], mm[7],
		  mm[9], mm[10], mm[11]);
    c[13] = det3(mm[0], mm[2], mm[3],
		  mm[4], mm[6], mm[7],
		  mm[8], mm[10], mm[11]);
    c[14] = -det3(mm[0], mm[1], mm[3],
		  mm[4], mm[5], mm[7],
		  mm[8], mm[9], mm[11]);
    c[15] = det3(mm[0], mm[1], mm[2],
		  mm[4], mm[5], mm[6],
		  mm[8], mm[9], mm[10]);

    r->m[0] = c[0] / d;
    r->m[1] = c[4] / d;
    r->m[2] = c[8] / d;
    r->m[3] = c[12] / d;

    r->m[4] = c[1] / d;
    r->m[5] = c[5] / d;
    r->m[6] = c[9] / d;
    r->m[7] = c[13] / d;

    r->m[8] = c[2] / d;
    r->m[9] = c[6] / d;
    r->m[10] = c[10] / d;
    r->m[11] = c[14] / d;

    r->m[12] = c[3] / d;
    r->m[13] = c[7] / d;
    r->m[14] = c[11] / d;
    r->m[15] = c[15] / d;

    return;
}
*/

/* Create a rect from 4 screen points */
CompRect
FWScreen::createSizedRect (float xScreen1,
			   float xScreen2,
			   float xScreen3,
			   float xScreen4,
			   float yScreen1,
			   float yScreen2,
			   float yScreen3,
			   float yScreen4)
{
    /* Left most point */
    float leftmost = xScreen1;

    if (xScreen2 <= leftmost)
	leftmost = xScreen2;

    if (xScreen3 <= leftmost)
	leftmost = xScreen3;

    if (xScreen4 <= leftmost)
	leftmost = xScreen4;

    /* Right most point */
    float rightmost = xScreen1;

    if (xScreen2 >= rightmost)
	rightmost = xScreen2;

    if (xScreen3 >= rightmost)
	rightmost = xScreen3;

    if (xScreen4 >= rightmost)
	rightmost = xScreen4;

    /* Top most point */
    float topmost = yScreen1;

    if (yScreen2 <= topmost)
	topmost = yScreen2;

    if (yScreen3 <= topmost)
	topmost = yScreen3;

    if (yScreen4 <= topmost)
	topmost = yScreen4;

    /* Bottom most point */
    float bottommost = yScreen1;

    if (yScreen2 >= bottommost)
	bottommost = yScreen2;

    if (yScreen3 >= bottommost)
	bottommost = yScreen3;

    if (yScreen4 >= bottommost)
	bottommost = yScreen4;
/*
    Box rect;
    rect.x1 = leftmost;
    rect.x2 = rightmost;
    rect.y1 = topmost;
    rect.y2 = bottommost;
*/
    return CompRect (leftmost, topmost, rightmost - leftmost, bottommost - topmost);
}

CompRect
FWWindow::calculateWindowRect (GLVector c1,
			       GLVector c2,
			       GLVector c3,
			       GLVector c4)
{
    FREEWINS_SCREEN (screen);

    GLMatrix transform;
    GLdouble xScreen1 = 0.0f, yScreen1 = 0.0f, zScreen1 = 0.0f;
    GLdouble xScreen2 = 0.0f, yScreen2 = 0.0f, zScreen2 = 0.0f;
    GLdouble xScreen3 = 0.0f, yScreen3 = 0.0f, zScreen3 = 0.0f;
    GLdouble xScreen4 = 0.0f, yScreen4 = 0.0f, zScreen4 = 0.0f;

    transform.reset ();
    fws->modifyMatrix (transform,
		       mTransform.angX,
		       mTransform.angY,
		       mTransform.angZ,
		       mIMidX, mIMidY, 0.0f,
		       mTransform.scaleX,
		       mTransform.scaleY, 0.0f, 0.0f, 0.0f, false);

    fws->rotateProjectVector(c1, transform, &xScreen1, &yScreen1, &zScreen1);
    fws->rotateProjectVector(c2, transform, &xScreen2, &yScreen2, &zScreen2);
    fws->rotateProjectVector(c3, transform, &xScreen3, &yScreen3, &zScreen3);
    fws->rotateProjectVector(c4, transform, &xScreen4, &yScreen4, &zScreen4);

    /* Save the non-rectangular points so that we can shape the rectangular IPW */

    mOutput.shapex1 = xScreen1;
    mOutput.shapex2 = xScreen2;
    mOutput.shapex3 = xScreen3;
    mOutput.shapex4 = xScreen4;
    mOutput.shapey1 = yScreen1;
    mOutput.shapey2 = yScreen2;
    mOutput.shapey3 = yScreen3;
    mOutput.shapey4 = yScreen4;


    return fws->createSizedRect(xScreen1, xScreen2, xScreen3, xScreen4,
				yScreen1, yScreen2, yScreen3, yScreen4);
}

void
FWWindow::calculateOutputRect ()
{
    GLVector corner1 = GLVector (WIN_OUTPUT_X (window), WIN_OUTPUT_Y (window), 1.0f, 1.0f);
    GLVector corner2 = GLVector (WIN_OUTPUT_X (window) + WIN_OUTPUT_W (window), WIN_OUTPUT_Y (window), 1.0f, 1.0f);
    GLVector corner3 = GLVector (WIN_OUTPUT_X (window), WIN_OUTPUT_Y (window) + WIN_OUTPUT_H (window), 1.0f, 1.0f);
    GLVector corner4 = GLVector (WIN_OUTPUT_X (window) + WIN_OUTPUT_W (window), WIN_OUTPUT_Y (window) + WIN_OUTPUT_H (window), 1.0f, 1.0f);

    mOutputRect = calculateWindowRect (corner1, corner2, corner3, corner4);
}

void
FWWindow::calculateInputRect ()
{
    GLVector corner1 = GLVector (WIN_REAL_X (window), WIN_REAL_Y (window), 1.0f, 1.0f);
    GLVector corner2 = GLVector (WIN_REAL_X (window) + WIN_REAL_W (window), WIN_REAL_Y (window), 1.0f, 1.0f);
    GLVector corner3 = GLVector (WIN_REAL_X (window), WIN_REAL_Y (window) + WIN_REAL_H (window), 1.0f, 1.0f);
    GLVector corner4 = GLVector (WIN_REAL_X (window) + WIN_REAL_W (window), WIN_REAL_Y (window) + WIN_REAL_H (window), 1.0f, 1.0f);

    mInputRect = calculateWindowRect (corner1, corner2, corner3, corner4);
}

void
FWWindow::calculateInputOrigin (float x, float y)
{
    mIMidX = x;
    mIMidY = y;
}

void
FWWindow::calculateOutputOrigin (float x, float y)
{
    float dx, dy;

    dx = x - WIN_OUTPUT_X (window);
    dy = y - WIN_OUTPUT_Y (window);

    mOMidX = WIN_OUTPUT_X (window) + dx * mTransform.scaleX;
    mOMidY = WIN_OUTPUT_Y (window) + dy * mTransform.scaleY;
}

/* Change angles more than 360 into angles out of 360 */
/*static int FWMakeIntoOutOfThreeSixty (int value)
{
    while (value > 0)
    {
	value -= 360;
    }

    if (value < 0)
	value += 360;

    return value;
}*/

/* Determine if we clicked in the z-axis region */
void
FWWindow::determineZAxisClick (int px,
			       int py,
			       bool motion)
{
    bool directionChange = FALSE;

    if (!mCan2D && motion)
    {

	static int steps;

	/* Check if we are going in a particular 3D direction
	 * because if we are going left/right and we suddenly
	 * change to 2D mode this would not be expected behaviour.
	 * It is only if we have a change in direction that we want
	 * to change to 2D rotation.
	 */

	Direction direction, oldDirection = LeftRight;

	static int ddx, ddy;

	unsigned int dx = pointerX - lastPointerX;
	unsigned int dy = pointerY - lastPointerY;

	ddx += dx;
	ddy += dy;

	if (steps >= 10)
	{
	    if (ddx > ddy)
		direction = LeftRight;
	    else
		direction = UpDown;

	    if (direction != oldDirection)
		directionChange = TRUE;

	    direction = oldDirection;
	}

	steps++;

    }
    else
	directionChange = TRUE;

    if (directionChange)
    {
	float clickRadiusFromCenter;

	int x = (WIN_REAL_X(window) + WIN_REAL_W(window)/2.0);
	int y = (WIN_REAL_Y(window) + WIN_REAL_H(window)/2.0);

	clickRadiusFromCenter = sqrt(pow((x - px), 2) + pow((y - py), 2));

	if (clickRadiusFromCenter > mRadius * (FWScreen::get (screen)->optionGetTdPercent () / 100))
	{
	    mCan2D = TRUE;
	    mCan3D = FALSE;
	}
	else
	{
	    mCan2D = FALSE;
	    mCan3D = TRUE;
	}
    }
}

/* Check to see if we can shape a window */
bool
FWWindow::canShape ()
{
    FREEWINS_SCREEN (screen);

    if (!fws->optionGetDoShapeInput ())
	return FALSE;
    
    if (!screen->XShape ())
	return FALSE;
    
    if (!fws->optionGetShapeWindowTypes ().evaluate (window))
	return FALSE;
    
    return TRUE;
}

/* Checks if w is a ipw and returns the real window */
CompWindow *
FWScreen::getRealWindow (CompWindow *w)
{
    FWWindowInputInfo *info;

    foreach (info, mTransformedWindows)
    {
	if (w->id () == info->ipw)
	    return info->w;
    }

    return NULL;
}

void
FWWindow::handleSnap ()
{
    FREEWINS_SCREEN (screen);
    
    /* Handle Snapping */
    if (fws->optionGetSnap () || fws->mSnap)
    {
	int snapFactor = fws->optionGetSnapThreshold ();
	mAnimate.destAngX = ((int) (mTransform.unsnapAngX) / snapFactor) * snapFactor;
	mAnimate.destAngY = ((int) (mTransform.unsnapAngY) / snapFactor) * snapFactor;
	mAnimate.destAngZ = ((int) (mTransform.unsnapAngZ) / snapFactor) * snapFactor;
	mTransform.scaleX =
		((float) ( (int) (mTransform.unsnapScaleX * (21 - snapFactor) + 0.5))) / (21 - snapFactor);
	mTransform.scaleY =
		((float) ( (int) (mTransform.unsnapScaleY * (21 - snapFactor) + 0.5))) / (21 - snapFactor);
    }
}
