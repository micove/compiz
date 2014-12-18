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

#include "opengl/xtoglsync.h"

bool XToGLSync::syncValuesInitialized = false;
XSyncValue XToGLSync::zero;
XSyncValue XToGLSync::one;

XToGLSync::XToGLSync () :
    f (None),
    fGL (NULL),
    c (None),
    a (None),
    state (XToGLSync::XTOGLS_READY)
{
    Display *dpy = screen->dpy ();

    f = XSyncCreateFence (dpy, DefaultRootWindow (dpy), False);
    fGL = GL::importSync (GL_SYNC_X11_FENCE_EXT, f, 0);

    if (!syncValuesInitialized)
    {
	XSyncIntToValue (&zero, 0);
	XSyncIntToValue (&one, 1);
	syncValuesInitialized = true;
    }

    XSyncIntToValue (&nextCounterValue, 1);
    c = XSyncCreateCounter (dpy, zero);

    XSyncAlarmAttributes alarmAttribs;
    alarmAttribs.trigger.counter = c;
    alarmAttribs.trigger.value_type = XSyncAbsolute;
    alarmAttribs.trigger.wait_value = one;
    alarmAttribs.trigger.test_type = XSyncPositiveTransition;
    alarmAttribs.events = True;
    a = XSyncCreateAlarm (dpy,
			  XSyncCACounter |
			  XSyncCAValueType |
			  XSyncCAValue |
			  XSyncCATestType |
			  XSyncCAEvents,
			  &alarmAttribs);

}

XToGLSync::~XToGLSync ()
{
    Display *dpy = screen->dpy ();

    switch (state) {
    case XTOGLS_WAITING:
	break;

    case XTOGLS_RESET_PENDING:
	{
	    XEvent ev;

	    XIfEvent (dpy, &ev, &XToGLSync::alarmEventPredicate,
		      reinterpret_cast<XPointer>(this));
	    handleEvent(reinterpret_cast<XSyncAlarmNotifyEvent*>(&ev));
	}
	// Fall through.
    case XTOGLS_READY:
	XSyncTriggerFence (dpy, f);
	XFlush (dpy);
	break;

    case XTOGLS_TRIGGER_SENT:
    case XTOGLS_DONE:
	// Nothing to do.
	break;
    }

    GL::deleteSync (fGL);
    XSyncDestroyFence (dpy, f);
    XSyncDestroyCounter (dpy, c);
    XSyncDestroyAlarm (dpy, a);
}

Bool XToGLSync::alarmEventPredicate (Display *dpy, XEvent *ev, XPointer arg)
{
    if (ev->type == screen->syncEvent () + XSyncAlarmNotify)
    {
	XToGLSync *sync = reinterpret_cast<XToGLSync*>(arg);
	XSyncAlarmNotifyEvent *ae =
	    reinterpret_cast<XSyncAlarmNotifyEvent*>(ev);

	if (ae->alarm == sync->a)
	    return True;
    }

    return False;
}

void XToGLSync::trigger (void)
{
    Display *dpy = screen->dpy ();
    assert (state == XTOGLS_READY);

    XSyncTriggerFence (dpy, f);
    XFlush (dpy);

    state = XTOGLS_TRIGGER_SENT;
}

void XToGLSync::insertWait (void)
{
    if (state != XTOGLS_TRIGGER_SENT)
	return;

    GL::waitSync (fGL, 0, GL_TIMEOUT_IGNORED);

    state = XTOGLS_WAITING;
}

GLenum XToGLSync::checkUpdateFinished (GLuint64 timeout)
{
    GLenum status;

    switch (state) {
	case XTOGLS_DONE:
	    return GL_ALREADY_SIGNALED;

	case XTOGLS_WAITING:
	    status = GL::clientWaitSync (fGL, 0, timeout);
	    if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED)
	    {
		state = XTOGLS_DONE;
	    }
	    return status;

	default:
	    return GL_WAIT_FAILED;
    }
}

void XToGLSync::reset (void)
{
    Display *dpy = screen->dpy ();
    if (state != XTOGLS_DONE)
    {
	return;
    }

    XSyncResetFence (dpy, f);

    XSyncAlarmAttributes values;
    values.trigger.wait_value = nextCounterValue;
    XSyncChangeAlarm (dpy, a, XSyncCAValue, &values);
    XSyncSetCounter (dpy, c, nextCounterValue);

    int overflow;
    XSyncValueAdd (&nextCounterValue, nextCounterValue, one, &overflow);

    state = XTOGLS_RESET_PENDING;
}

void XToGLSync::handleEvent (XSyncAlarmNotifyEvent* ae)
{
    if (ae->alarm == a)
    {
	if (state != XTOGLS_RESET_PENDING)
	{
	    return;
	}

	state = XTOGLS_READY;
    }
}

