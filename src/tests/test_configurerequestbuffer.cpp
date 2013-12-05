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
#include <deque>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <X11/Xlib.h>

#include "configurerequestbuffer-impl.h"
#include "asyncserverwindow.h"

namespace crb = compiz::window::configure_buffers;
namespace cw = compiz::window;

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::Invoke;
using testing::WithArgs;
using testing::SetArgReferee;
using testing::DoAll;
using testing::InSequence;
using testing::ReturnNull;
using testing::IsNull;

class MockAsyncServerWindow :
    public cw::AsyncServerWindow
{
    public:

	MOCK_METHOD2 (requestConfigureOnClient, int (const XWindowChanges &, unsigned int));
	MOCK_METHOD2 (requestConfigureOnFrame, int (const XWindowChanges &, unsigned int));
	MOCK_METHOD2 (requestConfigureOnWrapper, int (const XWindowChanges &, unsigned int));
	MOCK_METHOD0 (sendSyntheticConfigureNotify, void ());
	MOCK_CONST_METHOD0 (hasCustomShape, bool ());
};

class MockSyncServerWindow :
    public cw::SyncServerWindow
{
    public:

	MOCK_METHOD1 (queryAttributes, bool (XWindowAttributes &));
	MOCK_METHOD1 (queryFrameAttributes, bool (XWindowAttributes &));
	MOCK_METHOD3 (queryShapeRectangles, XRectangle * (int, int *, int *));
};

namespace
{
int REQUEST_X = 1;
int REQUEST_Y = 2;
int REQUEST_WIDTH = 3;
int REQUEST_HEIGHT = 4;
int REQUEST_BORDER = 5;

Window REQUEST_ABOVE = 6;
unsigned int REQUEST_MODE = 7;

crb::BufferLock::Ptr
CreateNormalLock (crb::CountedFreeze *cf)
{
    return boost::make_shared <crb::ConfigureBufferLock> (cf);
}

}

MATCHER_P2 (MaskXWC, xwc, vm, "Matches XWindowChanges")
{
    if (vm & CWX)
	if (xwc.x != arg.x)
	    return false;

    if (vm & CWY)
	if (xwc.y != arg.y)
	    return false;

    if (vm & CWWidth)
	if (xwc.width != arg.width)
	    return false;

    if (vm & CWHeight)
	if (xwc.height != arg.height)
	    return false;

    if (vm & CWBorderWidth)
	if (xwc.border_width != arg.border_width)
	    return false;

    if (vm & CWStackMode)
	if (xwc.stack_mode != arg.stack_mode)
	    return false;

    if (vm & CWSibling)
	if (xwc.sibling != arg.sibling)
	    return false;

    return true;
}

class ConfigureRequestBuffer :
    public testing::Test
{
    public:

	ConfigureRequestBuffer ()
	{
	    /* Initialize xwc, we control it
	     * through the value masks */
	    xwc.x = REQUEST_X;
	    xwc.y = REQUEST_Y;
	    xwc.width = REQUEST_WIDTH;
	    xwc.height = REQUEST_HEIGHT;
	    xwc.border_width = REQUEST_BORDER;
	    xwc.sibling = REQUEST_ABOVE;
	    xwc.stack_mode = REQUEST_MODE;
	}

    protected:

	XWindowChanges        xwc;
	MockAsyncServerWindow asyncServerWindow;
	MockSyncServerWindow  syncServerWindow;
};

class ConfigureRequestBufferDispatch :
    public ConfigureRequestBuffer
{
    protected:

	ConfigureRequestBufferDispatch () :
	    ConfigureRequestBuffer (),
	    factory (boost::bind (CreateNormalLock, _1)),
	    buffer (
		crb::ConfigureRequestBuffer::Create (&asyncServerWindow,
						     &syncServerWindow,
						     factory))
	{
	}

	crb::ConfigureRequestBuffer::LockFactory factory;
	crb::Buffer::Ptr buffer;
};

TEST_F (ConfigureRequestBufferDispatch, PushDirectSyntheticConfigureNotify)
{
    EXPECT_CALL (asyncServerWindow, sendSyntheticConfigureNotify ());

    buffer->pushSyntheticConfigureNotify ();
}

TEST_F (ConfigureRequestBufferDispatch, PushDirectClientUpdate)
{
    unsigned int   valueMask = CWX | CWY | CWBorderWidth |
			       CWSibling | CWStackMode;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnClient (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer->pushClientRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferDispatch, PushDirectWrapperUpdate)
{
    unsigned int   valueMask = CWX | CWY | CWBorderWidth |
			       CWSibling | CWStackMode;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnWrapper (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer->pushWrapperRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferDispatch, PushDirectFrameUpdate)
{
    unsigned int   valueMask = CWX | CWY | CWBorderWidth |
			       CWSibling | CWStackMode;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer->pushFrameRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferDispatch, PushUpdateLocked)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = 0;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferDispatch, PushCombinedUpdateLocked)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWX;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    valueMask |= CWY;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (MaskXWC (xwc, valueMask),
					       valueMask));

    lock->release ();
}

/*
 * This test is disabled until we can expose the QueryShapeRectangles API
 * to plugins
 */
TEST_F (ConfigureRequestBufferDispatch, DISABLED_PushUpdateLockedReleaseInOrder)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWX | CWY;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);
    EXPECT_CALL (asyncServerWindow, requestConfigureOnWrapper (_, _)).Times (0);
    EXPECT_CALL (asyncServerWindow, requestConfigureOnClient (_, _)).Times (0);

    buffer->pushClientRequest (xwc, valueMask);
    buffer->pushWrapperRequest (xwc, 0);
    buffer->pushFrameRequest (xwc, 0);

    InSequence s;

    /* Always frame -> wrapper -> client */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (MaskXWC (xwc, valueMask),
					       valueMask));
    EXPECT_CALL (asyncServerWindow, requestConfigureOnWrapper (MaskXWC (xwc, valueMask),
					       valueMask));
    EXPECT_CALL (asyncServerWindow, requestConfigureOnClient (MaskXWC (xwc, valueMask),
					       valueMask));

    lock->release ();
}

TEST_F (ConfigureRequestBufferDispatch, UnlockBuffer)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWX | CWY;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (MaskXWC (xwc, valueMask),
					       valueMask));

    lock->release ();
}

TEST_F (ConfigureRequestBufferDispatch, ImplicitUnlockBuffer)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWX | CWY;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (MaskXWC (xwc, valueMask),
					       valueMask));
}

TEST_F (ConfigureRequestBufferDispatch, ForceImmediateConfigureOnRestack)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWStackMode | CWSibling;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer->pushFrameRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferDispatch, ForceImmediateConfigureOnWindowSizeChange)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWWidth | CWHeight | CWBorderWidth;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer->pushFrameRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferDispatch, ForceImmediateConfigureOnClientReposition)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWX | CWY;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnClient (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer->pushClientRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferDispatch, ForceImmediateConfigureOnWrapperReposition)
{
    crb::Releasable::Ptr lock (buffer->obtainLock ());

    unsigned int   valueMask = CWX | CWY;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnWrapper (MaskXWC (xwc, valueMask),
					       valueMask));

    buffer->pushWrapperRequest (xwc, valueMask);
}

namespace
{
class MockLock :
    public crb::BufferLock
{
    public:

	/* We're currently importing the locks statefulness and coupling
	 * the caller with that */
	MockLock () :
	    armed (false)
	{
	    ON_CALL (*this, lock ()).WillByDefault (
			Invoke (this, &MockLock::FreezeIfUnarmed));
	    ON_CALL (*this, release ()).WillByDefault (
			Invoke (this, &MockLock::ReleaseIfArmed));
	}

	void OperateOver (crb::CountedFreeze *cf)
	{
	    countedFreeze = cf;
	}

	void FreezeIfUnarmed ()
	{
	    if (!armed)
	    {
		countedFreeze->freeze ();
		armed = true;
	    }
	}

	void ReleaseIfArmed ()
	{
	    if (armed)
	    {
		countedFreeze->release ();
		armed = false;
	    }
	}

	typedef boost::shared_ptr <MockLock> Ptr;

	MOCK_METHOD0 (lock, void ());
	MOCK_METHOD0 (release, void ());

    private:

	crb::CountedFreeze *countedFreeze;
	bool               armed;
};

class MockLockFactory
{
    public:

	crb::BufferLock::Ptr
	CreateMockLock (crb::CountedFreeze  *cf)
	{
	    MockLock::Ptr mockLock (locks.front ());
	    mockLock->OperateOver (cf);

	    locks.pop_front ();

	    return mockLock;
	}

	void
	QueueLockForCreation (const MockLock::Ptr &lock)
	{
	    locks.push_back (lock);
	}

    private:

	std::deque <MockLock::Ptr> locks;
};
}

class ConfigureRequestBufferLockBehaviour :
    public ConfigureRequestBuffer
{
    public:

	ConfigureRequestBufferLockBehaviour () :
	    ConfigureRequestBuffer (),
	    lock (boost::make_shared <MockLock> ()),
	    factory (
		boost::bind (&MockLockFactory::CreateMockLock,
			     &mockLockFactory,
			     _1)),
	    buffer (
		crb::ConfigureRequestBuffer::Create (
		    &asyncServerWindow,
		    &syncServerWindow,
		    factory))

	{
	    mockLockFactory.QueueLockForCreation (lock);
	}

    protected:

	typedef NiceMock <MockAsyncServerWindow> NiceServerWindow;
	typedef crb::ConfigureRequestBuffer::LockFactory LockFactory;

	MockLock::Ptr    lock;
	MockLockFactory  mockLockFactory;

	LockFactory      factory;
	crb::Buffer::Ptr buffer;
};

TEST_F (ConfigureRequestBufferLockBehaviour, RearmBufferLockOnRelease)
{
    EXPECT_CALL (*lock, lock ());
    crb::Releasable::Ptr releasable (buffer->obtainLock ());

    unsigned int   valueMask = CWX | CWY;

    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* Now the buffer will dispatch is configure request */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _));

    /* Rearm locks on release */
    EXPECT_CALL (*lock, lock ());

    /* Directly release the queue */
    releasable->release ();
}

TEST_F (ConfigureRequestBufferLockBehaviour, NoRearmBufferLockNoReleaseRequired)
{
    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());
    crb::Releasable::Ptr releasable (buffer->obtainLock ());

    /* No call to requestConfigureOnFrame if there's nothing to be configured */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* No rearm - we haven't released the whole buffer */
    EXPECT_CALL (*lock, lock ()).Times (0);

    /* Directly release the queue */
    releasable->release ();
}

TEST_F (ConfigureRequestBufferLockBehaviour, RearmWhenPushReady)
{
    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());
    crb::Releasable::Ptr releasable (buffer->obtainLock ());

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* No call to requestConfigureOnFrame if there's nothing to be configured */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    /* No rearm - we haven't released it */
    EXPECT_CALL (*lock, lock ()).Times (0);

    /* Directly release the queue */
    releasable->release ();

    /* Since we're now going to push something to a queue
     * that's effectively not locked, the locks should now
     * be released */
    unsigned int   valueMask = CWX | CWY;

    /* Now rearm it */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _));
    EXPECT_CALL (*lock, lock ());

    buffer->pushFrameRequest (xwc, valueMask);
}

TEST_F (ConfigureRequestBufferLockBehaviour, NoRearmBufferLockOnNoRelease)
{
    MockLock::Ptr    second (boost::make_shared <MockLock> ());
    mockLockFactory.QueueLockForCreation (second);

    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());
    EXPECT_CALL (*second, lock ());

    crb::Releasable::Ptr releasable (buffer->obtainLock ());
    crb::Releasable::Ptr otherReleasable (buffer->obtainLock ());

    /* We are releasing this lock */
    EXPECT_CALL (*lock, release ());

    /* No call to requestConfigureOnFrame if there's nothing to be configured */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    /* No rearm - we haven't released it */
    EXPECT_CALL (*lock, lock ()).Times (0);

    releasable->release ();
}

TEST_F (ConfigureRequestBufferLockBehaviour, QueryAttributesDispatchAndRearm)
{
    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());

    crb::Releasable::Ptr releasable (buffer->obtainLock ());

    unsigned int valueMask = CWX | CWY;

    /* Queue locked */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    /* Queue forceably unlocked, locks rearmed */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _));
    EXPECT_CALL (*lock, lock ());

    /* Expect a call to XGetWindowAttributes */
    EXPECT_CALL (syncServerWindow, queryShapeRectangles (_, _, _))
	    .WillOnce (
		ReturnNull ());

    int a, b;

    EXPECT_THAT (buffer->queryShapeRectangles (0, &a, &b), IsNull ());
}

TEST_F (ConfigureRequestBufferLockBehaviour, QueryFrameAttributesDispatchAndRearm)
{
    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());

    crb::Releasable::Ptr releasable (buffer->obtainLock ());

    unsigned int valueMask = CWX | CWY;

    /* Queue locked */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    /* Queue forceably unlocked, locks rearmed */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _));
    EXPECT_CALL (*lock, lock ());

    /* Expect a call to XGetWindowAttributes */
    XWindowAttributes xwa;
    EXPECT_CALL (syncServerWindow, queryFrameAttributes (_))
	    .WillOnce (
		DoAll (
		    SetArgReferee <0> (xwa),
		    Return (true)));

    buffer->queryFrameAttributes (xwa);
}

TEST_F (ConfigureRequestBufferLockBehaviour, QueryShapeRectanglesDispatchAndRearm)
{
    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());

    crb::Releasable::Ptr releasable (buffer->obtainLock ());

    unsigned int valueMask = CWX | CWY;

    /* Queue locked */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    /* Queue forceably unlocked, locks rearmed */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _));
    EXPECT_CALL (*lock, lock ());

    /* Expect a call to XGetWindowAttributes */
    XWindowAttributes xwa;
    EXPECT_CALL (syncServerWindow, queryFrameAttributes (_))
	    .WillOnce (
		DoAll (
		    SetArgReferee <0> (xwa),
		    Return (true)));

    buffer->queryFrameAttributes (xwa);
}

TEST_F (ConfigureRequestBufferLockBehaviour, ForceReleaseDispatchAndRearm)
{
    /* Locks get armed on construction */
    EXPECT_CALL (*lock, lock ());

    crb::Releasable::Ptr releasable (buffer->obtainLock ());

    unsigned int valueMask = CWX | CWY;

    /* Queue locked */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _)).Times (0);

    buffer->pushFrameRequest (xwc, valueMask);

    /* Queue forceably unlocked, locks rearmed */
    EXPECT_CALL (asyncServerWindow, requestConfigureOnFrame (_, _));
    EXPECT_CALL (*lock, lock ());

    /* Force release */
    buffer->forceRelease ();
}

