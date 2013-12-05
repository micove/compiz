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

#include <tr1/tuple>
#include <test-constrain-to-workarea.h>
#include <constrain-to-workarea.h>
#include <iostream>
#include <stdlib.h>
#include <cstring>

namespace cw = compiz::window;
namespace cwe = cw::extents;
namespace cp = compiz::place;

using ::testing::WithParamInterface;
using ::testing::ValuesIn;
using ::testing::Combine;

class PlaceClampGeometryToWorkArea :
    public CompPlaceTest
{
    public:

	PlaceClampGeometryToWorkArea () :
	    screensize (1000, 2000),
	    workArea (50, 50, 900, 1900),
            flags (0)
        {
	    memset (&extents, 0, sizeof (cwe::Extents));
        }

    protected:

	CompSize screensize;
	CompRect workArea;
	cw::Geometry g;
	cwe::Extents extents;
	unsigned int flags;
};

namespace
{
    const cw::Geometry LimitOfAllowedGeometry (50, 50, 900, 1900, 0);
}

namespace compiz
{
namespace window
{
std::ostream & operator<<(std::ostream &os, const Geometry &g)
{
    return os << "Window Geometry: (" << g.x () <<
                 ", " << g.y () <<
                 ", " << g.width () <<
                 ", " << g.height () <<
                 ", " << g.border () << ")";
}
namespace extents
{
std::ostream & operator<<(std::ostream &os, const Extents &e)
{
    return os << "Window Extents: (left: " << e.left <<
                 ", right: " << e.right <<
                 ", top: " << e.top <<
                 ", bottom: " << e.bottom << ")";
}
}
}
}

std::ostream & operator<<(std::ostream &os, const CompPoint &p)
{
    return os << "Point: (" << p.x () << ", " << p.y () << ")";
}

TEST_F (PlaceClampGeometryToWorkArea, NoConstrainmentRequired)
{
    g = cw::Geometry (100, 100, 200, 200, 0);

    /* Do nothing */
    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, cw::Geometry (100, 100, 200, 200, 0));
}

TEST_F (PlaceClampGeometryToWorkArea, LargerThanWorkAreaConstrainsToWorkAreaSize)
{
    /* Larger than workArea */
    g = cw::Geometry (50, 50, 950, 1950, 0);

    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, LimitOfAllowedGeometry);
}

TEST_F (PlaceClampGeometryToWorkArea, OutsideTopLeftConstrainment)
{
    /* Outside top left */
    g = cw::Geometry (0, 0, 900, 1900, 0);

    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, LimitOfAllowedGeometry);
}

TEST_F (PlaceClampGeometryToWorkArea, OutsideTopRightConstrainment)
{
    /* Outside top right */
    g = cw::Geometry (100, 0, 900, 1900, 0);

    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, LimitOfAllowedGeometry);
}

TEST_F (PlaceClampGeometryToWorkArea, OutsideBottomLeftConstrainment)
{
    /* Outside bottom left */
    g = cw::Geometry (0, 100, 900, 1900, 0);

    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, LimitOfAllowedGeometry);
}

TEST_F (PlaceClampGeometryToWorkArea, OutsideBottomRightConstrainment)
{
    /* Outside bottom right */
    g = cw::Geometry (100, 100, 900, 1900, 0);

    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, LimitOfAllowedGeometry);
}

TEST_F (PlaceClampGeometryToWorkArea, NoChangePositionIfSizeUnchanged)
{
    /* For the size only case, we should not
     * change the position of the window if
     * the size does not change */
    g = cw::Geometry (0, 0, 900, 1900, 0);
    flags = cp::clampGeometrySizeOnly;

    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, cw::Geometry (0, 0, 900, 1900, 0));
}

TEST_F (PlaceClampGeometryToWorkArea, ChangePositionIfSizeChanged)
{
    g = cw::Geometry (0, 0, 1000, 2000, 0);
    flags = cp::clampGeometrySizeOnly;

    cp::clampGeometryToWorkArea (g, workArea, extents, flags, screensize);

    EXPECT_EQ (g, LimitOfAllowedGeometry);
}

namespace
{
typedef std::tr1::tuple <cw::Geometry, cwe::Extents> ConstrainPositionToWorkAreaParam;

const cw::Geometry & WindowGeometry (const ConstrainPositionToWorkAreaParam &p)
{
    return std::tr1::get <0> (p);
}

const cwe::Extents & WindowExtents (const ConstrainPositionToWorkAreaParam &p)
{
    return std::tr1::get <1> (p);
}

CompPoint InitialPosition (const ConstrainPositionToWorkAreaParam &p)
{
    /* Initial position is where the window is right now */
    return (std::tr1::get <0> (p)).pos ();
}

const CompRect  WArea (50, 50, 900, 1900);
const CompPoint ExpectedPosition (WArea.pos ());
}

class PlaceConstrainPositionToWorkArea :
    public CompPlaceTest,
    public WithParamInterface <ConstrainPositionToWorkAreaParam>
{
    public:

	PlaceConstrainPositionToWorkArea ()
        {
	    memset (&extents, 0, sizeof (cwe::Extents));
        }

    protected:

	CompRect workArea;
	cw::Geometry g;
	cwe::Extents extents;
};

TEST_P (PlaceConstrainPositionToWorkArea, PositionConstrainedWithExtents)
{
    g = WindowGeometry (GetParam ());
    extents = WindowExtents (GetParam ());

    CompPoint pos = InitialPosition (GetParam ());
    pos = cp::constrainPositionToWorkArea (pos, g, extents, WArea);

    const CompPoint expectedAfterExtentsAdjustment = ExpectedPosition +
						     CompPoint (extents.left,
								extents.top);

    EXPECT_EQ (expectedAfterExtentsAdjustment, pos);
}

namespace
{
cwe::Extents PossibleExtents[] =
{
    cwe::Extents (0, 0, 0, 0),
    cwe::Extents (1, 0, 0, 0),
    cwe::Extents (0, 1, 0, 0),
    cwe::Extents (0, 0, 1, 0),
    cwe::Extents (0, 0, 0, 1)
};

cw::Geometry PossibleGeometries[] =
{
    cw::Geometry (WArea.x (), WArea.y (),
		  WArea.width (), WArea.height (), 0),
    cw::Geometry (WArea.x () - 1, WArea.y (),
		  WArea.width (), WArea.height (), 0),
    cw::Geometry (WArea.x (), WArea.y () - 1,
		  WArea.width (), WArea.height (), 0),
    cw::Geometry (WArea.x () + 1, WArea.y (),
		  WArea.width (), WArea.height (), 0),
    cw::Geometry (WArea.x (), WArea.y () + 1,
		  WArea.width (), WArea.height (), 0)
};
}

INSTANTIATE_TEST_CASE_P (PlacementData,
                         PlaceConstrainPositionToWorkArea,
                         Combine (ValuesIn (PossibleGeometries),
				  ValuesIn (PossibleExtents)));
class PlaceGetEdgePositions :
    public CompPlaceTest
{
    public:

	PlaceGetEdgePositions () :
	    geom (100, 200, 300, 400, 1),
	    border (1, 2, 3, 4),
	    pos (geom.pos ())
	{
	}

    protected:

	cw::Geometry geom;
	cwe::Extents border;
	CompPoint    pos;
};

TEST_F (PlaceGetEdgePositions, GetEdgePositions)
{
    int left = geom.x () - border.left;
    int right = left + (geom.widthIncBorders ()) +
		    (border.left + border.right);
    int top = geom.y () - border.top;
    int bottom = top + (geom.heightIncBorders ()) +
		    (border.top + border.bottom);

    const cwe::Extents ExpectedExtents (left, right, top, bottom);
    cwe::Extents actualExtents (cp::getWindowEdgePositions (pos,
							    geom,
							    border));

    EXPECT_EQ (ExpectedExtents, actualExtents);
}

namespace
{
const CompSize     SCREEN_SIZE (1000, 1000);
const unsigned int WINDOW_SIZE = 250;
}

TEST (PlaceGetViewportRelativeCoordinates, WithinScreenWidth)
{
    cw::Geometry geom (SCREEN_SIZE.width () / 2,
		       SCREEN_SIZE.height () / 2,
		       WINDOW_SIZE, WINDOW_SIZE,
		       0);

    CompPoint    position (cp::getViewportRelativeCoordinates (geom,
							       SCREEN_SIZE));
    EXPECT_EQ (geom.pos ().x (), position.x ());
}

TEST (PlaceGetViewportRelativeCoordinates, WithinScreenHeight)
{
    cw::Geometry geom (SCREEN_SIZE.width () / 2,
		       SCREEN_SIZE.height () / 2,
		       WINDOW_SIZE, WINDOW_SIZE,
		       0);

    CompPoint    position (cp::getViewportRelativeCoordinates (geom,
							       SCREEN_SIZE));
    EXPECT_EQ (geom.pos ().y (), position.y ());
}

TEST (PlaceGetViewportRelativeCoordinates, OutsideOuterScreenWidth)
{
    cw::Geometry geom (SCREEN_SIZE.width () + 1,
		       SCREEN_SIZE.height () / 2,
		       WINDOW_SIZE, WINDOW_SIZE,
		       0);

    CompPoint    position (cp::getViewportRelativeCoordinates (geom,
							       SCREEN_SIZE));
    EXPECT_EQ (1, position.x ());
}

TEST (PlaceGetViewportRelativeCoordinates, OutsideInnerScreenWidth)
{
    cw::Geometry geom (-(WINDOW_SIZE + 1),
		       SCREEN_SIZE.height () / 2,
		       WINDOW_SIZE, WINDOW_SIZE,
		       0);

    CompPoint    position (cp::getViewportRelativeCoordinates (geom,
							       SCREEN_SIZE));
    EXPECT_EQ (SCREEN_SIZE.width () - (WINDOW_SIZE + 1),
	       position.x ());
}

TEST (PlaceGetViewportRelativeCoordinates, OutsideOuterScreenHeight)
{
    cw::Geometry geom (SCREEN_SIZE.width () / 2,
		       SCREEN_SIZE.height () + 1,
		       WINDOW_SIZE, WINDOW_SIZE,
		       0);

    CompPoint    position (cp::getViewportRelativeCoordinates (geom,
							       SCREEN_SIZE));
    EXPECT_EQ (1, position.y ());
}

TEST (PlaceGetViewportRelativeCoordinates, OutsideInnerScreenHeight)
{
    cw::Geometry geom (SCREEN_SIZE.width () / 2,
		       -(WINDOW_SIZE + 1),
		       WINDOW_SIZE, WINDOW_SIZE,
		       0);

    CompPoint    position (cp::getViewportRelativeCoordinates (geom,
							       SCREEN_SIZE));
    EXPECT_EQ (SCREEN_SIZE.height () - (WINDOW_SIZE + 1),
	       position.y ());
}

namespace
{
const CompRect WORK_AREA (25, 25, 1000, 1000);

const cwe::Extents EdgePositions[] =
{
    /* Exact match */
    cwe::Extents (WORK_AREA.left (), WORK_AREA.right (),
		  WORK_AREA.top (), WORK_AREA.bottom ()),
    /* Just within */
    cwe::Extents (WORK_AREA.left () + 1, WORK_AREA.right () - 1,
		  WORK_AREA.top () + 1, WORK_AREA.bottom () - 1),
    /* Just outside right */
    cwe::Extents (WORK_AREA.left () + 1, WORK_AREA.right () + 1,
		  WORK_AREA.top (), WORK_AREA.bottom ()),
    /* Just outside left */
    cwe::Extents (WORK_AREA.left () - 1, WORK_AREA.right () - 1,
		  WORK_AREA.top (), WORK_AREA.bottom ()),
    /* Just outside top */
    cwe::Extents (WORK_AREA.left (), WORK_AREA.right (),
		  WORK_AREA.top () - 1, WORK_AREA.bottom () - 1),
    /* Just outside bottom */
    cwe::Extents (WORK_AREA.left (), WORK_AREA.right (),
		  WORK_AREA.top () + 1, WORK_AREA.bottom () + 1),
};
}

class PlaceClampEdgePositions :
    public CompPlaceTest,
    public WithParamInterface <CompWindowExtents>
{
};

TEST_P (PlaceClampEdgePositions, WithinWorkAreaWidth)
{
    CompWindowExtents edgePositions (GetParam ());
    cp::clampHorizontalEdgePositionsToWorkArea (edgePositions,
						WORK_AREA);

    const int edgePositionsWidth = edgePositions.right - edgePositions.left;

    ASSERT_GT (edgePositionsWidth, 0);
    EXPECT_LE (edgePositionsWidth, WORK_AREA.width ());
}

TEST_P (PlaceClampEdgePositions, OutsideLeft)
{
    CompWindowExtents edgePositions (GetParam ());
    cp::clampHorizontalEdgePositionsToWorkArea (edgePositions,
						WORK_AREA);

    ASSERT_GE (edgePositions.left, WORK_AREA.left ());
    ASSERT_GE (edgePositions.right, WORK_AREA.left ());
}

TEST_P (PlaceClampEdgePositions, OutsideRight)
{
    CompWindowExtents edgePositions (GetParam ());
    cp::clampHorizontalEdgePositionsToWorkArea (edgePositions,
						WORK_AREA);

    ASSERT_LE (edgePositions.left, WORK_AREA.right ());
    ASSERT_LE (edgePositions.right, WORK_AREA.right ());
}

TEST_P (PlaceClampEdgePositions, WithinWorkAreaHeight)
{
    CompWindowExtents edgePositions (GetParam ());
    cp::clampVerticalEdgePositionsToWorkArea (edgePositions,
					      WORK_AREA);

    const int edgePositionsHeight = edgePositions.bottom - edgePositions.top;

    ASSERT_GT (edgePositionsHeight, 0);
    EXPECT_LE (edgePositionsHeight, WORK_AREA.height ());
}

TEST_P (PlaceClampEdgePositions, OutsideTop)
{
    CompWindowExtents edgePositions (GetParam ());
    cp::clampVerticalEdgePositionsToWorkArea (edgePositions,
					      WORK_AREA);

    ASSERT_GE (edgePositions.top, WORK_AREA.top ());
    ASSERT_GE (edgePositions.bottom, WORK_AREA.top ());
}

TEST_P (PlaceClampEdgePositions, OutsideBottom)
{
    CompWindowExtents edgePositions (GetParam ());
    cp::clampVerticalEdgePositionsToWorkArea (edgePositions,
					      WORK_AREA);

    ASSERT_LE (edgePositions.top, WORK_AREA.bottom ());
    ASSERT_LE (edgePositions.bottom, WORK_AREA.bottom ());
}

INSTANTIATE_TEST_CASE_P (WAEdgePositions, PlaceClampEdgePositions,
			 ValuesIn (EdgePositions));

TEST (PlaceSubtractBordersFromEdgePositions, NormalGravity)
{
    const CompWindowExtents borders (1, 2, 3, 4);
    const CompWindowExtents edgePositions (100, 200, 100, 200);
    const unsigned int      legacyBorder = 1;

    CompWindowExtents       expectedEdgePositions (edgePositions.left + (borders.left),
						   edgePositions.right - (borders.right + 2 * legacyBorder),
						   edgePositions.top + (borders.top),
						   edgePositions.bottom - (borders.bottom + 2 * legacyBorder));

    CompWindowExtents       modifiedEdgePositions (edgePositions);

    cp::subtractBordersFromEdgePositions (modifiedEdgePositions,
					  borders,
					  legacyBorder);

    EXPECT_EQ (expectedEdgePositions, modifiedEdgePositions);
}

TEST (PlaceSizeAndPositionChanged, PositionChangedReturnsFalse)
{
    EXPECT_FALSE (cp::onlySizeChanged (CWX | CWY));
}

TEST (PlaceSizeAndPositionChanged, SizeChangedReturnsTrue)
{
    EXPECT_TRUE (cp::onlySizeChanged (CWWidth | CWHeight));
}

TEST (PlaceSizeAndPositionChanged, XAndWidthChangedReturnsFalse)
{
    EXPECT_FALSE (cp::onlySizeChanged (CWX | CWWidth));
}

TEST (PlaceSizeAndPositionChanged, YAndHeightChangedReturnsFalse)
{
    EXPECT_FALSE (cp::onlySizeChanged (CWY | CWHeight));
}

TEST (PlaceApplyWidthChange, ReturnFalseIfNoChange)
{
    CompWindowExtents edgePositions (100, 200, 100, 200);
    XWindowChanges xwc;
    xwc.width = 100;
    unsigned int mask = CWWidth;
    EXPECT_FALSE (cp::applyWidthChange(edgePositions, xwc, mask));
}

TEST (PlaceApplyWidthChange, ApplyWidthAndSetFlag)
{
    CompWindowExtents edgePositions (100, 200, 100, 200);
    XWindowChanges xwc;
    unsigned int mask = 0;
    ASSERT_TRUE (cp::applyWidthChange(edgePositions, xwc, mask));
    EXPECT_EQ (edgePositions.right - edgePositions.left, xwc.width);
    EXPECT_EQ (CWWidth, mask);
}

TEST (PlaceApplyHeightChange, ReturnFalseIfNoChange)
{
    CompWindowExtents edgePositions (100, 200, 100, 200);
    XWindowChanges xwc;
    xwc.height = 100;
    unsigned int mask = CWHeight;
    EXPECT_FALSE (cp::applyHeightChange(edgePositions, xwc, mask));
}

TEST (PlaceApplyWidthChange, ApplyHeightAndSetFlag)
{
    CompWindowExtents edgePositions (100, 200, 100, 200);
    XWindowChanges xwc;
    unsigned int mask = 0;
    ASSERT_TRUE (cp::applyHeightChange(edgePositions, xwc, mask));
    EXPECT_EQ (edgePositions.bottom - edgePositions.top, xwc.height);
    EXPECT_EQ (CWHeight, mask);
}
