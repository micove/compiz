#include "pixmap-requests.h"
#include <boost/foreach.hpp>
#include <algorithm>

#ifndef foreach
#define foreach BOOST_FOREACH
#endif

namespace cd = compiz::decor;
namespace cdp = compiz::decor::protocol;

DecorPixmap::DecorPixmap (Pixmap pixmap, PixmapDestroyQueue::Ptr d) :
    mPixmap (pixmap),
    mDeletor (d)
{
}

DecorPixmap::~DecorPixmap ()
{
    mDeletor->destroyUnusedPixmap (mPixmap);
}

Pixmap
DecorPixmap::getPixmap ()
{
    return mPixmap;
}

X11DecorPixmapReceiver::X11DecorPixmapReceiver (DecorPixmapRequestorInterface *requestor,
						DecorationInterface *decor) :
    mUpdateState (0),
    mDecorPixmapRequestor (requestor),
    mDecoration (decor)
{
}

void
X11DecorPixmapReceiver::pending ()
{
    if (mUpdateState & X11DecorPixmapReceiver::UpdateRequested)
	mUpdateState |= X11DecorPixmapReceiver::UpdatesPending;
    else
    {
	mUpdateState |= X11DecorPixmapReceiver::UpdateRequested;

	mDecorPixmapRequestor->postGenerateRequest (mDecoration->getFrameType (),
						    mDecoration->getFrameState (),
						    mDecoration->getFrameActions ());
    }
}

void X11DecorPixmapReceiver::update ()
{
    if (mUpdateState & X11DecorPixmapReceiver::UpdatesPending)
	mDecorPixmapRequestor->postGenerateRequest (mDecoration->getFrameType (),
						    mDecoration->getFrameState (),
						    mDecoration->getFrameActions ());

    mUpdateState = 0;
}

X11DecorPixmapRequestor::X11DecorPixmapRequestor (Display *dpy,
						  Window  window,
						  DecorationListFindMatchingInterface *listFinder) :
    mDpy (dpy),
    mWindow (window),
    mListFinder (listFinder)
{
}

int
X11DecorPixmapRequestor::postGenerateRequest (unsigned int frameType,
					      unsigned int frameState,
					      unsigned int frameActions)
{
    return decor_post_generate_request (mDpy,
					 mWindow,
					 frameType,
					 frameState,
					 frameActions);
}

void
X11DecorPixmapRequestor::handlePending (const long *data)
{
    DecorationInterface::Ptr d = mListFinder->findMatchingDecoration (static_cast <unsigned int> (data[0]),
								      static_cast <unsigned int> (data[1]),
								      static_cast <unsigned int> (data[2]));

    if (d)
	d->receiverInterface ().pending ();
    else
	postGenerateRequest (static_cast <unsigned int> (data[0]),
			     static_cast <unsigned int> (data[1]),
			     static_cast <unsigned int> (data[2]));
}

namespace
{
typedef PixmapReleasePool::FreePixmapFunc FreePixmapFunc;
}

PixmapReleasePool::PixmapReleasePool (const FreePixmapFunc &freePixmap) :
    mFreePixmap (freePixmap)
{
}

void
PixmapReleasePool::markUnused (Pixmap pixmap)
{
    mPendingUnusedNotificationPixmaps.push_back (pixmap);
    mPendingUnusedNotificationPixmaps.unique ();
}

int
PixmapReleasePool::destroyUnusedPixmap (Pixmap pixmap)
{
    std::list <Pixmap>::iterator it =
	std::find (mPendingUnusedNotificationPixmaps.begin (),
		   mPendingUnusedNotificationPixmaps.end (),
		   pixmap);

    if (it != mPendingUnusedNotificationPixmaps.end ())
    {
	Pixmap pixmap (*it);
	mPendingUnusedNotificationPixmaps.erase (it);

	mFreePixmap (pixmap);
    }

    return 0;
}

cd::PendingHandler::PendingHandler (const cd::RequestorForWindow &requestorForWindow) :
    mRequestorForWindow (requestorForWindow)
{
}

void
cd::PendingHandler::handleMessage (Window window, const long *data)
{
    DecorPixmapRequestorInterface *requestor = mRequestorForWindow (window);

    if (requestor)
	requestor->handlePending (data);
}

cd::UnusedHandler::UnusedHandler (const cd::DecorListForWindow &listForWindow,
				  const UnusedPixmapQueue::Ptr &queue,
				  const FreePixmapFunc         &freePixmap) :
    mListForWindow (listForWindow),
    mQueue (queue),
    mFreePixmap (freePixmap)
{
}

void
cd::UnusedHandler::handleMessage (Window window, Pixmap pixmap)
{
    DecorationListFindMatchingInterface *findMatching = mListForWindow (window);

    if (findMatching)
    {
	DecorationInterface::Ptr decoration (findMatching->findMatchingDecoration (pixmap));

	if (decoration)
	{
	    mQueue->markUnused (pixmap);
	    return;
	}
    }

   /* If a decoration was not found, then this pixmap is no longer in use
    * and we should free it */
    mFreePixmap (pixmap);
}

cdp::Communicator::Communicator (Atom                           pendingMsg,
				 Atom                           unusedMsg,
				 const cdp::PendingMessage      &pending,
				 const cdp::PixmapUnusedMessage &pixmapUnused) :
    mPendingMsgAtom (pendingMsg),
    mUnusedMsgAtom (unusedMsg),
    mPendingHandler (pending),
    mPixmapUnusedHander (pixmapUnused)
{
}

void
cdp::Communicator::handleClientMessage (const XClientMessageEvent &xce)
{
    if (xce.message_type == mPendingMsgAtom)
	mPendingHandler (xce.window, xce.data.l);
    else if (xce.message_type == mUnusedMsgAtom)
	mPixmapUnusedHander (xce.window, xce.data.l[0]);
}
