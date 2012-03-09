/*
 * Copyright © 2008 Dennis Kasprzyk
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
 */

#include <core/windowgeometrysaver.h>

compiz::window::GeometrySaver::GeometrySaver (const Geometry &g) :
    mGeometry (g),
    mMask (0)
{
}

unsigned int
compiz::window::GeometrySaver::push (const Geometry &g, unsigned int mask)
{
    /* Don't allow overwriting of any already set geometry */
    unsigned int useMask = mask & ~mMask;

    mMask |= useMask;
    mGeometry.applyChange (g, useMask);

    return useMask;
}

unsigned int
compiz::window::GeometrySaver::pop (Geometry &g, unsigned int mask)
{
    unsigned int restoreMask = mask & mMask;

    mMask &= ~restoreMask;
    g.applyChange (mGeometry, restoreMask);

    return restoreMask;
}

void
compiz::window::GeometrySaver::update (const Geometry &g,
				       unsigned int   mask)
{
    /* By default, only update bits that have not been saved */
    unsigned int updateMask = ~mMask;

    /* But also update bits that we are forcing an update on */
    updateMask |= mask;

    mGeometry.applyChange (g, updateMask);
}

unsigned int
compiz::window::GeometrySaver::get (Geometry &g)
{
    g = mGeometry;
    return mMask;
}
