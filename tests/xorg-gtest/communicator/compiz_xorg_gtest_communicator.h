/*
* Copyright © 2013 Sam Spilsbury
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
* Author: Sam Spilsbury <smspillaz@gmail.com>
*/
#ifndef _COMPIZ_XORG_GTEST_COMMUNICATOR_H
#define _COMPIZ_XORG_GTEST_COMMUNICATOR_H

#include <memory>
#include <vector>

namespace compiz
{
namespace testing
{
namespace messages
{
extern const char *TEST_HELPER_READY_MSG;
extern const char *TEST_HELPER_REGISTER_CLIENT;
extern const char *TEST_HELPER_LOCK_CONFIGURE_REQUESTS;
extern const char *TEST_HELPER_CHANGE_FRAME_EXTENTS;
extern const char *TEST_HELPER_FRAME_EXTENTS_CHANGED;
extern const char *TEST_HELPER_CONFIGURE_WINDOW;
extern const char *TEST_HELPER_WINDOW_CONFIGURE_PROCESSED;
extern const char *TEST_HELPER_DESTROY_ON_REPARENT;
extern const char *TEST_HELPER_RESTACK_ATLEAST_ABOVE;
extern const char *TEST_HELPER_WINDOW_READY;
}


class MessageAtoms
{
    public:

	MessageAtoms (Display *);

	Atom FetchForString (const char *string);

    private:

	class Private;
	std::auto_ptr <Private> priv;
};

bool ReceiveMessage (Display *, Atom, XEvent &, int timeout = -1);
void SendClientMessage (Display *, Atom, Window, Window, const std::vector <long> &data);
}
}

#endif
