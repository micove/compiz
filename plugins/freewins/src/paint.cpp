/**
 * Compiz Fusion Freewins plugin
 *
 * paint.cpp
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


/* ------ Window and Output Painting ------------------------------------*/

/* Damage util function */

void
FWWindow::damageArea ()
{
    CompositeScreen::get (screen)->damageRegion (mOutputRect);
}

/* Animation Prep */
void
FWScreen::preparePaint (int	      ms)
{
    /* FIXME: should only loop over all windows if at least one animation is running */
    foreach (CompWindow *w, screen->windows ())
    {
	FREEWINS_WINDOW (w);
	float speed = optionGetSpeed ();
	fww->mAnimate.steps = ((float) ms / ((20.1 - speed) * 100));

	if (fww->mAnimate.steps < 0.005)
	    fww->mAnimate.steps = 0.005;

	/* Animation. We calculate how much increment
	 * a window must rotate / scale per paint by
	 * using the set destination attributes minus
	 * the old attributes divided by the time
	 * remaining.
	 */

	/* Don't animate if the window is saved */
	fww->mTransform.angX += (float) fww->mAnimate.steps * (fww->mAnimate.destAngX - fww->mTransform.angX) * speed;
	fww->mTransform.angY += (float) fww->mAnimate.steps * (fww->mAnimate.destAngY - fww->mTransform.angY) * speed;
	fww->mTransform.angZ += (float) fww->mAnimate.steps * (fww->mAnimate.destAngZ - fww->mTransform.angZ) * speed;

	fww->mTransform.scaleX += (float) fww->mAnimate.steps * (fww->mAnimate.destScaleX - fww->mTransform.scaleX) * speed;
	fww->mTransform.scaleY += (float) fww->mAnimate.steps * (fww->mAnimate.destScaleY - fww->mTransform.scaleY) * speed;

	if (((fww->mTransform.angX >= fww->mAnimate.destAngX - 0.05 &&
	      fww->mTransform.angX <= fww->mAnimate.destAngX + 0.05 ) &&
	     (fww->mTransform.angY >= fww->mAnimate.destAngY - 0.05 &&
	      fww->mTransform.angY <= fww->mAnimate.destAngY + 0.05 ) &&
	     (fww->mTransform.angZ >= fww->mAnimate.destAngZ - 0.05 &&
	      fww->mTransform.angZ <= fww->mAnimate.destAngZ + 0.05 ) &&
	     (fww->mTransform.scaleX >= fww->mAnimate.destScaleX - 0.00005 &&
	      fww->mTransform.scaleX <= fww->mAnimate.destScaleX + 0.00005 ) &&
	     (fww->mTransform.scaleY >= fww->mAnimate.destScaleY - 0.00005 &&
	      fww->mTransform.scaleY <= fww->mAnimate.destScaleY + 0.00005 )))
	{
	    fww->mResetting = FALSE;

	    fww->mTransform.angX = fww->mAnimate.destAngX;
	    fww->mTransform.angY = fww->mAnimate.destAngY;
	    fww->mTransform.angZ = fww->mAnimate.destAngZ;
	    fww->mTransform.scaleX = fww->mAnimate.destScaleX;
	    fww->mTransform.scaleY = fww->mAnimate.destScaleY;

	    fww->mTransform.unsnapAngX = fww->mAnimate.destAngX;
	    fww->mTransform.unsnapAngY = fww->mAnimate.destAngY;
	    fww->mTransform.unsnapAngZ = fww->mAnimate.destAngZ;
	    fww->mTransform.unsnapScaleX = fww->mAnimate.destScaleX;
	    fww->mTransform.unsnapScaleY = fww->mAnimate.destScaleX;
	}
	//else
	//    fww->damageArea ();
    }

    cScreen->preparePaint (ms);
}

/* Paint the window rotated or scaled */
bool
FWWindow::glPaint (const GLWindowPaintAttrib &attrib,
		   const GLMatrix	     &transform,
		   const CompRegion	     &region,
		   unsigned int		     mask)
{

    GLMatrix wTransform (transform);
    int currentCull, invertCull;
    glGetIntegerv (GL_CULL_FACE_MODE, &currentCull);
    invertCull = (currentCull == GL_BACK) ? GL_FRONT : GL_BACK;

    bool status;

    FREEWINS_SCREEN (screen);

    /* Has something happened? */
    
    /* Check to see if we are painting on a transformed screen */
    /* Enable this code when we can animate between the two states */

    if ((mTransform.angX != 0.0 ||
	 mTransform.angY != 0.0 ||
	 mTransform.angZ != 0.0 ||
	 mTransform.scaleX != 1.0 ||
	 mTransform.scaleY != 1.0 ||
	 mOldWinX != WIN_REAL_X (window) ||
	 mOldWinY != WIN_REAL_Y (window)) && fws->optionGetShapeWindowTypes ().evaluate (window))
    {
	mOldWinX = WIN_REAL_X (window);
	mOldWinY = WIN_REAL_Y (window);

	/* Figure out where our 'origin' is, don't allow the origin to
	 * be where we clicked if the window is not grabbed, etc
	 */

	/* Here we duplicate some of the work the openGL does
	 * but for different reasons. We have access to the
	 * window's transformation matrix, so we will create
	 * our own matrix and apply the same transformations
	 * to it. From there, we create vectors for each point
	 * that we wish to track and multiply them by this
	 * matrix to give us the rotated / scaled co-ordinates.
	 * From there, we project these co-ordinates onto the flat
	 * screen that we have using the OGL viewport, projection
	 * matrix and model matrix. Projection gives us three
	 * co-ordinates, but we ignore Z and just use X and Y
	 * to store in a surrounding rectangle. We can use this
	 * surrounding rectangle to make things like shaping and
	 * damage a lot more accurate than they used to be.
	 */

	calculateOutputRect ();

	/* Prepare for transformation by
	 * doing any necessary adjustments */
	float autoScaleX = 1.0f;
	float autoScaleY = 1.0f;

	if (fws->optionGetAutoZoom ())
	{
	    float apparantWidth = mOutputRect.width ();
	    float apparantHeight = mOutputRect.height ();

	    autoScaleX = (float) WIN_OUTPUT_W (window) / (float) apparantWidth;
	    autoScaleY = (float) WIN_OUTPUT_H (window) / (float) apparantHeight;

	    if (autoScaleX >= 1.0f)
		autoScaleX = 1.0f;
	    if (autoScaleY >= 1.0f)
		autoScaleY = 1.0f;

	    autoScaleX = autoScaleY = (autoScaleX + autoScaleY) / 2;

	    /* Because we modified the scale after calculating
	     * the output rect, we need to recalculate again */
	    calculateOutputRect ();

	}
	/*
	float scaleX = autoScaleX - (1 - mTransform.scaleX);
	float scaleY = autoScaleY - (1 - mTransform.scaleY);
	*/

	/* Actually Transform the window */
	mask |= PAINT_WINDOW_TRANSFORMED_MASK;

	/* Adjust the window in the matrix to prepare for transformation */
	if (mGrab != grabRotate && mGrab != grabScale)
	{

	    calculateInputOrigin (WIN_REAL_X (window) + WIN_REAL_W (window) / 2.0f,
				  WIN_REAL_Y (window) + WIN_REAL_H (window) / 2.0f);
	    calculateOutputOrigin (WIN_OUTPUT_X (window) + WIN_OUTPUT_W (window) / 2.0f,
				   WIN_OUTPUT_Y (window) + WIN_OUTPUT_H (window) / 2.0f);
	}

	float adjustX = 0.0f;
	float adjustY = 0.0f;
	fws->modifyMatrix (wTransform,
			   mTransform.angX,
			   mTransform.angY,
			   mTransform.angZ,
			   mIMidX, mIMidY , 0.0f,
			   mTransform.scaleX,
			   mTransform.scaleY,
			   1.0f, adjustX, adjustY, TRUE);

	/* Create rects for input after we've dealt with output */
	calculateInputRect ();

	/* Determine if the window is inverted */
	Bool xInvert = FALSE;
	Bool yInvert = FALSE;

	Bool needsInvert = FALSE;
	float renX = fabs (fmodf (mTransform.angX, 360.0f));
	float renY = fabs (fmodf (mTransform.angY, 360.0f));

	if (90 < renX && renX < 270)
	    xInvert = TRUE;

	if (90 < renY && renY < 270)
	    yInvert = TRUE;

	if ((xInvert || yInvert) && !(xInvert && yInvert))
	    needsInvert = TRUE;

	if (needsInvert)
	    glCullFace (invertCull);

	status = gWindow->glPaint (attrib, wTransform, region, mask);

	if (needsInvert)
	    glCullFace (currentCull);

    }
    else
    {
	status = gWindow->glPaint (attrib, wTransform, region, mask);
    }

    // Check if there are rotated windows
    if (!((mTransform.angX >= 0.0f - 0.05 &&
	   mTransform.angX <= 0.0f + 0.05 ) &&
	  (mTransform.angY >= 0.0f - 0.05 &&
	   mTransform.angY <= 0.0f + 0.05 ) &&
	  (mTransform.angZ >= 0.0f - 0.05 &&
	   mTransform.angZ <= 0.0f + 0.05 ) &&
	  (mTransform.scaleX >= 1.0f - 0.00005 &&
	   mTransform.scaleX <= 1.0f + 0.00005 ) &&
	  (mTransform.scaleY >= 1.0f - 0.00005 &&
	   mTransform.scaleY <= 1.0f + 0.00005 )) && !mTransformed)
	mTransformed = TRUE;
    else if (mTransformed)
	mTransformed = FALSE;

    /* There is still animation to be done */
    if (!(((mTransform.angX >= mAnimate.destAngX - 0.05 &&
	    mTransform.angX <= mAnimate.destAngX + 0.05 ) &&
	   (mTransform.angY >= mAnimate.destAngY - 0.05 &&
	    mTransform.angY <= mAnimate.destAngY + 0.05 ) &&
	   (mTransform.angZ >= mAnimate.destAngZ - 0.05 &&
	    mTransform.angZ <= mAnimate.destAngZ + 0.05 ) &&
	   (mTransform.scaleX >= mAnimate.destScaleX - 0.00005 &&
	    mTransform.scaleX <= mAnimate.destScaleX + 0.00005 ) &&
	   (mTransform.scaleY >= mAnimate.destScaleY - 0.00005 &&
	    mTransform.scaleY <= mAnimate.destScaleY + 0.00005 ))))
    {
	damageArea ();
	mIsAnimating = TRUE;
    }
    else if (mIsAnimating) /* We're done animating now, and we were animating */
    {
	if (handleWindowInputInfo ())
	    adjustIPW ();
	mIsAnimating = FALSE;
    }

    return status;
}

/* Paint the window axis help onto the screen */
bool
FWScreen::glPaintOutput (const GLScreenPaintAttrib &attrib,
			 const GLMatrix		   &transform,
			 const CompRegion	   &region,
			 CompOutput		   *output,
			 unsigned int		   mask)
{
    GLMatrix zTransform (transform);

    if (!mTransformedWindows.empty ())
	mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

    bool status = gScreen->glPaintOutput (attrib, transform, region, output, mask);

    if (mAxisHelp && mHoverWindow)
    {
	int j;
	float x = WIN_REAL_X(mHoverWindow) + WIN_REAL_W(mHoverWindow)/2.0;
	float y = WIN_REAL_Y(mHoverWindow) + WIN_REAL_H(mHoverWindow)/2.0;

	FREEWINS_WINDOW (mHoverWindow);

	float zRad = fww->mRadius * (optionGetTdPercent () / 100);

	bool wasCulled = glIsEnabled (GL_CULL_FACE);

	zTransform.toScreenSpace (output, -DEFAULT_Z_CAMERA);

	glPushMatrix ();
	glLoadMatrixf (zTransform.getMatrix ());

	if (wasCulled)
	    glDisable (GL_CULL_FACE);

	if (optionGetShowCircle () && optionGetRotationAxis () == RotationAxisAlwaysCentre)
	{
	    glColor4usv  (optionGetCircleColor ());
	    glEnable (GL_BLEND);

	    glBegin (GL_POLYGON);
	    for (j = 0; j < 360; j += 10)
		glVertex3f ( x + zRad * cos(D2R(j)), y + zRad * sin(D2R(j)), 0.0 );
	    glEnd ();

	    glDisable (GL_BLEND);
	    glColor4usv  (optionGetLineColor ());
	    glLineWidth (3.0);

	    glBegin (GL_LINE_LOOP);
	    for (j = 360; j >= 0; j -= 10)
		glVertex3f ( x + zRad * cos(D2R(j)), y + zRad * sin(D2R(j)), 0.0 );
	    glEnd ();

	    glBegin (GL_LINE_LOOP);
	    for (j = 360; j >= 0; j -= 10)
		glVertex3f( x + fww->mRadius * cos(D2R(j)), y + fww->mRadius * sin(D2R(j)), 0.0 );
	    glEnd ();

	}

	/* Draw the 'gizmo' */
	if (optionGetShowGizmo ())
	{
	    glPushMatrix ();

	    glTranslatef (x, y, 0.0);

	    glScalef (zRad, zRad, zRad / (float)screen->width ());

	    glRotatef (fww->mTransform.angX, 1.0f, 0.0f, 0.0f);
	    glRotatef (fww->mTransform.angY, 0.0f, 1.0f, 0.0f);
	    glRotatef (fww->mTransform.angZ, 0.0f, 0.0f, 1.0f);

	    glLineWidth (4.0f);

	    for (int i = 0; i < 3; i++)
	    {
		glPushMatrix ();
		glColor4f (1.0 * (i==0), 1.0 * (i==1), 1.0 * (i==2), 1.0);
		glRotatef (90.0, 1.0 * (i==0), 1.0 * (i==1), 1.0 * (i==2));

		glBegin (GL_LINE_LOOP);
		for (j=360; j>=0; j -= 10)
		    glVertex3f ( cos (D2R (j)), sin (D2R (j)), 0.0 );
		glEnd ();
		glPopMatrix ();
	    }

	    glPopMatrix ();
	    glColor4usv (defaultColor);
	}

	/* Draw the bounding box */
	if (optionGetShowRegion ())
	{
	    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	    glEnable (GL_BLEND);
	    glColor4us (0x2fff, 0x2fff, 0x4fff, 0x4fff);
	    glRecti (fww->mInputRect.x1 (), fww->mInputRect.y1 (), fww->mInputRect.x2 (), fww->mInputRect.y2 ());
	    glColor4us (0x2fff, 0x2fff, 0x4fff, 0x9fff);
	    glBegin (GL_LINE_LOOP);
	    glVertex2i (fww->mInputRect.x1 (), fww->mInputRect.y1 ());
	    glVertex2i (fww->mInputRect.x2 (), fww->mInputRect.y1 ());
	    glVertex2i (fww->mInputRect.x1 (), fww->mInputRect.y2 ());
	    glVertex2i (fww->mInputRect.x2 (), fww->mInputRect.y2 ());
	    glEnd ();
	    glColor4usv (defaultColor);
	    glDisable (GL_BLEND);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	}

	if (optionGetShowCross ())
	{

	    glColor4usv  (optionGetCrossLineColor ());
	    glBegin(GL_LINES);
	    glVertex3f(x, y - (WIN_REAL_H (mHoverWindow) / 2), 0.0f);
	    glVertex3f(x, y + (WIN_REAL_H (mHoverWindow) / 2), 0.0f);
	    glEnd ();

	    glBegin(GL_LINES);
	    glVertex3f(x - (WIN_REAL_W (mHoverWindow) / 2), y, 0.0f);
	    glVertex3f(x + (WIN_REAL_W (mHoverWindow) / 2), y, 0.0f);
	    glEnd ();

	    /* Move to our first corner (TopLeft)  */
	    if (fww->mInput)
	    {
		glBegin(GL_LINES);
		glVertex3f(fww->mOutput.shapex1, fww->mOutput.shapey1, 0.0f);
		glVertex3f(fww->mOutput.shapex2, fww->mOutput.shapey2, 0.0f);
		glEnd ();

		glBegin(GL_LINES);
		glVertex3f(fww->mOutput.shapex2, fww->mOutput.shapey2, 0.0f);
		glVertex3f(fww->mOutput.shapex4, fww->mOutput.shapey4, 0.0f);
		glEnd ();

		glBegin(GL_LINES);
		glVertex3f(fww->mOutput.shapex4, fww->mOutput.shapey4, 0.0f);
		glVertex3f(fww->mOutput.shapex3, fww->mOutput.shapey3, 0.0f);
		glEnd ();

		glBegin(GL_LINES);
		glVertex3f(fww->mOutput.shapex3, fww->mOutput.shapey3, 0.0f);
		glVertex3f(fww->mOutput.shapex1, fww->mOutput.shapey1, 0.0f);
		glEnd ();
	    }
	}
	if (wasCulled)
	    glEnable(GL_CULL_FACE);

	glColor4usv(defaultColor);
	glPopMatrix ();
    }

    return status;
}

void
FWScreen::donePaint ()
{

    if (mAxisHelp && mHoverWindow)
    {
	FREEWINS_WINDOW (mHoverWindow);

	REGION region;

	region.rects = &region.extents;
	region.numRects = region.size = 1;

	region.extents.x1 = MIN (WIN_REAL_X (mHoverWindow),
				 WIN_REAL_X (mHoverWindow)
				 + WIN_REAL_W (mHoverWindow)
				 / 2.0f - fww->mRadius);
	region.extents.x2 = MAX (WIN_REAL_X (mHoverWindow),
				 WIN_REAL_X (mHoverWindow)
				 + WIN_REAL_W (mHoverWindow)
				 / 2.0f + fww->mRadius);

	region.extents.y1 = MIN (WIN_REAL_Y (mHoverWindow),
				 WIN_REAL_Y (mHoverWindow)
				 + WIN_REAL_H (mHoverWindow)
				 / 2.0f - fww->mRadius);
	region.extents.y2 = MAX (WIN_REAL_Y (mHoverWindow),
				 WIN_REAL_Y (mHoverWindow)
				 + WIN_REAL_H (mHoverWindow)
				 / 2.0f + fww->mRadius);

	CompRegion damageRegion (region.extents.x1, region.extents.y1, region.extents.x2 - region.extents.x1, region.extents.y2 - region.extents.y1);

	cScreen->damageRegion (damageRegion);
    }

    cScreen->donePaint ();
}

/* Damage the Window Rect */
bool
FWWindow::damageRect (bool initial,
		      const CompRect &rect)
{
    FREEWINS_SCREEN(screen);

    if (mTransformed)
	damageArea ();

    /**
     * Special situations where we must damage the screen
     * i.e when we are playing with windows and wobbly is
     * enabled
     */

    if ((mGrab == grabMove && !fws->optionGetImmediateMoves ())
	|| (mIsAnimating || window->grabbed ()))
	fws->cScreen->damageScreen ();

    return cWindow->damageRect (initial, rect);
}
