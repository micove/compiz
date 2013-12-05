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

#ifndef _COMPIZ_DECOR_PIXMAP_REQUESTS_H
#define _COMPIZ_DECOR_PIXMAP_REQUESTS_H

#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/make_shared.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <decoration.h>

#include <X11/Xlib.h>

class DecorPixmapInterface :
    boost::noncopyable
{
    public:

	typedef boost::shared_ptr <DecorPixmapInterface> Ptr;

	virtual ~DecorPixmapInterface () {};

	virtual Pixmap getPixmap () = 0;
};

class DecorPixmapReceiverInterface :
    boost::noncopyable
{
    public:

	virtual ~DecorPixmapReceiverInterface () {}

	virtual void pending () = 0;
	virtual void update () = 0;
};

/* So far, nothing particularly interesting here
 * we just need a way to pass around pointers for
 * testing */
class DecorationInterface :
    boost::noncopyable
{
    public:

	typedef boost::shared_ptr <DecorationInterface> Ptr;

	virtual ~DecorationInterface () {}

	virtual DecorPixmapReceiverInterface & receiverInterface () = 0;
	virtual unsigned int getFrameType () const = 0;
	virtual unsigned int getFrameState () const = 0;
	virtual unsigned int getFrameActions () const = 0;
};

class PixmapDestroyQueue :
    boost::noncopyable
{
    public:

	typedef boost::shared_ptr <PixmapDestroyQueue> Ptr;

	virtual ~PixmapDestroyQueue () {}

	virtual int destroyUnusedPixmap (Pixmap pixmap) = 0;
};

class UnusedPixmapQueue :
    boost::noncopyable
{
    public:

	typedef boost::shared_ptr <UnusedPixmapQueue> Ptr;

	virtual ~UnusedPixmapQueue () {}

	virtual void markUnused (Pixmap pixmap) = 0;
};

class PixmapReleasePool :
    public PixmapDestroyQueue,
    public UnusedPixmapQueue
{
    public:

	typedef boost::function <int (Pixmap)> FreePixmapFunc;

	typedef boost::shared_ptr <PixmapReleasePool> Ptr;

	PixmapReleasePool (const FreePixmapFunc &freePixmap);

	void markUnused (Pixmap pixmap);
	int destroyUnusedPixmap (Pixmap pixmap);

    private:

	std::list <Pixmap> mPendingUnusedNotificationPixmaps;
	FreePixmapFunc     mFreePixmap;

};

class DecorPixmap :
    public DecorPixmapInterface
{
    public:

	typedef boost::shared_ptr <DecorPixmap> Ptr;

	DecorPixmap (Pixmap p, PixmapDestroyQueue::Ptr deletor);
	~DecorPixmap ();

	Pixmap getPixmap ();

    private:

	Pixmap mPixmap;
	PixmapDestroyQueue::Ptr mDeletor;
};

class DecorPixmapRequestorInterface :
    boost::noncopyable
{
    public:

	virtual ~DecorPixmapRequestorInterface () {}

	virtual int postGenerateRequest (unsigned int frameType,
					 unsigned int frameState,
					 unsigned int frameActions) = 0;

	virtual void handlePending (const long *data) = 0;
};

class DecorationListFindMatchingInterface
{
    public:

	virtual ~DecorationListFindMatchingInterface () {}

	virtual DecorationInterface::Ptr findMatchingDecoration (unsigned int frameType,
								 unsigned int frameState,
								 unsigned int frameActions) const = 0;
	virtual DecorationInterface::Ptr findMatchingDecoration (Pixmap pixmap) const = 0;
};

namespace compiz
{
namespace decor
{
typedef boost::function <DecorationListFindMatchingInterface * (Window)> DecorListForWindow;
typedef boost::function <DecorPixmapRequestorInterface * (Window)> RequestorForWindow;

class PendingHandler :
    virtual boost::noncopyable
{
    public:

	PendingHandler (const RequestorForWindow &);

	void handleMessage (Window window, const long *data);

    private:

	RequestorForWindow     mRequestorForWindow;
};

class UnusedHandler :
    virtual boost::noncopyable
{
    public:

	UnusedHandler (const DecorListForWindow &,
		       const UnusedPixmapQueue::Ptr &,
		       const PixmapReleasePool::FreePixmapFunc &);

	void handleMessage (Window, Pixmap);

    private:

	DecorListForWindow mListForWindow;
	UnusedPixmapQueue::Ptr mQueue;
	PixmapReleasePool::FreePixmapFunc mFreePixmap;
};

namespace protocol
{
typedef boost::function <void (Window, const long *)> PendingMessage;
typedef boost::function <void (Window, Pixmap)> PixmapUnusedMessage;

class Communicator :
    virtual boost::noncopyable
{
    public:

	Communicator (Atom pendingMsg,
		      Atom unusedMsg,
		      const PendingMessage &,
		      const PixmapUnusedMessage &);

	void handleClientMessage (const XClientMessageEvent &);

    private:

	Atom           mPendingMsgAtom;
	Atom           mUnusedMsgAtom;
	PendingMessage mPendingHandler;
	PixmapUnusedMessage mPixmapUnusedHander;
};
}
}
}

class X11DecorPixmapRequestor :
    public DecorPixmapRequestorInterface
{
    public:

	X11DecorPixmapRequestor (Display *dpy,
				 Window  xid,
				 DecorationListFindMatchingInterface *listFinder);

	int postGenerateRequest (unsigned int frameType,
				 unsigned int frameState,
				 unsigned int frameActions);

	void handlePending (const long *data);

    private:

	Display *mDpy;
	Window  mWindow;
	DecorationListFindMatchingInterface *mListFinder;
};

class X11DecorPixmapReceiver :
    public DecorPixmapReceiverInterface
{
    public:

	static const unsigned int UpdateRequested = 1 << 0;
	static const unsigned int UpdatesPending = 1 << 1;

	X11DecorPixmapReceiver (DecorPixmapRequestorInterface *,
				DecorationInterface *decor);

	void pending ();
	void update ();
    private:

	unsigned int mUpdateState;
	DecorPixmapRequestorInterface *mDecorPixmapRequestor;
	DecorationInterface *mDecoration;
};

#endif
