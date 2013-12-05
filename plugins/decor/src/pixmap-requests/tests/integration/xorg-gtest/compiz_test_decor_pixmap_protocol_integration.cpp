/*
 * Compiz XOrg GTest Decoration Pixmap Protocol Integration Tests
 *
 * Copyright (C) 2013 Sam Spilsbury.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authored By:
 * Sam Spilsbury <smspillaz@gmail.com>
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "decoration.h"

#include <xorg/gtest/xorg-gtest.h>
#include <compiz-xorg-gtest.h>

#include "pixmap-requests.h"
#include "compiz_decor_pixmap_requests_mock.h"

namespace xt = xorg::testing;
namespace ct = compiz::testing;
namespace cd = compiz::decor;
namespace cdp = compiz::decor::protocol;

using ::testing::AtLeast;
using ::testing::ReturnNull;
using ::testing::Return;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::_;

class DecorPixmapProtocol :
    public xorg::testing::Test
{
    public:

	typedef boost::function <void (const XClientMessageEvent &)> ClientMessageReceiver;

	void SetUp ();

	void WaitForClientMessageOnAndDeliverTo (Window client,
						 Atom   message,
						 const ClientMessageReceiver &receiver);

    protected:

	Atom deletePixmapMessage;
	Atom pendingMessage;
};

namespace
{
bool Advance (Display *d, bool result)
{
    return ct::AdvanceToNextEventOnSuccess (d, result);
}

Window MOCK_WINDOW = 1;
Pixmap MOCK_PIXMAP = 2;

class MockClientMessageReceiver
{
    public:

	MOCK_METHOD1 (receiveMsg, void (const XClientMessageEvent &));
};

class DisplayFetch
{
    public:

	virtual ~DisplayFetch () {}

	virtual ::Display * Display () const = 0;
};

class XFreePixmapWrapper
{
    public:

	XFreePixmapWrapper (const DisplayFetch &df) :
	    mDisplayFetch (df)
	{
	}

	int FreePixmap (Pixmap pixmap)
	{
	    return XFreePixmap (mDisplayFetch.Display (), pixmap);
	}

    private:

	const DisplayFetch &mDisplayFetch;
};

class XErrorTracker
{
    public:

	XErrorTracker ();
	~XErrorTracker ();

	MOCK_METHOD1 (errorHandler, void (int));

    private:

	XErrorHandler handler;

	static int handleXError (Display *dpy, XErrorEvent *ev);
	static XErrorTracker *tracker;
};

XErrorTracker * XErrorTracker::tracker = NULL;

XErrorTracker::XErrorTracker ()
{
    tracker = this;
    handler = XSetErrorHandler (handleXError);
}

XErrorTracker::~XErrorTracker ()
{
    tracker = NULL;
    XSetErrorHandler (handler);
}

int
XErrorTracker::handleXError (Display *dpy, XErrorEvent *ev)
{
    tracker->errorHandler (ev->error_code);
    return 0;
}

bool PixmapValid (Display *d, Pixmap p)
{
    Window root;
    unsigned int width, height, border, depth;
    int x, y;

    XErrorTracker tracker;

    EXPECT_CALL (tracker, errorHandler (BadDrawable)).Times (AtLeast (0));

    bool success = XGetGeometry (d, p, &root, &x, &y,
				 &width, &height, &border, &depth);

    return success;
}
}

void
DecorPixmapProtocol::SetUp ()
{
    xorg::testing::Test::SetUp ();

    XSelectInput (Display (),
		  DefaultRootWindow (Display ()),
		  StructureNotifyMask);

    deletePixmapMessage = XInternAtom (Display (), DECOR_DELETE_PIXMAP_ATOM_NAME, 0);
    pendingMessage = XInternAtom (Display (), DECOR_PIXMAP_PENDING_ATOM_NAME, 0);
}

class DeliveryMatcher :
    public ct::XEventMatcher
{
    public:

	DeliveryMatcher (Atom message,
			 const DecorPixmapProtocol::ClientMessageReceiver &receiver) :
	    mMessage (message),
	    mReceiver (receiver)
	{
	}

	bool MatchAndExplain (const XEvent &xce,
			      MatchResultListener *listener) const
	{
	    if (xce.xclient.message_type == mMessage)
	    {
		mReceiver (reinterpret_cast <const XClientMessageEvent &> (xce));
		return true;
	    }
	    else
		return false;
	}

	void DescribeTo (std::ostream *os) const
	{
	    *os << "Message delivered";
	}

    private:

	Atom mMessage;
	DecorPixmapProtocol::ClientMessageReceiver mReceiver;
};

void
DecorPixmapProtocol::WaitForClientMessageOnAndDeliverTo (Window client,
							 Atom   message,
							 const ClientMessageReceiver &receiver)
{
    ::Display *d = Display ();

    DeliveryMatcher delivery (message, receiver);
    ASSERT_TRUE (Advance (d, ct::WaitForEventOfTypeOnWindowMatching (d,
								     client,
								     ClientMessage,
								     -1,
								     -1,
								     delivery)));
}

TEST_F (DecorPixmapProtocol, PostDeleteCausesClientMessage)
{
    MockClientMessageReceiver receiver;
    ::Display *d = Display ();

    decor_post_delete_pixmap (d,
			      MOCK_WINDOW,
			      MOCK_PIXMAP);

    EXPECT_CALL (receiver, receiveMsg (_)).Times (1);

    WaitForClientMessageOnAndDeliverTo (MOCK_WINDOW,
					deletePixmapMessage,
					boost::bind (&MockClientMessageReceiver::receiveMsg,
						     &receiver,
						     _1));
}

TEST_F (DecorPixmapProtocol, PostDeleteDispatchesClientMessageToReceiver)
{
    MockProtocolDispatchFuncs dispatch;
    cdp::Communicator         communicator (pendingMessage,
					    deletePixmapMessage,
					    boost::bind (&MockProtocolDispatchFuncs::handlePending,
							 &dispatch,
							 _1,
							 _2),
					    boost::bind (&MockProtocolDispatchFuncs::handleUnused,
							 &dispatch,
							 _1,
							 _2));

    decor_post_delete_pixmap (Display (),
			      MOCK_WINDOW,
			      MOCK_PIXMAP);

    EXPECT_CALL (dispatch, handleUnused (MOCK_WINDOW, MOCK_PIXMAP)).Times (1);

    WaitForClientMessageOnAndDeliverTo (MOCK_WINDOW,
					deletePixmapMessage,
					boost::bind (&cdp::Communicator::handleClientMessage,
						     &communicator,
						     _1));
}

/* Test end to end. Post the delete message and cause the pixmap to be freed */

class DecorPixmapProtocolEndToEnd :
    public DecorPixmapProtocol,
    public DisplayFetch
{
    public:

	DecorPixmapProtocolEndToEnd () :
	    freePixmap (*this),
	    releasePool (new PixmapReleasePool (
			     boost::bind (&XFreePixmapWrapper::FreePixmap,
				      &freePixmap,
				      _1))),
	    pendingHandler (boost::bind (&MockFindRequestor::findRequestor,
					 &mockFindRequestor,
					 _1)),
	    unusedHandler (boost::bind (&MockFindList::findList,
					&mockFindList,
					_1),
			   releasePool,
			   boost::bind (&XFreePixmapWrapper::FreePixmap,
					&freePixmap,
					_1)),
	    stubDecoration (new StubDecoration ())
	{
	}

	void SetUp ()
	{
	    DecorPixmapProtocol::SetUp ();

	    communicator.reset (new cdp::Communicator (
				    pendingMessage,
				    deletePixmapMessage,
				    boost::bind (&cd::PendingHandler::handleMessage,
						 &pendingHandler,
						 _1,
						 _2),
				    boost::bind (&cd::UnusedHandler::handleMessage,
						 &unusedHandler,
						 _1,
						 _2)));
	}

	::Display *
	Display () const
	{
	    return DecorPixmapProtocol::Display ();
	}

	XFreePixmapWrapper       freePixmap;
	PixmapReleasePool::Ptr   releasePool;
	cd::PendingHandler       pendingHandler;
	cd::UnusedHandler        unusedHandler;
	boost::shared_ptr <cdp::Communicator> communicator;
	MockFindList             mockFindList;
	MockFindRequestor        mockFindRequestor;
	StubDecoration::Ptr      stubDecoration;
	MockDecorPixmapRequestor mockRequestor;
	MockDecorationListFindMatching mockFindMatching;

};

class DecorPixmapProtocolDeleteEndToEnd :
    public DecorPixmapProtocolEndToEnd
{
    public:

	void SetUp ()
	{
	    DecorPixmapProtocolEndToEnd::SetUp ();

	    ::Display *d = Display ();

	    pixmap = XCreatePixmap (d,
				   DefaultRootWindow (d),
				   1,
				   1,
				   DefaultDepth (d,
						 DefaultScreen (d)));

	    decor_post_delete_pixmap (d,
				      MOCK_WINDOW,
				      pixmap);
	}

	void TearDown ()
	{
	    if (PixmapValid (Display (), pixmap))
		XFreePixmap (Display (), pixmap);
	}

    protected:

	Pixmap                   pixmap;
};

TEST_F (DecorPixmapProtocolDeleteEndToEnd, TestFreeNotFoundWindowPixmapImmediately)
{
    ::Display *d = Display ();

    EXPECT_CALL (mockFindList, findList (MOCK_WINDOW)).WillOnce (ReturnNull ());

    /* Deliver it to the communicator */
    WaitForClientMessageOnAndDeliverTo (MOCK_WINDOW,
					deletePixmapMessage,
					boost::bind (&cdp::Communicator::handleClientMessage,
						     communicator.get (),
						     _1));

    /* Check if the pixmap is still valid */
    EXPECT_FALSE (PixmapValid (d, pixmap));
}

TEST_F (DecorPixmapProtocolDeleteEndToEnd, TestFreeUnusedPixmapImmediately)
{
    ::Display *d = Display ();

    EXPECT_CALL (mockFindMatching, findMatchingDecoration (pixmap)).WillOnce (Return (DecorationInterface::Ptr ()));
    EXPECT_CALL (mockFindList, findList (MOCK_WINDOW)).WillOnce (Return (&mockFindMatching));
    /* Deliver it to the communicator */
    WaitForClientMessageOnAndDeliverTo (MOCK_WINDOW,
					deletePixmapMessage,
					boost::bind (&cdp::Communicator::handleClientMessage,
						     communicator.get (),
						     _1));

    /* Check if the pixmap is still valid */
    EXPECT_FALSE (PixmapValid (d, pixmap));
}

TEST_F (DecorPixmapProtocolDeleteEndToEnd, TestQueuePixmapIfUsed)
{
    ::Display *d = Display ();

    EXPECT_CALL (mockFindMatching, findMatchingDecoration (pixmap)).WillOnce (Return (stubDecoration));
    EXPECT_CALL (mockFindList, findList (MOCK_WINDOW)).WillOnce (Return (&mockFindMatching));
    /* Deliver it to the communicator */
    WaitForClientMessageOnAndDeliverTo (MOCK_WINDOW,
					deletePixmapMessage,
					boost::bind (&cdp::Communicator::handleClientMessage,
						     communicator.get (),
						     _1));

    /* Check if the pixmap is still valid */
    EXPECT_TRUE (PixmapValid (d, pixmap));

    /* Call destroyUnusedPixmap on the release pool, it should release
     * the pixmap which was otherwise unused */
    releasePool->destroyUnusedPixmap (pixmap);

    /* Pixmap should now be invalid */
    EXPECT_FALSE (PixmapValid (d, pixmap));
}

class DecorPixmapProtocolPendingEndToEnd :
    public DecorPixmapProtocolEndToEnd
{
    public:

	DecorPixmapProtocolPendingEndToEnd () :
	    frameType (1),
	    frameState (2),
	    frameActions (3)
	{
	}

	void SetUp ()
	{
	    DecorPixmapProtocolEndToEnd::SetUp ();

	    ::Display *d = Display ();

	    decor_post_pending (d, MOCK_WINDOW, frameType, frameState, frameActions);
	}

    protected:

	unsigned int frameType, frameState, frameActions;
};

MATCHER_P3 (MatchArrayValues3, v1, v2, v3, "Matches three array values")
{
    return static_cast <unsigned int> (arg[0]) == v1 &&
	   static_cast <unsigned int> (arg[1]) == v2 &&
	   static_cast <unsigned int> (arg[2]) == v3;
}

TEST_F (DecorPixmapProtocolPendingEndToEnd, TestPostPendingMarksAsPendingOnClient)
{
    EXPECT_CALL (mockFindRequestor, findRequestor (MOCK_WINDOW)).WillOnce (Return (&mockRequestor));
    EXPECT_CALL (mockRequestor, handlePending (MatchArrayValues3 (frameType, frameState, frameActions)));

    /* Deliver it to the communicator */
    WaitForClientMessageOnAndDeliverTo (MOCK_WINDOW,
					pendingMessage,
					boost::bind (&cdp::Communicator::handleClientMessage,
						     communicator.get (),
						     _1));
}
