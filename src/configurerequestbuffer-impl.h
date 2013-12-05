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
#ifndef _COMPIZ_GEOMETRY_UPDATE_QUEUE_H
#define _COMPIZ_GEOMETRY_UPDATE_QUEUE_H

#include <memory>
#include <vector>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <X11/Xlib.h>

#include <core/configurerequestbuffer.h>

namespace compiz
{
namespace window
{
class AsyncServerWindow;
namespace configure_buffers
{

class Lockable
{
    public:

	typedef boost::shared_ptr <Lockable> Ptr;
	typedef boost::weak_ptr <Lockable> Weak;

	virtual ~Lockable () {}

	virtual void lock () = 0;
};

class BufferLock :
    public Lockable,
    public Releasable
{
    public:
	typedef boost::shared_ptr <BufferLock> Ptr;

	virtual ~BufferLock () {}
};

class CountedFreeze
{
    public:

	virtual ~CountedFreeze () {}

	virtual void freeze () = 0;
	virtual void release () = 0;

	virtual void untrackLock (compiz::window::configure_buffers::BufferLock *lock) = 0;
};

class ConfigureRequestBuffer :
    public CountedFreeze,
    public Buffer
{
    public:

	typedef boost::function <BufferLock::Ptr (CountedFreeze *)> LockFactory;

	void freeze ();
	void release ();

	void untrackLock (compiz::window::configure_buffers::BufferLock *lock);

	void pushClientRequest (const XWindowChanges &xwc, unsigned int mask);
	void pushWrapperRequest (const XWindowChanges &xwc, unsigned int mask);
	void pushFrameRequest (const XWindowChanges &xwc, unsigned int mask);
	void pushSyntheticConfigureNotify ();
	compiz::window::configure_buffers::Releasable::Ptr obtainLock ();

	/* Implement getAttributes and require that
	 * the queue is released before calling through
	 * to the SyncServerWindow */
	bool queryAttributes (XWindowAttributes &attrib);
	bool queryFrameAttributes (XWindowAttributes &attrib);
	XRectangle * queryShapeRectangles (int kind,
					   int *count,
					   int *ordering);

	void forceRelease ();

	static compiz::window::configure_buffers::Buffer::Ptr
	Create (AsyncServerWindow *asyncServerWindow,
		SyncServerWindow  *syncServerWindow,
		const LockFactory &factory);

    private:

	ConfigureRequestBuffer (AsyncServerWindow *asyncServerWindow,
				SyncServerWindow  *syncServerWindow,
				const LockFactory &factory);

	class Private;
	std::auto_ptr <Private> priv;
};

class ConfigureBufferLock :
    public compiz::window::configure_buffers::BufferLock
{
    public:

	typedef boost::shared_ptr <ConfigureBufferLock> Ptr;

	ConfigureBufferLock (CountedFreeze *);
	~ConfigureBufferLock ();

	void lock ();
	void release ();

    private:

	class Private;
	std::auto_ptr <Private> priv;
};
}
}
}
#endif
