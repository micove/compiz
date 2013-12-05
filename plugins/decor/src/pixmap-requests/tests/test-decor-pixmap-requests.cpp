/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Sam Spilsbury <sam.spilsbury@canonical.com>
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/bind.hpp>
#include <iostream>

#include <X11/Xlib.h>
#include "pixmap-requests.h"
#include "compiz_decor_pixmap_requests_mock.h"

using ::testing::AtLeast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::IsNull;
using ::testing::_;

namespace cd = compiz::decor;
namespace cdp = compiz::decor::protocol;

TEST(DecorPixmapRequestsTest, TestDestroyPixmapDeletes)
{
    boost::shared_ptr <MockDecorPixmapDeletor> mockDeletor = boost::make_shared <MockDecorPixmapDeletor> ();
    DecorPixmap pm (1, mockDeletor);

    EXPECT_CALL (*(mockDeletor.get ()), destroyUnusedPixmap (1)).WillOnce (Return (1));
}

TEST(DecorPixmapRequestsTest, TestPendingGeneratesRequest)
{
    MockDecorPixmapRequestor mockRequestor;
    MockDecoration           mockDecoration;
    X11DecorPixmapReceiver   receiver (&mockRequestor, &mockDecoration);

    EXPECT_CALL (mockDecoration, getFrameActions ()).WillOnce (Return (3));
    EXPECT_CALL (mockDecoration, getFrameState ()).WillOnce (Return (2));
    EXPECT_CALL (mockDecoration, getFrameType ()).WillOnce (Return (1));

    EXPECT_CALL (mockRequestor, postGenerateRequest (1, 2, 3));

    receiver.pending ();
}

TEST(DecorPixmapRequestsTest, TestPendingGeneratesOnlyOneRequest)
{
    MockDecorPixmapRequestor mockRequestor;
    MockDecoration           mockDecoration;
    X11DecorPixmapReceiver   receiver (&mockRequestor, &mockDecoration);

    EXPECT_CALL (mockDecoration, getFrameActions ()).WillOnce (Return (3));
    EXPECT_CALL (mockDecoration, getFrameState ()).WillOnce (Return (2));
    EXPECT_CALL (mockDecoration, getFrameType ()).WillOnce (Return (1));

    EXPECT_CALL (mockRequestor, postGenerateRequest (1, 2, 3));

    receiver.pending ();
    receiver.pending ();
}

TEST(DecorPixmapRequestsTest, TestUpdateGeneratesRequestIfNewOnePending)
{
    MockDecorPixmapRequestor mockRequestor;
    MockDecoration           mockDecoration;
    X11DecorPixmapReceiver   receiver (&mockRequestor, &mockDecoration);

    EXPECT_CALL (mockDecoration, getFrameActions ()).WillOnce (Return (3));
    EXPECT_CALL (mockDecoration, getFrameState ()).WillOnce (Return (2));
    EXPECT_CALL (mockDecoration, getFrameType ()).WillOnce (Return (1));

    EXPECT_CALL (mockRequestor, postGenerateRequest (1, 2, 3));

    receiver.pending ();
    receiver.pending ();

    EXPECT_CALL (mockDecoration, getFrameActions ()).WillOnce (Return (3));
    EXPECT_CALL (mockDecoration, getFrameState ()).WillOnce (Return (2));
    EXPECT_CALL (mockDecoration, getFrameType ()).WillOnce (Return (1));

    EXPECT_CALL (mockRequestor, postGenerateRequest (1, 2, 3));

    receiver.update ();
}

TEST(DecorPixmapRequestsTest, TestUpdateGeneratesNoRequestIfNoNewOnePending)
{
    MockDecorPixmapRequestor mockRequestor;
    MockDecoration           mockDecoration;
    X11DecorPixmapReceiver   receiver (&mockRequestor, &mockDecoration);

    EXPECT_CALL (mockDecoration, getFrameActions ()).WillOnce (Return (3));
    EXPECT_CALL (mockDecoration, getFrameState ()).WillOnce (Return (2));
    EXPECT_CALL (mockDecoration, getFrameType ()).WillOnce (Return (1));

    EXPECT_CALL (mockRequestor, postGenerateRequest (1, 2, 3));

    receiver.pending ();
    receiver.update ();
}

class DecorPixmapReleasePool :
    public ::testing::Test
{
    public:

	DecorPixmapReleasePool () :
	    mockFreeFunc (boost::bind (&XlibPixmapMock::freePixmap,
				       &xlibPixmapMock,
				       _1)),
	    releasePool (mockFreeFunc)
	{
	}

	XlibPixmapMock xlibPixmapMock;
	PixmapReleasePool::FreePixmapFunc mockFreeFunc;

	PixmapReleasePool releasePool;
};

TEST_F (DecorPixmapReleasePool, MarkUnusedNoFree)
{
    /* Never free pixmaps on markUnused */

    EXPECT_CALL (xlibPixmapMock, freePixmap (_)).Times (0);

    releasePool.markUnused (static_cast <Pixmap> (1));
}

TEST_F (DecorPixmapReleasePool, NoFreeOnPostDeleteIfNotInList)
{
    EXPECT_CALL (xlibPixmapMock, freePixmap (_)).Times (0);

    releasePool.destroyUnusedPixmap (static_cast <Pixmap> (1));
}

TEST_F (DecorPixmapReleasePool, FreeOnPostDeleteIfMarkedUnused)
{
    EXPECT_CALL (xlibPixmapMock, freePixmap (1)).Times (1);

    releasePool.markUnused (static_cast <Pixmap> (1));
    releasePool.destroyUnusedPixmap (static_cast <Pixmap> (1));
}

TEST_F (DecorPixmapReleasePool, FreeOnPostDeleteIfMarkedUnusedOnceOnly)
{
    EXPECT_CALL (xlibPixmapMock, freePixmap (1)).Times (1);

    releasePool.markUnused (static_cast <Pixmap> (1));
    releasePool.destroyUnusedPixmap (static_cast <Pixmap> (1));
    releasePool.destroyUnusedPixmap (static_cast <Pixmap> (1));
}

TEST_F (DecorPixmapReleasePool, UnusedUniqueness)
{
    EXPECT_CALL (xlibPixmapMock, freePixmap (1)).Times (1);

    releasePool.markUnused (static_cast <Pixmap> (1));
    releasePool.markUnused (static_cast <Pixmap> (1));
    releasePool.destroyUnusedPixmap (static_cast <Pixmap> (1));
    releasePool.destroyUnusedPixmap (static_cast <Pixmap> (1));
}

class DecorPendingMessageHandler :
    public ::testing::Test
{
    public:

	DecorPendingMessageHandler () :
	    pendingHandler (boost::bind (&MockFindRequestor::findRequestor,
					 &mockRequestorFind,
					 _1))
	{
	}

	MockFindRequestor              mockRequestorFind;
	MockDecorPixmapRequestor       mockRequestor;

	cd::PendingHandler             pendingHandler;
};

namespace
{
Window mockWindow = 1;
}

TEST_F (DecorPendingMessageHandler, NoPendingIfNotFound)
{
    EXPECT_CALL (mockRequestor, handlePending (_)).Times (0);
    EXPECT_CALL (mockRequestorFind, findRequestor (mockWindow)).WillOnce (ReturnNull ());

    long data = 1;
    pendingHandler.handleMessage (mockWindow, &data);
}

TEST_F (DecorPendingMessageHandler, PendingIfFound)
{
    long data = 1;

    EXPECT_CALL (mockRequestor, handlePending (Pointee (data)));
    EXPECT_CALL (mockRequestorFind, findRequestor (mockWindow)).WillOnce (Return (&mockRequestor));

    pendingHandler.handleMessage (mockWindow, &data);
}

class DecorUnusedMessageHandler :
    public ::testing::Test
{
    public:

	DecorUnusedMessageHandler () :
	    mockUnusedPixmapQueue (new MockUnusedPixmapQueue ()),
	    unusedHandler (boost::bind (&MockFindList::findList,
					&mockListFind,
					_1),
			   mockUnusedPixmapQueue,
			   boost::bind (&XlibPixmapMock::freePixmap,
					&xlibPixmapMock,
					_1))
	{
	}

	MockFindList mockListFind;
	MockDecorationListFindMatching mockListFinder;
	MockUnusedPixmapQueue::Ptr     mockUnusedPixmapQueue;
	XlibPixmapMock                 xlibPixmapMock;

	cd::UnusedHandler unusedHandler;
};

namespace
{
Pixmap mockPixmap = 2;
}

TEST_F (DecorUnusedMessageHandler, FreeImmediatelyWindowNotFound)
{
    /* Don't verify calls to mockListFind */
    EXPECT_CALL (mockListFind, findList (_)).Times (AtLeast (0));

    /* Just free the pixmap immediately if no window was found */
    EXPECT_CALL (xlibPixmapMock, freePixmap (mockPixmap)).Times (1);

    ON_CALL (mockListFind, findList (mockWindow)).WillByDefault (ReturnNull ());
    unusedHandler.handleMessage (mockWindow, mockPixmap);
}

TEST_F (DecorUnusedMessageHandler, FreeImmediatelyIfNoDecorationFound)
{
    /* Don't verify calls to mockListFind or mockListFinder */
    EXPECT_CALL (mockListFind, findList (_)).Times (AtLeast (0));
    EXPECT_CALL (mockListFinder, findMatchingDecoration (_)).Times (AtLeast (0));

    EXPECT_CALL (xlibPixmapMock, freePixmap (mockPixmap)).Times (1);

    ON_CALL (mockListFind, findList (mockWindow))
	.WillByDefault (Return (&mockListFinder));
    ON_CALL (mockListFinder, findMatchingDecoration (mockPixmap))
	.WillByDefault (Return (DecorationInterface::Ptr ()));

    unusedHandler.handleMessage (mockWindow, mockPixmap);
}

TEST_F (DecorUnusedMessageHandler, AddToQueueIfInUse)
{
    /* Don't verify calls to mockListFind or mockListFinder */
    EXPECT_CALL (mockListFind, findList (_)).Times (AtLeast (0));
    EXPECT_CALL (mockListFinder, findMatchingDecoration (_)).Times (AtLeast (0));

    DecorationInterface::Ptr mockDecoration (new StubDecoration ());

    /* Do not immediately free the pixmap */
    EXPECT_CALL (xlibPixmapMock, freePixmap (mockPixmap)).Times (0);
    EXPECT_CALL (*mockUnusedPixmapQueue, markUnused (mockPixmap)).Times (1);

    ON_CALL (mockListFind, findList (mockWindow))
	.WillByDefault (Return (&mockListFinder));
    ON_CALL (mockListFinder, findMatchingDecoration (mockPixmap))
	.WillByDefault (Return (mockDecoration));

    unusedHandler.handleMessage (mockWindow, mockPixmap);
}

namespace
{
Atom pendingMsg = 3;
Atom unusedMsg = 4;
}

class DecorProtocolCommunicator :
    public ::testing::Test
{
    public:

	DecorProtocolCommunicator () :
	    handlePendingFunc (boost::bind (&MockProtocolDispatchFuncs::handlePending,
					    &mockProtoDispatch,
					    _1,
					    _2)),
	    handleUnusedFunc (boost::bind (&MockProtocolDispatchFuncs::handleUnused,
					   &mockProtoDispatch,
					   _1,
					   _2)),
	    protocolCommunicator (pendingMsg,
				  unusedMsg,
				  handlePendingFunc,
				  handleUnusedFunc)
	{
	}

	void ClientMessageData (XClientMessageEvent &msg,
				Window              window,
				Atom                atom,
				long                l1,
				long                l2,
				long                l3,
				long                l4)
	{
	    msg.window = window;
	    msg.message_type = atom;
	    msg.data.l[0] = l1;
	    msg.data.l[1] = l2;
	    msg.data.l[2] = l3;
	    msg.data.l[3] = l4;
	}

	MockProtocolDispatchFuncs mockProtoDispatch;
	cdp::PendingMessage       handlePendingFunc;
	cdp::PixmapUnusedMessage  handleUnusedFunc;

	cdp::Communicator protocolCommunicator;
};

MATCHER_P (MatchArray3, v, "Contains values")
{
    return arg[0] == v[0] &&
	    arg[1] == v[1] &&
	    arg[2] == v[2];
}

TEST_F (DecorProtocolCommunicator, TestDispatchToPendingHandler)
{
    long data[3];

    data[0] = 1;
    data[1] = 2;
    data[2] = 3;

    XClientMessageEvent ev;
    ClientMessageData (ev,
		       mockWindow,
		       pendingMsg,
		       data[0],
		       data[1],
		       data[2],
		       0);

    EXPECT_CALL (mockProtoDispatch, handlePending (mockWindow,
						   MatchArray3 (data)))
	.Times (1);

    protocolCommunicator.handleClientMessage (ev);
}

TEST_F (DecorProtocolCommunicator, TestDispatchToUnusedHandler)
{
    XClientMessageEvent ev;
    ClientMessageData (ev,
		       mockWindow,
		       unusedMsg,
		       mockPixmap,
		       0,
		       0,
		       0);

    EXPECT_CALL (mockProtoDispatch, handleUnused (mockWindow,
						  mockPixmap))
	.Times (1);

    protocolCommunicator.handleClientMessage (ev);
}
