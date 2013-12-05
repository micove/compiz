/*
 * Copyright © 2012 Canonical Ltd.
 * Copyright © 2013 Sam Spilsbury <smspillaz@gmail.com>
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

#ifndef _COMPIZ_DECOR_PIXMAP_REQUESTS_MOCK_H
#define _COMPIZ_DECOR_PIXMAP_REQUESTS_MOCK_H

#include <gmock/gmock.h>
#include <boost/bind.hpp>

#include <X11/Xlib.h>
#include "pixmap-requests.h"

class MockDecorPixmapDeletor :
    public PixmapDestroyQueue
{
    public:

	MockDecorPixmapDeletor ();
	~MockDecorPixmapDeletor ();

	MOCK_METHOD1 (destroyUnusedPixmap, int (Pixmap p));
};

class MockDecorPixmapReceiver :
    public DecorPixmapReceiverInterface
{
    public:

	MockDecorPixmapReceiver ();
	~MockDecorPixmapReceiver ();

	MOCK_METHOD0 (pending, void ());
	MOCK_METHOD0 (update, void ());
};

class MockDecoration :
    public DecorationInterface
{
    public:

	MockDecoration ();
	~MockDecoration ();

	MOCK_METHOD0 (receiverInterface, DecorPixmapReceiverInterface & ());
	MOCK_CONST_METHOD0 (getFrameType, unsigned int ());
	MOCK_CONST_METHOD0 (getFrameState, unsigned int ());
	MOCK_CONST_METHOD0 (getFrameActions, unsigned int ());
};

class MockDecorationListFindMatching :
    public DecorationListFindMatchingInterface
{
    public:

	MockDecorationListFindMatching ();
	~MockDecorationListFindMatching ();

	MOCK_CONST_METHOD3 (findMatchingDecoration, DecorationInterface::Ptr (unsigned int, unsigned int, unsigned int));
	MOCK_CONST_METHOD1 (findMatchingDecoration, DecorationInterface::Ptr (Pixmap));
};

class MockDecorPixmapRequestor :
    public DecorPixmapRequestorInterface
{
    public:

	MockDecorPixmapRequestor ();
	~MockDecorPixmapRequestor ();

	MOCK_METHOD3 (postGenerateRequest, int (unsigned int, unsigned int, unsigned int));
	MOCK_METHOD1 (handlePending, void (const long *));
};

class XlibPixmapMock
{
    public:

	XlibPixmapMock ();
	~XlibPixmapMock ();

	MOCK_METHOD1(freePixmap, int (Pixmap));
};

class MockFindRequestor
{
    public:

	MockFindRequestor ();
	~MockFindRequestor ();

	MOCK_METHOD1 (findRequestor, DecorPixmapRequestorInterface * (Window));
};

class MockFindList
{
    public:

	MockFindList ();
	~MockFindList ();

	MOCK_METHOD1 (findList, DecorationListFindMatchingInterface * (Window));
};

class MockUnusedPixmapQueue :
    public UnusedPixmapQueue
{
    public:

	MockUnusedPixmapQueue ();
	~MockUnusedPixmapQueue ();

	typedef boost::shared_ptr <MockUnusedPixmapQueue> Ptr;

	MOCK_METHOD1 (markUnused, void (Pixmap));
};

class StubReceiver :
    public DecorPixmapReceiverInterface
{
    public:

	void pending () {}
	void update () {}
};

class StubDecoration :
    public DecorationInterface
{
    public:

	DecorPixmapReceiverInterface & receiverInterface ()
	{
	    return mReceiver;
	}

	unsigned int getFrameType  () const { return 0; }
	unsigned int getFrameState () const { return 0; }
	unsigned int getFrameActions () const { return 0; }

    private:

	StubReceiver mReceiver;
};

class MockProtocolDispatchFuncs
{
    public:

	MockProtocolDispatchFuncs ();
	~MockProtocolDispatchFuncs ();

	MOCK_METHOD2 (handlePending, void (Window, const long *));
	MOCK_METHOD2 (handleUnused, void (Window, Pixmap));
};

#endif
