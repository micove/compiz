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
#ifndef _COMPIZ_CONFIGURE_REQUEST_BUFFER_H
#define _COMPIZ_CONFIGURE_REQUEST_BUFFER_H

#include <boost/shared_ptr.hpp>
#include <X11/Xlib.h>
#include <syncserverwindow.h>

namespace compiz
{
namespace window
{
namespace configure_buffers
{
class Releasable
{
    public:

	virtual ~Releasable () {}
	typedef boost::shared_ptr <Releasable> Ptr;

	virtual void release () = 0;
};

class Buffer :
    public SyncServerWindow
{
    public:

	typedef boost::shared_ptr <Buffer> Ptr;

	virtual ~Buffer () {}

	virtual void pushClientRequest (const XWindowChanges &xwc, unsigned int mask) = 0;
	virtual void pushWrapperRequest (const XWindowChanges &xwc, unsigned int mask) = 0;
	virtual void pushFrameRequest (const XWindowChanges &xwc, unsigned int mask) = 0;

	virtual void pushSyntheticConfigureNotify () = 0;

	virtual Releasable::Ptr obtainLock () = 0;

	/* This API will all configure requests to be
	 * released. It should only be used in situations
	 * where you have a server grab and need
	 * to have complete sync with the server */
	virtual void forceRelease () = 0;
};
}
}
}
#endif
