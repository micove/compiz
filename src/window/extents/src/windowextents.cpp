/*
 * Copyright © 2005 Novell, Inc.
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

#include <core/windowextents.h>
#include <core/point.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

CompPoint
compiz::window::extents::shift (const CompWindowExtents &extents,
				unsigned int            gravity)
{
    CompPoint rv = CompPoint ();

    switch (gravity) {
	/* We treat StaticGravity like NorthWestGravity here
	 * as when decorating / undecorating the window we
	 * really do need to move it in order to handle
	 * any cases where it goes offscreen */
	case StaticGravity:
	case NorthGravity:
	case NorthWestGravity:
	case NorthEastGravity:
	case CenterGravity:
	    rv.setY (extents.top);
	    break;
	case SouthGravity:
	case SouthWestGravity:
	case SouthEastGravity:
	    rv.setY (-extents.bottom);
	    break;
	default:
	    break;
    }

    switch (gravity) {
	/* We treat StaticGravity like NorthWestGravity here
	 * as when decorating / undecorating the window we
	 * really do need to move it in order to handle
	 * any cases where it goes offscreen */
	case StaticGravity:
	case WestGravity:
	case NorthWestGravity:
	case SouthWestGravity:
	    rv.setX (extents.left);
	    break;
	case EastGravity:
	case NorthEastGravity:
	case SouthEastGravity:
	    rv.setX (-extents.right);
	    break;
    }

    return rv;
}

compiz::window::extents::Extents::Extents () :
  left (0),
  right (0),
  top (0),
  bottom (0)
{}

compiz::window::extents::Extents::Extents (int left, int right, int top, int bottom) :
    left (left),
    right (right),
    top (top),
    bottom (bottom)
{}

/* Just here to keep ABI compatability */
bool
compiz::window::extents::Extents::operator== (const Extents &other)
{
    const Extents &self = const_cast <const Extents &> (*this);
    return self == other;
}

bool
compiz::window::extents::Extents::operator== (const Extents &other) const
{
    return this->left == other.left &&
	   this->right == other.right &&
	   this->top == other.top &&
	   this->bottom == other.bottom;
}

/* Just here to keep ABI compatability */
bool
compiz::window::extents::Extents::operator!= (const Extents &other)
{
    const Extents &self = const_cast <const Extents &> (*this);
    return self != other;
}

bool
compiz::window::extents::Extents::operator!= (const Extents &other) const
{
    return !(*this == other);
}
