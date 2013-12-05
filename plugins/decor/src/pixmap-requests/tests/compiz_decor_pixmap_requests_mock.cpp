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

#include "compiz_decor_pixmap_requests_mock.h"

/* These exist purely because the implicit details of the
 * constructors of the MOCK_METHODs are quite complex. We can
 * save compilation time by generating those constructors once,
 * and linking, rather than generating multiple definitions and
 * having the linker eliminate them */

MockDecorPixmapDeletor::MockDecorPixmapDeletor ()
{
}

MockDecorPixmapDeletor::~MockDecorPixmapDeletor ()
{
}

MockDecorPixmapReceiver::MockDecorPixmapReceiver ()
{
}

MockDecorPixmapReceiver::~MockDecorPixmapReceiver ()
{
}

MockDecoration::MockDecoration ()
{
}

MockDecoration::~MockDecoration ()
{
}

MockDecorationListFindMatching::MockDecorationListFindMatching ()
{
}

MockDecorationListFindMatching::~MockDecorationListFindMatching ()
{
}

MockDecorPixmapRequestor::MockDecorPixmapRequestor ()
{
}

MockDecorPixmapRequestor::~MockDecorPixmapRequestor ()
{
}

XlibPixmapMock::XlibPixmapMock ()
{
}

XlibPixmapMock::~XlibPixmapMock ()
{
}

MockFindRequestor::MockFindRequestor ()
{
}

MockFindRequestor::~MockFindRequestor ()
{
}

MockFindList::MockFindList ()
{
}

MockFindList::~MockFindList ()
{
}

MockUnusedPixmapQueue::MockUnusedPixmapQueue ()
{
}

MockUnusedPixmapQueue::~MockUnusedPixmapQueue ()
{
}

MockProtocolDispatchFuncs::MockProtocolDispatchFuncs ()
{
}

MockProtocolDispatchFuncs::~MockProtocolDispatchFuncs ()
{
}
