/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2005 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "constrain-to-workarea.h"

namespace compiz
{
namespace place
{
unsigned int clampGeometrySizeOnly = (1 << 0);
unsigned int clampGeometryToViewport = (1 << 1);
}
}

namespace cp = compiz::place;
namespace cw = compiz::window;

void
cp::clampGeometryToWorkArea (cw::Geometry            &g,
			     const CompRect          &workArea,
			     const CompWindowExtents &border,
			     unsigned int            flags,
			     const CompSize          &screenSize)
{
    int	     x, y, left, right, bottom, top;

    if (flags & clampGeometryToViewport)
    {
	/* left, right, top, bottom target coordinates, clamed to viewport
	 * sizes as we don't need to validate movements to other viewports;
	 * we are only interested in inner-viewport movements */

	x = g.x () % screenSize.width ();
	if ((x + g.width ()) < 0)
	    x += screenSize.width ();

	y = g.y () % screenSize.height ();
	if ((y + g.height ()) < 0)
	    y += screenSize.height ();
    }
    else
    {
	x = g.x ();
	y = g.y ();
    }

    left   = x - border.left;
    right  = left + g.widthIncBorders () +  (border.left +
					     border.right);
    top    = y - border.top;
    bottom = top + g.heightIncBorders () + (border.top +
					    border.bottom);

    if ((right - left) > workArea.width ())
    {
	left  = workArea.left ();
	right = workArea.right ();
    }
    else
    {
	if (left < workArea.left ())
	{
	    right += workArea.left () - left;
	    left  = workArea.left ();
	}

	if (right > workArea.right ())
	{
	    left -= right - workArea.right ();
	    right = workArea.right ();
	}
    }

    if ((bottom - top) > workArea.height ())
    {
	top    = workArea.top ();
	bottom = workArea.bottom ();
    }
    else
    {
	if (top < workArea.top ())
	{
	    bottom += workArea.top () - top;
	    top    = workArea.top ();
	}

	if (bottom > workArea.bottom ())
	{
	    top   -= bottom - workArea.bottom ();
	    bottom = workArea.bottom ();
	}
    }

    /* bring left/right/top/bottom to actual window coordinates */
    left   += border.left;
    right  -= border.right + 2 * g.border ();
    top    += border.top;
    bottom -= border.bottom + 2 * g.border ();

    if ((right - left) != g.width ())
    {
	g.setWidth (right - left);
	flags &= ~clampGeometrySizeOnly;
    }

    if ((bottom - top) != g.height ())
    {
	g.setHeight (bottom - top);
	flags &= ~clampGeometrySizeOnly;
    }

    if (!(flags & clampGeometrySizeOnly))
    {
	if (left != x)
	    g.setX (g.x () + left - x);

	if (top != y)
	    g.setY (g.y () + top - y);
    }
}

CompPoint &
cp::constrainPositionToWorkArea (CompPoint               &pos,
				 const cw::Geometry      &serverGeometry,
				 const CompWindowExtents &border,
				 const CompRect          &workArea)
{
    CompWindowExtents extents;
    int               delta;

    extents.left   = pos.x () - border.left;
    extents.top    = pos.y () - border.top;
    extents.right  = extents.left + serverGeometry.widthIncBorders () +
		     (border.left +
		      border.right);
    extents.bottom = extents.top + serverGeometry.heightIncBorders () +
		     (border.top +
		      border.bottom);

    delta = workArea.right () - extents.right;
    if (delta < 0)
	extents.left += delta;

    delta = workArea.left () - extents.left;
    if (delta > 0)
	extents.left += delta;

    delta = workArea.bottom () - extents.bottom;
    if (delta < 0)
	extents.top += delta;

    delta = workArea.top () - extents.top;
    if (delta > 0)
	extents.top += delta;

    pos.setX (extents.left + border.left);
    pos.setY (extents.top  + border.top);

    return pos;
}

CompPoint cp::getViewportRelativeCoordinates (const cw::Geometry &geom,
					      const CompSize     &screen)
{

    /* left, right, top, bottom target coordinates, clamed to viewport
     * sizes as we don't need to validate movements to other viewports;
     * we are only interested in inner-viewport movements */

    int x = geom.x () % screen.width ();
    if ((geom.x2 ()) < 0)
	x += screen.width ();

    int y = geom.y () % screen.height ();
    if ((geom.y2 ()) < 0)
	y += screen.height ();

    return CompPoint (x, y);
}

CompWindowExtents cp::getWindowEdgePositions (const CompPoint         &position,
					      const cw::Geometry      &geom,
					      const CompWindowExtents &border)
{
    CompWindowExtents edgePositions;

    edgePositions.left   = position.x () - border.left;
    edgePositions.right  = edgePositions.left +
			   geom.widthIncBorders () +  (border.left +
						       border.right);
    edgePositions.top    = position.y () - border.top;
    edgePositions.bottom = edgePositions.top +
			   geom.heightIncBorders () + (border.top +
						       border.bottom);

    return edgePositions;
}

void cp::clampHorizontalEdgePositionsToWorkArea (CompWindowExtents &edgePositions,
						 const CompRect    &workArea)
{
    if ((edgePositions.right - edgePositions.left) > workArea.width ())
    {
	edgePositions.left  = workArea.left ();
	edgePositions.right = workArea.right ();
    }
    else
    {
	if (edgePositions.left < workArea.left ())
	{
	    edgePositions.right += workArea.left () - edgePositions.left;
	    edgePositions.left  = workArea.left ();
	}

	if (edgePositions.right > workArea.right ())
	{
	    edgePositions.left -= edgePositions.right - workArea.right ();
	    edgePositions.right = workArea.right ();
	}
    }

}

void cp::clampVerticalEdgePositionsToWorkArea (CompWindowExtents &edgePositions,
					       const CompRect    &workArea)
{
    if ((edgePositions.bottom - edgePositions.top) > workArea.height ())
    {
	edgePositions.top    = workArea.top ();
	edgePositions.bottom = workArea.bottom ();
    }
    else
    {
	if (edgePositions.top < workArea.top ())
	{
	    edgePositions.bottom += workArea.top () - edgePositions.top;
	    edgePositions.top    = workArea.top ();
	}

	if (edgePositions.bottom > workArea.bottom ())
	{
	    edgePositions.top   -= edgePositions.bottom - workArea.bottom ();
	    edgePositions.bottom = workArea.bottom ();
	}
    }
}

void cp::subtractBordersFromEdgePositions (CompWindowExtents       &edgePositions,
					   const CompWindowExtents &border,
					   unsigned int            legacyBorder)
{
    const unsigned int doubleBorder = 2 * legacyBorder;

    edgePositions.left   += border.left;
    edgePositions.right  -= border.right + doubleBorder;
    edgePositions.top    += border.top;
    edgePositions.bottom -= border.bottom + doubleBorder;
}

bool cp::onlySizeChanged (unsigned int mask)
{
    return (!(mask & (CWX | CWY)) && (mask & (CWWidth | CWHeight)));
}

bool cp::applyWidthChange (const CompWindowExtents &edgePositions,
			   XWindowChanges          &xwc,
			   unsigned int            &mask)
{
    bool alreadySet = mask & CWWidth;

    if (alreadySet)
	alreadySet = edgePositions.right - edgePositions.left == xwc.width;

    if (!alreadySet)
    {
	xwc.width = edgePositions.right - edgePositions.left;
	mask       |= CWWidth;
	return true;
    }

    return false;
}

bool cp::applyHeightChange (const CompWindowExtents &edgePositions,
			    XWindowChanges          &xwc,
			    unsigned int            &mask)
{
    bool alreadySet = mask & CWHeight;

    if (alreadySet)
	alreadySet = edgePositions.bottom - edgePositions.top == xwc.height;

    if (!alreadySet)
    {
	xwc.height = edgePositions.bottom - edgePositions.top;
	mask        |= CWHeight;
	return true;
    }

    return false;
}
