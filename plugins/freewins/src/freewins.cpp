/**
 * Compiz Fusion Freewins plugin
 *
 * freewins.cpp
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
 *  - Modifier key to rotate on the Z Axis
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

COMPIZ_PLUGIN_20090315 (freewins, FWPluginVTable);

/* Information on window resize */
void
FWWindow::resizeNotify (int dx,
			int dy,
			int dw,
			int dh)
{
    calculateInputRect ();

    int x = WIN_REAL_X(window) + WIN_REAL_W(window)/2.0;
    int y = WIN_REAL_Y(window) + WIN_REAL_H(window)/2.0;

    mRadius = sqrt(pow((x - WIN_REAL_X (window)), 2) + pow((y - WIN_REAL_Y (window)), 2));

    window->resizeNotify (dx, dy, dw, dh);
}

void
FWWindow::moveNotify (int dx,
		      int dy,
		      bool immediate)
{
    FREEWINS_SCREEN (screen);

    /* Did we move and IPW and not the actual window? */
    CompWindow *useWindow = fws->getRealWindow (window);

    if (useWindow)
	useWindow->move (dx, dy, fws->optionGetImmediateMoves ());
    else if (window != fws->mGrabWindow)
	adjustIPW ();

    if (!useWindow)
	useWindow = window;

    int x = WIN_REAL_X (useWindow) + WIN_REAL_W (useWindow) /2.0;
    int y = WIN_REAL_Y (useWindow) + WIN_REAL_H (useWindow) /2.0;


    mRadius = sqrt (pow((x - WIN_REAL_X (useWindow)), 2) + pow ((y - WIN_REAL_Y (useWindow)), 2));

    useWindow->moveNotify (dx, dy, immediate);
}

void
FWScreen::reloadSnapKeys ()
{
    unsigned int imask = optionGetInvertModsMask ();
    mInvertMask = 0;

    if (imask & InvertModsShiftMask)
	mInvertMask |= ShiftMask;
    if (imask & InvertModsAltMask)
	mInvertMask |= CompAltMask;
    if (imask & InvertModsControlMask)
	mInvertMask |= ControlMask;
    if (imask & InvertModsMetaMask)
	mInvertMask |= CompMetaMask;

    unsigned int smask = optionGetSnapModsMask ();
    mSnapMask = 0;
    if (smask & SnapModsShiftMask)
	mSnapMask |= ShiftMask;
    if (smask & SnapModsAltMask)
	mSnapMask |= CompAltMask;
    if (smask & SnapModsControlMask)
	mSnapMask |= ControlMask;
    if (smask & SnapModsMetaMask)
	mSnapMask |= CompMetaMask;
}

void
FWScreen::optionChanged (CompOption *option,
			 FreewinsOptions::Options num)
{
    switch (num)
    {
	case FreewinsOptions::SnapMods:
	case FreewinsOptions::InvertMods:
	    reloadSnapKeys ();
	    break;
	default:
	    break;
    }
}

/* ------ Plugin Initialisation ---------------------------------------*/

/* Window initialisation / cleaning */

FWWindow::FWWindow (CompWindow *w) :
    PluginClassHandler <FWWindow, CompWindow> (w),
    window (w),
    cWindow (CompositeWindow::get (w)),
    gWindow (GLWindow::get (w)),
    mIMidX (WIN_REAL_W (w) / 2.0),
    mIMidY (WIN_REAL_H (w) / 2.0),
    mOMidX (0.0f),
    mOMidY (0.0f),
    mAdjustX (0.0f),
    mAdjustY (0.0f),
    mOldWinX (0),
    mOldWinY (0),
    mWinH (0),
    mWinW (0),
    mDirection (UpDown),
    mCorner (CornerTopLeft),
    mInput (NULL),
    mOutputRect (w->outputRect ()),
    mInputRect (w->borderRect ()),
    mResetting (false),
    mIsAnimating (false),
    mCan2D (false),
    mCan3D (false),
    mTransformed (false),
    mGrab (grabNone)
{
    WindowInterface::setHandler (window);
    CompositeWindowInterface::setHandler (cWindow);
    GLWindowInterface::setHandler (gWindow);

    int x = WIN_REAL_X (w) + WIN_REAL_W (w) /2.0;
    int y = WIN_REAL_Y (w) + WIN_REAL_H (w) /2.0;

    mRadius = sqrt (pow ((x - WIN_REAL_X (w)), 2) + pow ((y - WIN_REAL_Y (w)), 2));
}

FWWindow::~FWWindow ()
{
    if (canShape ())
	handleWindowInputInfo ();

    FREEWINS_SCREEN (screen);

    if (fws->mGrabWindow == window)
	fws->mGrabWindow = NULL;
}

#define ROTATE_INC optionGetRotateIncrementAmount ()
#define NEG_ROTATE_INC optionGetRotateIncrementAmount () *-1

#define SCALE_INC optionGetScaleIncrementAmount ()
#define NEG_SCALE_INC optionGetScaleIncrementAmount () *-1

FWScreen::FWScreen (CompScreen *screen) :
    PluginClassHandler <FWScreen, CompScreen> (screen),
    cScreen (CompositeScreen::get (screen)),
    gScreen (GLScreen::get (screen)),
    mClick_root_x (0),
    mClick_root_y (0),
    mGrabWindow (NULL),
    mHoverWindow (NULL),
    mLastGrabWindow (NULL),
    mAxisHelp (false),
    mSnap (false),
    mInvert (false),
    mSnapMask (0),
    mInvertMask (0),
    mGrabIndex (0)
{
    ScreenInterface::setHandler (screen);
    CompositeScreenInterface::setHandler (cScreen);
    GLScreenInterface::setHandler (gScreen);

    /* TODO: warning about shape! */

    /* BCOP Action initiation */
    optionSetInitiateRotationButtonInitiate (boost::bind (&FWScreen::initiateFWRotate, this, _1, _2, _3));
    optionSetInitiateRotationButtonTerminate (boost::bind (&FWScreen::terminateFWRotate, this, _1, _2, _3));
    optionSetInitiateScaleButtonInitiate (boost::bind (&FWScreen::initiateFWScale, this, _1, _2, _3));
    optionSetInitiateScaleButtonTerminate (boost::bind (&FWScreen::terminateFWScale, this, _1, _2, _3));
    optionSetResetButtonInitiate (boost::bind (&FWScreen::resetFWTransform, this, _1, _2, _3));
    optionSetResetKeyInitiate (boost::bind (&FWScreen::resetFWTransform, this, _1, _2, _3));
    optionSetToggleAxisKeyInitiate (boost::bind (&FWScreen::toggleFWAxis, this, _1, _2, _3));
    
    // Rotate / Scale Up Down Left Right TODO: rebind these actions on option change

    optionSetScaleUpButtonInitiate (boost::bind (&FWScreen::scale, this, _1, _2, _3, SCALE_INC));
    optionSetScaleDownButtonInitiate (boost::bind (&FWScreen::scale, this, _1, _2, _3, NEG_SCALE_INC));
    optionSetScaleUpKeyInitiate (boost::bind (&FWScreen::scale, this, _1, _2, _3, SCALE_INC));
    optionSetScaleDownKeyInitiate (boost::bind (&FWScreen::scale, this, _1, _2, _3, NEG_SCALE_INC));

    optionSetRotateUpKeyInitiate (boost::bind (&FWScreen::rotate, this, _1, _2, _3, 0, ROTATE_INC, 0));
    optionSetRotateDownKeyInitiate (boost::bind (&FWScreen::rotate, this, _1, _2, _3, 0, NEG_ROTATE_INC, 0));
    optionSetRotateLeftKeyInitiate (boost::bind (&FWScreen::rotate, this, _1, _2, _3, ROTATE_INC, 0, 0));
    optionSetRotateRightKeyInitiate (boost::bind (&FWScreen::rotate, this, _1, _2, _3, NEG_ROTATE_INC, 0, 0));
    optionSetRotateCKeyInitiate (boost::bind (&FWScreen::rotate, this, _1, _2, _3, 0, 0, ROTATE_INC));
    optionSetRotateCcKeyInitiate (boost::bind (&FWScreen::rotate, this, _1, _2, _3, 0, 0, NEG_ROTATE_INC));

    optionSetRotateInitiate (boost::bind (&FWScreen::rotateAction, this, _1, _2, _3));
    optionSetIncrementRotateInitiate (boost::bind (&FWScreen::incrementRotateAction, this, _1, _2, _3));
    optionSetScaleInitiate (boost::bind (&FWScreen::scaleAction, this, _1, _2, _3));

    optionSetSnapModsNotify (boost::bind (&FWScreen::optionChanged, this, _1, _2));
    optionSetInvertModsNotify (boost::bind (&FWScreen::optionChanged, this, _1, _2));

    reloadSnapKeys ();
}

bool
FWPluginVTable::init ()
{
    if (!screen->XShape ())
    {
	compLogMessage ("shelf", CompLogLevelError,
			"No Shape extension found. IPW Usage not enabled \n");
    }

    if (CompPlugin::checkPluginABI ("core", CORE_ABIVERSION)		&&
	CompPlugin::checkPluginABI ("composite", COMPIZ_COMPOSITE_ABI)	&&
	CompPlugin::checkPluginABI ("opengl", COMPIZ_OPENGL_ABI))
	return true;

    return false;
}
