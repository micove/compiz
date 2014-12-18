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

#include "screen-size-change.h"
#include <stdio.h>


compiz::place::ScreenSizeChangeObject::ScreenSizeChangeObject (const compiz::window::Geometry &g) :
    mSaver (g)
{
}

compiz::place::ScreenSizeChangeObject::~ScreenSizeChangeObject ()
{
}

compiz::window::Geometry
compiz::place::ScreenSizeChangeObject::adjustForSize (const CompSize &oldSize,
						      const CompSize &newSize)
{
    int            vpX, vpY;
    compiz::window::Geometry g, vpRelRect;
    int		   pivotX, pivotY, pivotWidth, pivotHeight;

    g = getGeometry ();
    compiz::window::Geometry og (g);

    pivotX = g.x ();
    pivotY = g.y ();

    /* FIXME: Should use saved geometry for maximized / fullscreen windows */

    /* calculate target vp x, y index for window's pivot point */
    pivotWidth = (getState () & compiz::place::WindowMaximized) ? newSize.width () : oldSize.width ();
    pivotHeight = (getState () & compiz::place::WindowMaximized) ? newSize.height () : oldSize.height ();

    vpX = pivotX / pivotWidth;
    if (pivotX < 0)
	vpX -= 1;
    vpY = pivotY / pivotHeight;
    if (pivotY < 0)
	vpY -= 1;

    unsigned int mask = mSaver.pop (vpRelRect, CHANGE_X | CHANGE_Y |
					       CHANGE_WIDTH | CHANGE_HEIGHT);

    if (mask)
    {
	/* set position/size to saved original rectangle */
	g.applyChange (compiz::window::Geometry (vpRelRect.x (),
						 vpRelRect.y (),
						 vpRelRect.width (),
						 vpRelRect.height (),
						 vpRelRect.border ()), mask);
    }
    else
    {
	/* set position/size to window's current rectangle
	   (with position relative to target viewport) */
	vpRelRect.setX (g.x () - vpX * oldSize.width ());
	vpRelRect.setY (g.y () - vpY * oldSize.height ());
	vpRelRect.setWidth (g.width ());
	vpRelRect.setHeight (g.height ());

	g = vpRelRect;

	/* if coords. relative to viewport are outside new viewport area,
	   shift window left/up so that it falls inside */
	if (vpRelRect.x () + vpRelRect.width() >= newSize.width ())
	    g.setX (g.x () - (vpRelRect.x () + vpRelRect.width () - newSize.width ()));
	if (vpRelRect.y () + vpRelRect.height() >= newSize.height ())
	    g.setY (g.y () - (vpRelRect.y () + vpRelRect.width () - newSize.height ()));

	g.setWidth (vpRelRect.width ());
	g.setHeight (vpRelRect.height ());
    }

    unsigned int flags = 0;
    const CompRect &workArea = getWorkarea (g);

    compiz::place::clampGeometryToWorkArea (g, workArea, getExtents (), flags, newSize);

    if (!mask)
    {
	/* save window geometry (relative to viewport) so that it
	can be restored later */
	mask = getGeometry ().changeMask (g);
	mSaver.push (vpRelRect, mask);
    }
    else
    {
	compiz::window::Geometry rg (vpRelRect.x () + vpX * newSize.width (),
				     vpRelRect.y () + vpY * newSize.height (),
				     vpRelRect.width (),
				     vpRelRect.height (), vpRelRect.border ());

	/* Don't care about any bits not restored */
	rg.applyChange (g, ~mask);

	/* Push any bits back on the saver
	 * that don't match the requested window geometry
	 * since we will need to restore to them later */

	unsigned int remaining = g.changeMask (rg);
	mSaver.push (vpRelRect, remaining);
    }

    g.setX (g.x () + vpX * newSize.width ());
    g.setY (g.y () + vpY * newSize.height ());

    /* for maximized/fullscreen windows, update saved pos/size XXX,
     * also pull in the old code to handle maximized windows which
     * currently can't be implemented yet */

    /* actually move/resize window in directions given by mask */
    applyGeometry (g, og);

    return g;
}

void
compiz::place::ScreenSizeChangeObject::unset ()
{
    compiz::window::Geometry g;
    mSaver.pop (g, !0);
}
