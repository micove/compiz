/*
 * Copyright © 2012 Sam Spilsbury
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
 * Authored by: Sam Spilsbury <smspillaz@gmail.com>
 */
#ifndef _COMPIZ_ASYNC_SERVER_WINDOW_H
#define _COMPIZ_ASYNC_SERVER_WINDOW_H

#include <X11/Xlib.h>

namespace compiz
{
namespace window
{
class AsyncServerWindow
{
    public:

	virtual ~AsyncServerWindow () {};

	virtual int requestConfigureOnClient (const XWindowChanges &xwc,
					      unsigned int         valueMask) = 0;
	virtual int requestConfigureOnWrapper (const XWindowChanges &xwc,
					       unsigned int         valueMask) = 0;
	virtual int requestConfigureOnFrame  (const XWindowChanges &xwc,
					      unsigned int         valueMask) = 0;
	virtual void sendSyntheticConfigureNotify () = 0;
	virtual bool hasCustomShape () const = 0;
};
}
}

#endif
