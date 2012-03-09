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

#include "privatewindow.h"
#include "core/window.h"


CompWindow::Geometry &
CompWindow::serverGeometry () const
{
    return priv->serverGeometry;
}

CompWindow::Geometry &
CompWindow::geometry () const
{
    return priv->geometry;
}

int
CompWindow::x () const
{
    return priv->geometry.x ();
}

int
CompWindow::y () const
{
    return priv->geometry.y ();
}

CompPoint
CompWindow::pos () const
{
    return CompPoint (priv->geometry.x (), priv->geometry.y ());
}

/* With border */
int
CompWindow::width () const
{
    return priv->width +
	    priv->geometry.border ()  * 2;
}

int
CompWindow::height () const
{
    return priv->height +
	    priv->geometry.border ()  * 2;;
}

CompSize
CompWindow::size () const
{
    return CompSize (priv->width + priv->geometry.border ()  * 2,
		     priv->height + priv->geometry.border ()  * 2);
}

int
CompWindow::serverX () const
{
    return priv->serverGeometry.x ();
}

int
CompWindow::serverY () const
{
    return priv->serverGeometry.y ();
}

CompPoint
CompWindow::serverPos () const
{
    return CompPoint (priv->serverGeometry.x (),
		      priv->serverGeometry.y ());
}

/* With border */
int
CompWindow::serverWidth () const
{
    return priv->serverGeometry.width () +
	   2 * priv->serverGeometry.border ();
}

int
CompWindow::serverHeight () const
{
    return priv->serverGeometry.height () +
	   2 * priv->serverGeometry.border ();
}

const CompSize
CompWindow::serverSize () const
{
    return CompSize (priv->serverGeometry.width () +
		     2 * priv->serverGeometry.border (),
		     priv->serverGeometry.height () +
		     2 * priv->serverGeometry.border ());
}

CompRect
CompWindow::borderRect () const
{
    return CompRect (priv->geometry.x () - priv->geometry.border () - priv->border.left,
		     priv->geometry.y () - priv->geometry.border () - priv->border.top,
		     priv->geometry.width () + priv->geometry.border () * 2 +
		     priv->border.left + priv->border.right,
		     priv->geometry.height () + priv->geometry.border () * 2 +
		     priv->border.top + priv->border.bottom);
}

CompRect
CompWindow::serverBorderRect () const
{
    return CompRect (priv->serverGeometry.x () - priv->geometry.border () - priv->border.left,
		     priv->serverGeometry.y () - priv->geometry.border () - priv->border.top,
		     priv->serverGeometry.width () + priv->geometry.border () * 2 +
		     priv->border.left + priv->border.right,
		     priv->serverGeometry.height () + priv->geometry.border () * 2 +
		     priv->border.top + priv->border.bottom);
}

CompRect
CompWindow::inputRect () const
{
    return CompRect (priv->geometry.x () - priv->geometry.border () - priv->serverInput.left,
		     priv->geometry.y () - priv->geometry.border () - priv->serverInput.top,
		     priv->geometry.width () + priv->geometry.border () * 2 +
		     priv->serverInput.left + priv->serverInput.right,
		     priv->geometry.height () +priv->geometry.border () * 2 +
		     priv->serverInput.top + priv->serverInput.bottom);
}

CompRect
CompWindow::serverInputRect () const
{
    return CompRect (priv->serverGeometry.x () - priv->serverGeometry.border () - priv->serverInput.left,
		     priv->serverGeometry.y () - priv->serverGeometry.border () - priv->serverInput.top,
		     priv->serverGeometry.width () + priv->serverGeometry.border () * 2 +
		     priv->serverInput.left + priv->serverInput.right,
		     priv->serverGeometry.height () + priv->serverGeometry.border () * 2 +
		     priv->serverInput.top + priv->serverInput.bottom);
}

CompRect
CompWindow::outputRect () const
{
    return CompRect (priv->geometry.x () - priv->serverGeometry.border ()- priv->output.left,
		     priv->geometry.y () - priv->serverGeometry.border () - priv->output.top,
		     priv->geometry.width () + priv->serverGeometry.border () * 2 +
		     priv->output.left + priv->output.right,
		     priv->geometry.height () + priv->serverGeometry.border () * 2 +
		     priv->output.top + priv->output.bottom);
}

CompRect
CompWindow::serverOutputRect () const
{
    return CompRect (priv->serverGeometry.x () - priv->serverGeometry.border () -  priv->output.left,
		     priv->serverGeometry.y () - priv->serverGeometry.border () - priv->output.top,
		     priv->serverGeometry.width () + priv->serverGeometry.border () * 2 +
		     priv->output.left + priv->output.right,
		     priv->serverGeometry.height () + priv->serverGeometry.border () * 2 +
		     priv->output.top + priv->output.bottom);
}
