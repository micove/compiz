/*
 * Copyright © 2011 Canonical Ltd.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Canonical Ltd. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Canonical Ltd. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * CANONICAL, LTD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL CANONICAL, LTD. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authored by: Sam Spilsbury <sam.spilsbury@canonical.com>
 */

#include <test-screen-size-change.h>
#include <screen-size-change.h>

namespace cp = compiz::place;
namespace cw = compiz::window;

namespace compiz
{
namespace window
{
std::ostream &
operator<< (std::ostream &os, const Geometry &g)
{
    return os << "compiz::window::Geometry " << std::endl
	      << " - x: " << g.x () << std::endl
	      << " - y: " << g.y () << std::endl
	      << " - width: " << g.width () << std::endl
	      << " - height: " << g.height () << std::endl
	      << " - border: " << g.border ();
}
}
}

namespace
{
class StubScreenSizeChangeObject :
    public cp::ScreenSizeChangeObject
{
    public:

	StubScreenSizeChangeObject (const cw::Geometry &);
	~StubScreenSizeChangeObject ();

	const cw::Geometry & getGeometry () const;
	void applyGeometry (cw::Geometry &n,
			    cw::Geometry &o);
	const CompPoint & getViewport () const;
	const CompRect &  getWorkarea (const cw::Geometry &g) const;
	const cw::extents::Extents & getExtents () const;
	unsigned int getState () const;

	void setVp (const CompPoint &);
	void setWorkArea (const CompRect &);
	void setExtents (unsigned int left,
			 unsigned int right,
			 unsigned int top,
			 unsigned int bottom);

	void setGeometry (const cw::Geometry &g);
	cw::Geometry sizeAdjustTest (const CompSize &oldSize,
				     const CompSize &newSize);

    private:

	CompPoint            mCurrentVp;
	CompRect             mCurrentWorkArea;
	cw::extents::Extents mCurrentExtents;
	cw::Geometry         mCurrentGeometry;
};

const unsigned int MOCK_STRUT_SIZE = 24;

const unsigned int WIDESCREEN_MONITOR_WIDTH = 1280;
const unsigned int TALLER_MONITOR_HEIGHT = 1050;
const unsigned int MONITOR_WIDTH = 1024;
const unsigned int MONITOR_HEIGHT = 768;

const unsigned int DUAL_MONITOR_WIDTH = MONITOR_WIDTH * 2;
const unsigned int DUAL_MONITOR_HEIGHT = MONITOR_HEIGHT * 2;

const unsigned int WINDOW_WIDTH = 300;
const unsigned int WINDOW_HEIGHT = 400;
const unsigned int WINDOW_X = (MONITOR_WIDTH / 2) - (WINDOW_WIDTH / 2);
const unsigned int WINDOW_Y = (MONITOR_HEIGHT / 2) - (WINDOW_HEIGHT / 2);

void
reserveStruts (CompRect     &workArea,
	       unsigned int strutSize)
{
    workArea.setLeft (workArea.left () + strutSize);
    workArea.setTop (workArea.top () + strutSize);
    workArea.setBottom (workArea.bottom () - strutSize);
}
}

class PlaceScreenSizeChange :
    public CompPlaceScreenSizeChangeTest
{
    protected:

	PlaceScreenSizeChange () :
	    windowGeometryBeforeChange (),
	    stubScreenSizeChangeObject (windowGeometryBeforeChange)
	{
	}

	cw::Geometry
	ChangeScreenSizeAndAdjustWindow (const CompSize &newSize);

	void
	SetWindowGeometry (const cw::Geometry &geometry);

	void
	SetInitialScreenSize (const CompSize &size);

	cw::Geometry
	GetInitialWindowGeometry ();

    private:

	CompSize screenSizeAfterChange;
	CompSize screenSizeBeforeChange;

	cw::Geometry windowGeometryBeforeChange;

	StubScreenSizeChangeObject stubScreenSizeChangeObject;
};

cw::Geometry
PlaceScreenSizeChange::GetInitialWindowGeometry ()
{
    return windowGeometryBeforeChange;
}

void
PlaceScreenSizeChange::SetInitialScreenSize (const CompSize &size)
{
    screenSizeBeforeChange = size;
}

void
PlaceScreenSizeChange::SetWindowGeometry (const compiz::window::Geometry &geometry)
{
    windowGeometryBeforeChange = geometry;
    stubScreenSizeChangeObject.setGeometry (windowGeometryBeforeChange);
}

cw::Geometry
PlaceScreenSizeChange::ChangeScreenSizeAndAdjustWindow (const CompSize &newSize)
{
    cw::Geometry g (stubScreenSizeChangeObject.sizeAdjustTest (screenSizeBeforeChange,
							       newSize));
    screenSizeBeforeChange = newSize;
    return g;
}

StubScreenSizeChangeObject::StubScreenSizeChangeObject (const cw::Geometry &g) :
    ScreenSizeChangeObject (g),
    mCurrentVp (0, 0),
    mCurrentWorkArea (50, 50, 1000, 1000),
    mCurrentGeometry (g)
{
    memset (&mCurrentExtents, 0, sizeof (cw::extents::Extents));
}

StubScreenSizeChangeObject::~StubScreenSizeChangeObject ()
{
}

const cw::Geometry &
StubScreenSizeChangeObject::getGeometry () const
{
    return mCurrentGeometry;
}

void
StubScreenSizeChangeObject::applyGeometry (cw::Geometry &n,
					   cw::Geometry &o)
{
    ASSERT_EQ (mCurrentGeometry, o) << "incorrect usage of applyGeometry";

    mCurrentGeometry = n;
}

const CompPoint &
StubScreenSizeChangeObject::getViewport () const
{
    return mCurrentVp;
}

const CompRect &
StubScreenSizeChangeObject::getWorkarea (const cw::Geometry &g) const
{
    return mCurrentWorkArea;
}

const cw::extents::Extents &
StubScreenSizeChangeObject::getExtents () const
{
    return mCurrentExtents;
}

unsigned int
StubScreenSizeChangeObject::getState () const
{
    return 0;
}

void
StubScreenSizeChangeObject::setVp (const CompPoint &p)
{
    mCurrentVp = p;
}

void
StubScreenSizeChangeObject::setWorkArea (const CompRect &wa)
{
    mCurrentWorkArea = wa;
}

void
StubScreenSizeChangeObject::setExtents (unsigned int left,
				        unsigned int right,
					unsigned int top,
					unsigned int bottom)
{
    mCurrentExtents.left = left;
    mCurrentExtents.right = right;
    mCurrentExtents.top = top;
    mCurrentExtents.bottom = bottom;
}

void
StubScreenSizeChangeObject::setGeometry (const cw::Geometry &g)
{
    mCurrentGeometry = g;
}

cw::Geometry
StubScreenSizeChangeObject::sizeAdjustTest (const CompSize &oldSize,
					    const CompSize &newSize)
{
    CompRect workArea (0,
		       0,
		       newSize.width (),
		       newSize.height ());

    /* Reserve top, bottom and left parts of the screen for
     * fake "24px" panels */
    reserveStruts (workArea, MOCK_STRUT_SIZE);

    setWorkArea (workArea);

    cw::Geometry g = adjustForSize (oldSize, newSize);

    return g;
}

TEST_F (PlaceScreenSizeChange, NoMovementOnSmallerWidth)
{
    SetInitialScreenSize (CompSize (WIDESCREEN_MONITOR_WIDTH,
				    MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* First test that changing the screen size
     * to something smaller here doesn't cause our
     * (small) window to be moved */
    cw::Geometry expectedWindowGeometryAfterChange =
	GetInitialWindowGeometry ();

    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
						   MONITOR_HEIGHT));

    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, NoMovementOnSmallerHeight)
{
    SetInitialScreenSize (CompSize (MONITOR_WIDTH,
				    TALLER_MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* First test that changing the screen size
     * to something smaller here doesn't cause our
     * (small) window to be moved */
    cw::Geometry expectedWindowGeometryAfterChange =
	GetInitialWindowGeometry ();

    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
						   MONITOR_HEIGHT));

    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, NoMovementOnLargerSize)
{
    SetInitialScreenSize (CompSize (MONITOR_WIDTH,
				    MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Making the screen size bigger with no
     * saved geometry should cause the window not to move */
    cw::Geometry expectedWindowGeometryAfterChange =
	GetInitialWindowGeometry ();

    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, MovedToEdgeOfRemainingMonitorOnUnplug)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));
    /* Move the window to the other "monitor" */
    SetWindowGeometry (cw::Geometry (MONITOR_WIDTH + WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug a "monitor" */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
						   MONITOR_HEIGHT));

    /* The window should be exactly on-screen at the edge */
    cw::Geometry expectedWindowGeometryAfterChange (MONITOR_WIDTH - WINDOW_WIDTH,
						    WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, MovedBackToOriginalPositionOnReplug)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH, MONITOR_HEIGHT));
    /* Move the window to the other "monitor" */
    SetWindowGeometry (cw::Geometry (MONITOR_WIDTH + WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug a "monitor" */
    ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH, MONITOR_HEIGHT));

    /* Re-plug the monitor - window should go back
     * to the same position */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH, MONITOR_HEIGHT));

    /* Window should be at the same position we left it at */
    cw::Geometry expectedWindowGeometryAfterChange (MONITOR_WIDTH + WINDOW_X,
						    WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, MovedBackToOriginalPositionOnExpansion)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (MONITOR_WIDTH + WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug a "monitor" */
    ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
					       MONITOR_HEIGHT));

    /* Re-plug the monitor */
    ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
					       MONITOR_HEIGHT));

    /* Plug 2 monitors downwards, no change */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange (MONITOR_WIDTH + WINDOW_X,
						    WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, NoOverlapStrutsOnRePlacement)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));
    ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
					       DUAL_MONITOR_HEIGHT));

    /* Move the window to the bottom "monitor" */
    SetWindowGeometry (cw::Geometry (MONITOR_WIDTH + WINDOW_X,
				     MONITOR_HEIGHT + WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug bottom "monitor" */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange (MONITOR_WIDTH + WINDOW_X,
						    MONITOR_HEIGHT -
						    WINDOW_HEIGHT -
						    MOCK_STRUT_SIZE,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);

    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, MovedToOriginalPositionOnPerpendicularExpansion)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
					       DUAL_MONITOR_HEIGHT));

    /* Move the window to the bottom "monitor" */
    SetWindowGeometry (cw::Geometry (MONITOR_WIDTH + WINDOW_X,
				     MONITOR_HEIGHT + WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug bottom "monitor" */
    ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
					       MONITOR_HEIGHT));

    /* Re-plug bottom "monitor" */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange (MONITOR_WIDTH + WINDOW_X,
						    MONITOR_HEIGHT + WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainOnSecondViewportWhenConstraineToFirst)
{
    /* Unplug a "monitor" */
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));
    /* Move the entire window right a viewport */
    SetWindowGeometry (cw::Geometry (DUAL_MONITOR_WIDTH +
				     MONITOR_WIDTH +
				     WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    cw::Geometry expectedWindowGeometryAfterChange (MONITOR_WIDTH +
						    (MONITOR_WIDTH -
						     WINDOW_WIDTH),
						    WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);

    /* Now change the screen resolution again - the window should
     * move to be within the constrained size of its current
     * viewport */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainOnSecondViewportAfterMovedToOriginalPosition)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));
    /* Move the entire window right a viewport */
    SetWindowGeometry (cw::Geometry (DUAL_MONITOR_WIDTH +
				     MONITOR_WIDTH +
				     WINDOW_X,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug a "monitor" */
    ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
					       DUAL_MONITOR_HEIGHT));

    /* Replug the monitor, make sure that the geometry is restored */
    cw::Geometry expectedWindowGeometryAfterChange (DUAL_MONITOR_WIDTH +
						    MONITOR_WIDTH +
						    WINDOW_X,
						    WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);

    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainAtOriginalPositionOnSecondViewportUnplug)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (DUAL_MONITOR_WIDTH + WINDOW_X,
				     MONITOR_HEIGHT + WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange (MONITOR_WIDTH + WINDOW_X,
						    MONITOR_HEIGHT + WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainAtOriginalPositionOnSecondViewportReplug)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));
    SetWindowGeometry (cw::Geometry (DUAL_MONITOR_WIDTH + WINDOW_X,
				     MONITOR_HEIGHT + WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
					       DUAL_MONITOR_HEIGHT));

    /* Replug the monitor and move the window to where it fits on the first
     * monitor on the second viewport, then make sure it doesn't move */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange = GetInitialWindowGeometry ();
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainOnPreviousViewportWhenMovedToFirstMonitor)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));

    /* Deal with the case where the position is negative, which means
     * it's actually wrapped around to the rightmost viewport
     */
    SetWindowGeometry (cw::Geometry (WINDOW_X - MONITOR_WIDTH,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug the right "monitor" */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange (-WINDOW_WIDTH,
						    WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainOnPreviousViewportWhenRestored)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));

    /* Deal with the case where the position is negative, which means
     * it's actually wrapped around to the rightmost viewport
     */
    SetWindowGeometry (cw::Geometry (WINDOW_X - MONITOR_WIDTH,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    /* Unplug the right "monitor" */
    ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
					       DUAL_MONITOR_HEIGHT));

    /* Re-plug the right "monitor" */
    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange = GetInitialWindowGeometry ();
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainOnPreviousViewportFirstMonitor)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));

    /* Move the window to the left monitor, verify that it survives an
     * unplug/plug cycle
     */
    SetWindowGeometry (cw::Geometry (WINDOW_X - DUAL_MONITOR_WIDTH,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange (WINDOW_X - MONITOR_WIDTH,
						    WINDOW_Y,
						    WINDOW_WIDTH,
						    WINDOW_HEIGHT,
						    0);
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}

TEST_F (PlaceScreenSizeChange, RemainOnPreviousViewportFirstMonitorWhenRestored)
{
    SetInitialScreenSize (CompSize (DUAL_MONITOR_WIDTH,
				    DUAL_MONITOR_HEIGHT));

    /* Move the window to the left monitor, verify that it survives an
     * unplug/plug cycle
     */
    SetWindowGeometry (cw::Geometry (WINDOW_X - DUAL_MONITOR_WIDTH,
				     WINDOW_Y,
				     WINDOW_WIDTH,
				     WINDOW_HEIGHT,
				     0));

    ChangeScreenSizeAndAdjustWindow (CompSize (MONITOR_WIDTH,
					       DUAL_MONITOR_HEIGHT));

    cw::Geometry windowGeometryAfterChange =
	ChangeScreenSizeAndAdjustWindow (CompSize (DUAL_MONITOR_WIDTH,
						   DUAL_MONITOR_HEIGHT));

    cw::Geometry expectedWindowGeometryAfterChange
	= GetInitialWindowGeometry ();
    EXPECT_EQ (expectedWindowGeometryAfterChange, windowGeometryAfterChange);
}
