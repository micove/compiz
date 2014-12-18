/*
 * Copyright © 2011 NVIDIA Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * NVIDIA Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * NVIDIA Corporation makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NVIDIA CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: James Jones <jajones@nvidia.com>
 */

#ifndef _GLXTOGLSYNC_H
#define _GLXTOGLSYNC_H

#include <composite/composite.h>
#include <opengl/opengl.h>
#include <X11/extensions/sync.h>

#include "opengl/opengl.h"

/**
 * Class that manages an XFenceSync wrapped in a GLsync object.
 *
 * Can be used to synchronize operations in the GL command stream
 * with operations in the X command stream.
 */
class XToGLSync {
    public:
	XToGLSync ();
	~XToGLSync ();

	XSyncAlarm alarm (void) const { return a; }
	bool isReady (void) const { return state == XTOGLS_READY; }

	/**
	 * Sends the trigger request to the server. The fence will be signaled
	 * after all rendering has completed.
	 */
	void trigger (void);

	/**
	 * Calls glWaitSync. Any OpenGL commands after this will wait for the
	 * fence to be signaled.
	 */
	void insertWait (void);

	/**
	 * Blocks until the fence is signaled, or until a timeout expires.
	 *
	 * \param The maximum time to wait, in nanoseconds.
	 * \return One of \c GL_ALREADY_SIGNALED, \c GL_CONDITION_SATISFIED,
	 *	\c GL_TIMEOUT_EXPIRED, or \c GL_WAIT_FAILED.
	 */
	GLenum checkUpdateFinished (GLuint64 timeout);

	/**
	 * Resets the fence.
	 */
	void reset (void);
	void handleEvent (XSyncAlarmNotifyEvent *ev);

    private:
	XSyncFence f;
	GLsync fGL;

	XSyncCounter c;
	XSyncAlarm a;
	XSyncValue nextCounterValue;

	enum {
	    XTOGLS_READY,
	    XTOGLS_TRIGGER_SENT,
	    XTOGLS_WAITING,
	    XTOGLS_DONE,
	    XTOGLS_RESET_PENDING,
	} state;

	static bool syncValuesInitialized;
	static XSyncValue zero;
	static XSyncValue one;

	static Bool alarmEventPredicate (Display *dpy, XEvent *ev, XPointer arg);
};

#endif
