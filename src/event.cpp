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

#include <stdlib.h>
#include <string.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xfixes.h>

#include <core/core.h>
#include <core/atoms.h>
#include "privatescreen.h"
#include "privatewindow.h"

static Window xdndWindow = None;
static Window edgeWindow = None;



bool
PrivateWindow::handleSyncAlarm ()
{
    if (priv->syncWait)
    {
	priv->syncWait = false;

	if (window->resize (priv->syncGeometry))
	{
	    window->windowNotify (CompWindowNotifySyncAlarm);
	}
	else
	{
	    /* resizeWindow failing means that there is another pending
	       resize and we must send a new sync request to the client */
	    window->sendSyncRequest ();
	}
    }

    return false;
}


static bool
autoRaiseTimeout (CompScreen *screen)
{
    CompWindow  *w = screen->findWindow (screen->activeWindow ());

    if (screen->autoRaiseWindow () == screen->activeWindow () ||
	(w && (screen->autoRaiseWindow () == w->transientFor ())))
    {
	w = screen->findWindow (screen->autoRaiseWindow ());
	if (w)
	    w->updateAttributes (CompStackingUpdateModeNormal);
    }

    return false;
}

#define REAL_MOD_MASK (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | \
		       Mod3Mask | Mod4Mask | Mod5Mask | CompNoMask)

static bool
isCallBackBinding (CompOption	           &option,
		   CompAction::BindingType type,
		   CompAction::State       state)
{
    if (!option.isAction ())
	return false;

    if (!(option.value ().action ().type () & type))
	return false;

    if (!(option.value ().action ().state () & state))
	return false;

    return true;
}

static bool
isInitiateBinding (CompOption	           &option,
		   CompAction::BindingType type,
		   CompAction::State       state,
		   CompAction	           **action)
{
    if (!isCallBackBinding (option, type, state))
	return false;

    if (option.value ().action ().initiate ().empty ())
	return false;

    *action = &option.value ().action ();

    return true;
}

static bool
isTerminateBinding (CompOption	            &option,
		    CompAction::BindingType type,
		    CompAction::State       state,
		    CompAction              **action)
{
    if (!isCallBackBinding (option, type, state))
	return false;

    if (option.value ().action ().terminate ().empty ())
	return false;

    *action = &option.value ().action ();

    return true;
}

bool
PrivateScreen::triggerButtonPressBindings (CompOption::Vector &options,
					   XButtonEvent       *event,
					   CompOption::Vector &arguments)
{
    CompAction::State state = CompAction::StateInitButton;
    CompAction        *action;
    unsigned int      ignored = modHandler->ignoredModMask ();
    unsigned int      modMask = REAL_MOD_MASK & ~ignored;
    unsigned int      bindMods;
    unsigned int      edge = 0;

    if (edgeWindow)
    {
	unsigned int i;

	if (event->root != root)
	    return false;

	if (event->window != edgeWindow)
	{
	    if (grabs.empty () || event->window != root)
		return false;
	}

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	{
	    if (edgeWindow == screenEdge[i].id)
	    {
		edge = 1 << i;
		arguments[1].value ().set ((int) activeWindow);
		break;
	    }
	}
    }

    foreach (CompOption &option, options)
    {
	if (isInitiateBinding (option, CompAction::BindingTypeButton, state,
			       &action))
	{
	    if (action->button ().button () == (int) event->button)
	    {
		bindMods = modHandler->virtualToRealModMask (
		    action->button ().modifiers ());

		if ((bindMods & modMask) == (event->state & modMask))
		    if (action->initiate () (action, state, arguments))
			return true;
	    }
	}

	if (edge)
	{
	    if (isInitiateBinding (option, CompAction::BindingTypeEdgeButton,
				   state | CompAction::StateInitEdge, &action))
	    {
		if ((action->button ().button () == (int) event->button) &&
		    (action->edgeMask () & edge))
		{
		    bindMods = modHandler->virtualToRealModMask (
			action->button ().modifiers ());

		    if ((bindMods & modMask) == (event->state & modMask))
			if (action->initiate () (action, state |
						 CompAction::StateInitEdge,
						 arguments))
			    return true;
		}
	    }
	}
    }

    return false;
}

bool
PrivateScreen::triggerButtonReleaseBindings (CompOption::Vector &options,
					     XButtonEvent       *event,
					     CompOption::Vector &arguments)
{
    CompAction::State       state = CompAction::StateTermButton;
    CompAction::BindingType type  = CompAction::BindingTypeButton |
				    CompAction::BindingTypeEdgeButton;
    CompAction	            *action;

    foreach (CompOption &option, options)
    {
	if (isTerminateBinding (option, type, state, &action))
	{
	    if (action->button ().button () == (int) event->button)
	    {
		if (action->terminate () (action, state, arguments))
		    return true;
	    }
	}
    }

    return false;
}

bool
PrivateScreen::triggerKeyPressBindings (CompOption::Vector &options,
					XKeyEvent          *event,
					CompOption::Vector &arguments)
{
    CompAction::State state = 0;
    CompAction	      *action;
    unsigned int      modMask = REAL_MOD_MASK & ~modHandler->ignoredModMask ();
    unsigned int      bindMods;

    if (event->keycode == escapeKeyCode)
	state = CompAction::StateCancel;
    else if (event->keycode == returnKeyCode)
	state = CompAction::StateCommit;

    if (state)
    {
	foreach (CompOption &o, options)
	{
	    if (o.isAction ())
	    {
		if (!o.value ().action ().terminate ().empty ())
		    o.value ().action ().terminate () (&o.value ().action (),
						       state, noOptions);
	    }
	}

	if (state == CompAction::StateCancel)
	    return false;
    }

    state = CompAction::StateInitKey;
    foreach (CompOption &option, options)
    {
	if (isInitiateBinding (option, CompAction::BindingTypeKey,
			       state, &action))
	{
	    bindMods = modHandler->virtualToRealModMask (
		action->key ().modifiers ());

	    if (action->key ().keycode () == (int) event->keycode)
	    {
		if ((bindMods & modMask) == (event->state & modMask))
		    if (action->initiate () (action, state, arguments))
			return true;
	    }
	    else if (!xkbEvent && action->key ().keycode () == 0)
	    {
		if (bindMods == (event->state & modMask))
		    if (action->initiate () (action, state, arguments))
			return true;
	    }
	}
    }

    return false;
}

bool
PrivateScreen::triggerKeyReleaseBindings (CompOption::Vector &options,
					  XKeyEvent          *event,
					  CompOption::Vector &arguments)
{
    CompAction::State state = CompAction::StateTermKey;
    CompAction        *action;
    unsigned int      ignored = modHandler->ignoredModMask ();
    unsigned int      modMask = REAL_MOD_MASK & ~ignored;
    unsigned int      bindMods;
    unsigned int      mods;

    mods = modHandler->keycodeToModifiers (event->keycode);
    if (!xkbEvent && !mods)
	return false;

    foreach (CompOption &option, options)
    {
	if (isTerminateBinding (option, CompAction::BindingTypeKey,
				state, &action))
	{
	    bindMods = modHandler->virtualToRealModMask (action->key ().modifiers ());

	    if ((bindMods & modMask) == 0)
	    {
		if ((unsigned int) action->key ().keycode () ==
						  (unsigned int) event->keycode)
		{
		    if (action->terminate () (action, state, arguments))
			return true;
		}
	    }
	    else if (!xkbEvent && ((mods & modMask & bindMods) != bindMods))
	    {
		if (action->terminate () (action, state, arguments))
		    return true;
	    }
	}
    }

    return false;
}

bool
PrivateScreen::triggerStateNotifyBindings (CompOption::Vector  &options,
					   XkbStateNotifyEvent *event,
					   CompOption::Vector  &arguments)
{
    CompAction::State state;
    CompAction        *action;
    unsigned int      ignored = modHandler->ignoredModMask ();
    unsigned int      modMask = REAL_MOD_MASK & ~ignored;
    unsigned int      bindMods;

    if (event->event_type == KeyPress)
    {
	state = CompAction::StateInitKey;

	foreach (CompOption &option, options)
	{
	    if (isInitiateBinding (option, CompAction::BindingTypeKey,
				   state, &action))
	    {
		if (action->key ().keycode () == 0)
		{
		    bindMods =
			modHandler->virtualToRealModMask (action->key ().modifiers ());

		    if ((event->mods & modMask & bindMods) == bindMods)
		    {
			if (action->initiate () (action, state, arguments))
			    return true;
		    }
		}
	    }
	}
    }
    else
    {
	state = CompAction::StateTermKey;

	foreach (CompOption &option, options)
	{
	    if (isTerminateBinding (option, CompAction::BindingTypeKey,
				    state, &action))
	    {
		bindMods = modHandler->virtualToRealModMask (action->key ().modifiers ());

		if ((event->mods & modMask & bindMods) != bindMods)
		{
		    if (action->terminate () (action, state, arguments))
			return true;
		}
	    }
	}
    }

    return false;
}

static bool
isBellAction (CompOption        &option,
	      CompAction::State state,
	      CompAction        **action)
{
    if (option.type () != CompOption::TypeAction &&
	option.type () != CompOption::TypeBell)
	return false;

    if (!option.value ().action ().bell ())
	return false;

    if (!(option.value ().action ().state () & state))
	return false;

    if (option.value ().action ().initiate ().empty ())
	return false;

    *action = &option.value ().action ();

    return true;
}

static bool
triggerBellNotifyBindings (CompOption::Vector &options,
			   CompOption::Vector &arguments)
{
    CompAction::State state = CompAction::StateInitBell;
    CompAction        *action;

    foreach (CompOption &option, options)
    {
	if (isBellAction (option, state, &action))
	{
	    if (action->initiate () (action, state, arguments))
		return true;
	}
    }

    return false;
}

static bool
isEdgeAction (CompOption        &option,
	      CompAction::State state,
	      unsigned int      edge)
{
    if (option.type () != CompOption::TypeAction &&
	option.type () != CompOption::TypeButton &&
	option.type () != CompOption::TypeEdge)
	return false;

    if (!(option.value ().action ().edgeMask () & edge))
	return false;

    if (!(option.value ().action ().state () & state))
	return false;

    return true;
}

static bool
isEdgeEnterAction (CompOption        &option,
		   CompAction::State state,
		   CompAction::State delayState,
		   unsigned int      edge,
		   CompAction        **action)
{
    if (!isEdgeAction (option, state, edge))
	return false;

    if (option.value ().action ().type () & CompAction::BindingTypeEdgeButton)
	return false;

    if (option.value ().action ().initiate ().empty ())
	return false;

    if (delayState)
    {
	if ((option.value ().action ().state () &
	     CompAction::StateNoEdgeDelay) !=
	    (delayState & CompAction::StateNoEdgeDelay))
	{
	    /* ignore edge actions which shouldn't be delayed when invoking
	       undelayed edges (or vice versa) */
	    return false;
	}
    }


    *action = &option.value ().action ();

    return true;
}

static bool
isEdgeLeaveAction (CompOption        &option,
		   CompAction::State state,
		   unsigned int      edge,
		   CompAction        **action)
{
    if (!isEdgeAction (option, state, edge))
	return false;

    if (option.value ().action ().terminate ().empty ())
	return false;

    *action = &option.value ().action ();

    return true;
}

static bool
triggerEdgeEnterBindings (CompOption::Vector &options,
			  CompAction::State  state,
			  CompAction::State  delayState,
			  unsigned int	     edge,
			  CompOption::Vector &arguments)
{
    CompAction *action;

    foreach (CompOption &option, options)
    {
	if (isEdgeEnterAction (option, state, delayState, edge, &action))
	{
	    if (action->initiate () (action, state, arguments))
		return true;
	}
    }

    return false;
}

static bool
triggerEdgeLeaveBindings (CompOption::Vector &options,
			  CompAction::State  state,
			  unsigned int	     edge,
			  CompOption::Vector &arguments)
{
    CompAction *action;

    foreach (CompOption &option, options)
    {
	if (isEdgeLeaveAction (option, state, edge, &action))
	{
	    if (action->terminate () (action, state, arguments))
		return true;
	}
    }

    return false;
}

static bool
triggerAllEdgeEnterBindings (CompAction::State  state,
			     CompAction::State  delayState,
			     unsigned int       edge,
			     CompOption::Vector &arguments)
{
    foreach (CompPlugin *p, CompPlugin::getPlugins ())
    {
	CompOption::Vector &options = p->vTable->getOptions ();
	if (triggerEdgeEnterBindings (options, state, delayState, edge,
				      arguments))
	{
	    return true;
	}
    }
    return false;
}

static bool
delayedEdgeTimeout (CompDelayedEdgeSettings *settings)
{
    triggerAllEdgeEnterBindings (settings->state,
				 ~CompAction::StateNoEdgeDelay,
				 settings->edge,
				 settings->options);

    return false;
}

bool
PrivateScreen::triggerEdgeEnter (unsigned int       edge,
				 CompAction::State  state,
				 CompOption::Vector &arguments)
{
    int                     delay;

    delay = optionGetEdgeDelay ();

    if (delay > 0)
    {
	CompAction::State delayState;
	edgeDelaySettings.edge    = edge;
	edgeDelaySettings.state   = state;
	edgeDelaySettings.options = arguments;

	edgeDelayTimer.start  (
	    boost::bind (delayedEdgeTimeout, &edgeDelaySettings),
			 delay, (unsigned int) ((float) delay * 1.2));

	delayState = CompAction::StateNoEdgeDelay;
	if (triggerAllEdgeEnterBindings (state, delayState, edge, arguments))
	    return true;
    }
    else
    {
	if (triggerAllEdgeEnterBindings (state, 0, edge, arguments))
	    return true;
    }

    return false;
}

bool
PrivateScreen::handleActionEvent (XEvent *event)
{
    static CompOption::Vector o (8);
    Window xid;

    o[0].setName ("event_window", CompOption::TypeInt);
    o[1].setName ("window", CompOption::TypeInt);
    o[2].setName ("modifiers", CompOption::TypeInt);
    o[3].setName ("x", CompOption::TypeInt);
    o[4].setName ("y", CompOption::TypeInt);
    o[5].setName ("root", CompOption::TypeInt);
    o[6].reset ();
    o[7].reset ();

    switch (event->type) {
    case ButtonPress:
	/* We need to determine if we clicked on a parent frame
	 * window, if so, pass the appropriate child window as
	 * "window" and the frame as "event_window"
	 */

	xid = event->xbutton.window;

	foreach (CompWindow *w, screen->windows ())
	{
	    if (w->frame () == xid)
		xid = w->id ();
	}

	o[0].value ().set ((int) event->xbutton.window);
	o[1].value ().set ((int) xid);
	o[2].value ().set ((int) event->xbutton.state);
	o[3].value ().set ((int) event->xbutton.x_root);
	o[4].value ().set ((int) event->xbutton.y_root);
	o[5].value ().set ((int) event->xbutton.root);

	o[6].setName ("button", CompOption::TypeInt);
	o[7].setName ("time", CompOption::TypeInt);

	o[6].value ().set ((int) event->xbutton.button);
	o[7].value ().set ((int) event->xbutton.time);

	foreach (CompPlugin *p, CompPlugin::getPlugins ())
	{
	    CompOption::Vector &options = p->vTable->getOptions ();
	    if (triggerButtonPressBindings (options, &event->xbutton, o))
		return true;
	}
	break;
    case ButtonRelease:
	o[0].value ().set ((int) event->xbutton.window);
	o[1].value ().set ((int) event->xbutton.window);
	o[2].value ().set ((int) event->xbutton.state);
	o[3].value ().set ((int) event->xbutton.x_root);
	o[4].value ().set ((int) event->xbutton.y_root);
	o[5].value ().set ((int) event->xbutton.root);

	o[6].setName ("button", CompOption::TypeInt);
	o[7].setName ("time", CompOption::TypeInt);

	o[6].value ().set ((int) event->xbutton.button);
	o[7].value ().set ((int) event->xbutton.time);

	foreach (CompPlugin *p, CompPlugin::getPlugins ())
	{
	    CompOption::Vector &options = p->vTable->getOptions ();
	    if (triggerButtonReleaseBindings (options, &event->xbutton, o))
		return true;
	}
	break;
    case KeyPress:
	o[0].value ().set ((int) event->xkey.window);
	o[1].value ().set ((int) activeWindow);
	o[2].value ().set ((int) event->xkey.state);
	o[3].value ().set ((int) event->xkey.x_root);
	o[4].value ().set ((int) event->xkey.y_root);
	o[5].value ().set ((int) event->xkey.root);

	o[6].setName ("keycode", CompOption::TypeInt);
	o[7].setName ("time", CompOption::TypeInt);

	o[6].value ().set ((int) event->xkey.keycode);
	o[7].value ().set ((int) event->xkey.time);

	foreach (CompPlugin *p, CompPlugin::getPlugins ())
	{
	    CompOption::Vector &options = p->vTable->getOptions ();
	    if (triggerKeyPressBindings (options, &event->xkey, o))
		return true;
	}
	break;
    case KeyRelease:
	o[0].value ().set ((int) event->xkey.window);
	o[1].value ().set ((int) activeWindow);
	o[2].value ().set ((int) event->xkey.state);
	o[3].value ().set ((int) event->xkey.x_root);
	o[4].value ().set ((int) event->xkey.y_root);
	o[5].value ().set ((int) event->xkey.root);

	o[6].setName ("keycode", CompOption::TypeInt);
	o[7].setName ("time", CompOption::TypeInt);

	o[6].value ().set ((int) event->xkey.keycode);
	o[7].value ().set ((int) event->xkey.time);

	foreach (CompPlugin *p, CompPlugin::getPlugins ())
	{
	    CompOption::Vector &options = p->vTable->getOptions ();
	    if (triggerKeyReleaseBindings (options, &event->xkey, o))
		return true;
	}
	break;
    case EnterNotify:
	if (event->xcrossing.mode   != NotifyGrab   &&
	    event->xcrossing.mode   != NotifyUngrab &&
	    event->xcrossing.detail != NotifyInferior)
	{
	    unsigned int      edge, i;
	    CompAction::State state;

	    if (event->xcrossing.root != root)
		return false;

	    if (edgeDelayTimer.active ())
		edgeDelayTimer.stop ();

	    if (edgeWindow && edgeWindow != event->xcrossing.window)
	    {
		state = CompAction::StateTermEdge;
		edge  = 0;

		for (i = 0; i < SCREEN_EDGE_NUM; i++)
		{
		    if (edgeWindow == screenEdge[i].id)
		    {
			edge = 1 << i;
			break;
		    }
		}

		edgeWindow = None;

		o[0].value ().set ((int) event->xcrossing.window);
		o[1].value ().set ((int) activeWindow);
		o[2].value ().set ((int) event->xcrossing.state);
		o[3].value ().set ((int) event->xcrossing.x_root);
		o[4].value ().set ((int) event->xcrossing.y_root);
		o[5].value ().set ((int) event->xcrossing.root);

		o[6].setName ("time", CompOption::TypeInt);
		o[6].value ().set ((int) event->xcrossing.time);

		foreach (CompPlugin *p, CompPlugin::getPlugins ())
		{
		    CompOption::Vector &options = p->vTable->getOptions ();
		    if (triggerEdgeLeaveBindings (options, state, edge, o))
			return true;
		}
	    }

	    edge = 0;

	    for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    {
		if (event->xcrossing.window == screenEdge[i].id)
		{
		    edge = 1 << i;
		    break;
		}
	    }

	    if (edge)
	    {
		state = CompAction::StateInitEdge;

		edgeWindow = event->xcrossing.window;

		o[0].value ().set ((int) event->xcrossing.window);
		o[1].value ().set ((int) activeWindow);
		o[2].value ().set ((int) event->xcrossing.state);
		o[3].value ().set ((int) event->xcrossing.x_root);
		o[4].value ().set ((int) event->xcrossing.y_root);
		o[5].value ().set ((int) event->xcrossing.root);

		o[6].setName ("time", CompOption::TypeInt);
		o[6].value ().set ((int) event->xcrossing.time);

		if (triggerEdgeEnter (edge, state, o))
		    return true;
	    }
	}
	break;
    case ClientMessage:
	if (event->xclient.message_type == Atoms::xdndEnter)
	{
	    xdndWindow = event->xclient.window;
	}
	else if (event->xclient.message_type == Atoms::xdndLeave)
	{
	    unsigned int      edge = 0;
	    CompAction::State state;

	    if (!xdndWindow)
	    {
		CompWindow *w;

		w = screen->findWindow (event->xclient.window);
		if (w)
		{
		    unsigned int i;

		    for (i = 0; i < SCREEN_EDGE_NUM; i++)
		    {
			if (event->xclient.window == screenEdge[i].id)
			{
			    edge = 1 << i;
			    break;
			}
		    }
		}
	    }

	    if (edge)
	    {
		state = CompAction::StateTermEdgeDnd;

		o[0].value ().set ((int) event->xclient.window);
		o[1].value ().set ((int) activeWindow);
		o[2].value ().set ((int) 0); /* fixme */
		o[3].value ().set ((int) 0); /* fixme */
		o[4].value ().set ((int) 0); /* fixme */
		o[5].value ().set ((int) root);

		foreach (CompPlugin *p, CompPlugin::getPlugins ())
		{
		    CompOption::Vector &options = p->vTable->getOptions ();
		    if (triggerEdgeLeaveBindings (options, state, edge, o))
			return true;
		}
	    }
	}
	else if (event->xclient.message_type == Atoms::xdndPosition)
	{
	    unsigned int      edge = 0;
	    CompAction::State state;

	    if (xdndWindow == event->xclient.window)
	    {
		CompWindow *w;

		w = screen->findWindow (event->xclient.window);
		if (w)
		{
		    unsigned int i;

		    for (i = 0; i < SCREEN_EDGE_NUM; i++)
		    {
			if (xdndWindow == screenEdge[i].id)
			{
			    edge = 1 << i;
			    break;
			}
		    }
		}
	    }

	    if (edge)
	    {
		state = CompAction::StateInitEdgeDnd;

		o[0].value ().set ((int) event->xclient.window);
		o[1].value ().set ((int) activeWindow);
		o[2].value ().set ((int) 0); /* fixme */
		o[3].value ().set ((int) event->xclient.data.l[2] >> 16);
		o[4].value ().set ((int) event->xclient.data.l[2] & 0xffff);
		o[5].value ().set ((int) root);

		if (triggerEdgeEnter (edge, state, o))
		    return true;
	    }

	    xdndWindow = None;
	}
	break;
    default:
	if (event->type == xkbEvent)
	{
	    XkbAnyEvent *xkbEvent = (XkbAnyEvent *) event;

	    if (xkbEvent->xkb_type == XkbStateNotify)
	    {
		XkbStateNotifyEvent *stateEvent = (XkbStateNotifyEvent *) event;

		o[0].value ().set ((int) activeWindow);
		o[1].value ().set ((int) activeWindow);
		o[2].value ().set ((int) stateEvent->mods);

		o[3].setName ("time", CompOption::TypeInt);
		o[3].value ().set ((int) xkbEvent->time);
		o[4].reset ();
		o[5].reset ();

		foreach (CompPlugin *p, CompPlugin::getPlugins ())
		{
		    CompOption::Vector &options = p->vTable->getOptions ();
		    if (triggerStateNotifyBindings (options, stateEvent, o))
			return true;
		}
	    }
	    else if (xkbEvent->xkb_type == XkbBellNotify)
	    {
		o[0].value ().set ((int) activeWindow);
		o[1].value ().set ((int) activeWindow);

		o[2].setName ("time", CompOption::TypeInt);
		o[2].value ().set ((int) xkbEvent->time);
		o[3].reset ();
		o[4].reset ();
		o[5].reset ();


		foreach (CompPlugin *p, CompPlugin::getPlugins ())
		{
		    CompOption::Vector &options = p->vTable->getOptions ();
		    if (triggerBellNotifyBindings (options, o))
			return true;
		}
	    }
	}
	break;
    }

    return false;
}

void
PrivateScreen::setDefaultWindowAttributes (XWindowAttributes *wa)
{
    wa->x		      = 0;
    wa->y		      = 0;
    wa->width		      = 1;
    wa->height		      = 1;
    wa->border_width	      = 0;
    wa->depth		      = 0;
    wa->visual		      = NULL;
    wa->root		      = None;
    wa->c_class		      = InputOnly;
    wa->bit_gravity	      = NorthWestGravity;
    wa->win_gravity	      = NorthWestGravity;
    wa->backing_store	      = NotUseful;
    wa->backing_planes	      = 0;
    wa->backing_pixel	      = 0;
    wa->save_under	      = false;
    wa->colormap	      = None;
    wa->map_installed	      = false;
    wa->map_state	      = IsUnviewable;
    wa->all_event_masks	      = 0;
    wa->your_event_mask	      = 0;
    wa->do_not_propagate_mask = 0;
    wa->override_redirect     = true;
    wa->screen		      = NULL;
}

void
CompScreen::handleCompizEvent (const char         *plugin,
			       const char         *event,
			       CompOption::Vector &options)
    WRAPABLE_HND_FUNC (7, handleCompizEvent, plugin, event, options)

void
CompScreen::handleEvent (XEvent *event)
{
    WRAPABLE_HND_FUNC (6, handleEvent, event)

    CompWindow *w = NULL;
    XWindowAttributes wa;

    switch (event->type) {
    case ButtonPress:
	if (event->xbutton.root == priv->root)
	    priv->setCurrentOutput (
		outputDeviceForPoint (event->xbutton.x_root,
						 event->xbutton.y_root));
	break;
    case MotionNotify:
	if (event->xmotion.root == priv->root)
	    priv->setCurrentOutput (
		outputDeviceForPoint (event->xmotion.x_root,
				      event->xmotion.y_root));
	break;
    case KeyPress:
	w = findWindow (priv->activeWindow);
	if (w)
	    priv->setCurrentOutput (w->outputDevice ());
	break;
    default:
	break;
    }

    if (priv->handleActionEvent (event))
    {
	if (priv->grabs.empty ())
	    XAllowEvents (priv->dpy, AsyncPointer, event->xbutton.time);

	return;
    }

    switch (event->type) {
    case SelectionRequest:
	priv->handleSelectionRequest (event);
	break;
    case SelectionClear:
	priv->handleSelectionClear (event);
	break;
    case ConfigureNotify:
	w = findWindow (event->xconfigure.window);
	if (w && !w->frame ())
	{
	    w->priv->configure (&event->xconfigure);
	}
	else
	{
	    w = findTopLevelWindow (event->xconfigure.window);

	    if (w && w->frame () == event->xconfigure.window)
	    {
		w->priv->configureFrame (&event->xconfigure);
	    }
	    else
	    {
		if (event->xconfigure.window == priv->root)
		    priv->configure (&event->xconfigure);
	    }
	}
	break;
    case CreateNotify:
    {
	bool failure = false;

	/* Failure means that window has been destroyed. We still have to add 
	 * the window to the window list as we might get configure requests
	 * which require us to stack other windows relative to it. Setting
	 * some default values if this is the case. */
	if ((failure = !XGetWindowAttributes (priv->dpy, event->xcreatewindow.window, &wa)))
	    priv->setDefaultWindowAttributes (&wa);

	w = findTopLevelWindow (event->xcreatewindow.window, true);

	if ((event->xcreatewindow.parent == wa.root || failure) &&
	    (!w || w->frame () != event->xcreatewindow.window))
	{
	    /* Track the window if it was created on this
	     * screen, otherwise we still need to register
	     * for FocusChangeMask. Also, we don't want to
	     * manage it straight away - in reality we want
	     * that to wait until the map request */
	    if (failure || (wa.root == priv->root))
	    {
		/* Our SubstructureRedirectMask doesn't work on OverrideRedirect
		 * windows so we need to track them directly here */
		if (!event->xcreatewindow.override_redirect)
		    new CoreWindow (event->xcreatewindow.window);
		else
		{
		    CoreWindow *cw = 
			new CoreWindow (event->xcreatewindow.window);
		    
		    if (cw)
		    {
			w = cw->manage (priv->getTopWindow (), wa);
			delete cw;
		    }
		}
	    }
	    else
		XSelectInput (priv->dpy, event->xcreatewindow.window,
			      FocusChangeMask);
	}
	break;
    }
    case DestroyNotify:
	w = findWindow (event->xdestroywindow.window);
	if (w)
	{
	    w->moveInputFocusToOtherWindow ();
	    w->destroy ();
	}
	else
	{
	    foreach (CoreWindow *cw, priv->createdWindows)
	    {
		if (cw->priv->id == event->xdestroywindow.window)
		{
		    priv->createdWindows.remove (cw);
		    break;
		}
	    }
	}
	break;
    case MapNotify:

	/* Some broken applications and toolkits (eg QT) will lie to
	 * us about their override-redirect mask - not setting it on
	 * the initial CreateNotify and then setting it later on
	 * just after creation. Unfortunately, this means that QT
	 * has successfully bypassed both of our window tracking
	 * mechanisms (eg, not override-redirect at CreateNotify time
	 * and then bypassing MapRequest because it *is* override-redirect
	 * at XMapWindow time, so we need to catch this case and make
	 * sure that windows are tracked here */
	
	foreach (CoreWindow *cw, priv->createdWindows)
	{
	    if (cw->priv->id == event->xmap.window)
	    {
		/* Failure means the window has been destroyed, but
		 * still add it to the window list anyways since we
		 * will soon handle the DestroyNotify event for it
		 * and in between CreateNotify time and DestroyNotify
		 * time there might be ConfigureRequests asking us
		 * to stack windows relative to it
		 */
		if (!XGetWindowAttributes (screen->dpy (), cw->priv->id, &wa))
		    priv->setDefaultWindowAttributes (&wa);

		w = cw->manage (priv->getTopWindow (), wa);
		delete cw;
		break;
	    }
	}

	/* Search in already-created windows for this window */
	if (!w)
	    w = findWindow (event->xmap.window);

	if (w)
	{
	    if (w->priv->pendingMaps)
	    {
		/* The only case where this happens
		 * is where the window unmaps itself
		 * but doesn't get destroyed so when
		 * it re-maps we need to reparent it */

		if (!w->priv->frame)
		    w->priv->reparent ();
		w->priv->managed = true;
	    }

	    /* been shaded */
	    if (w->priv->height == 0)
	    {
		if (w->id () == priv->activeWindow)
		    w->moveInputFocusTo ();
	    }

	    w->map ();
	}

	break;
    case UnmapNotify:
	w = findWindow (event->xunmap.window);
	if (w)
	{
	    /* Normal -> Iconic */
	    if (w->pendingUnmaps ())
	    {
		priv->setWmState (IconicState, w->id ());
		w->priv->pendingUnmaps--;
	    }
	    else /* X -> Withdrawn */
	    {
		/* Iconic -> Withdrawn */
		if (w->state () & CompWindowStateHiddenMask)
		{
		    w->priv->minimized = false;

		    w->changeState (w->state () & ~CompWindowStateHiddenMask);

		    priv->updateClientList ();
		}
		else /* Closing */
		    w->windowNotify (CompWindowNotifyClose);

		if (!w->overrideRedirect ())
		    priv->setWmState (WithdrawnState, w->id ());

		w->priv->placed     = false;
		w->priv->managed    = false;
		w->priv->unmanaging = true;

		if (w->priv->frame)
		{
		    w->priv->unreparent ();
		}
	    }

	    w->unmap ();

	    if (!w->shaded () && !w->priv->pendingMaps)
		w->moveInputFocusToOtherWindow ();
	}
	break;
    case ReparentNotify:
	w = findWindow (event->xreparent.window);
	if (!w && event->xreparent.parent == priv->root)
	{
	    /* Failure means that window has been destroyed. We still have to add 
	     * the window to the window list as we might get configure requests
	     * which require us to stack other windows relative to it. Setting
	     * some default values if this is the case. */
	    if (!XGetWindowAttributes (priv->dpy, event->xcreatewindow.window, &wa))
		priv->setDefaultWindowAttributes (&wa);

	    CoreWindow *cw = new CoreWindow (event->xreparent.window);

	    if (cw)
	    {
		cw->manage (priv->getTopWindow (), wa);
		delete cw;
	    }
	}
	else if (w && !(event->xreparent.parent == w->priv->wrapper ||
		 event->xreparent.parent == priv->root))
	{
	    /* This is the only case where a window is removed but not
	       destroyed. We must remove our event mask and all passive
	       grabs. */
	    XSelectInput (priv->dpy, w->id (), NoEventMask);
	    XShapeSelectInput (priv->dpy, w->id (), NoEventMask);
	    XUngrabButton (priv->dpy, AnyButton, AnyModifier, w->id ());

	    w->moveInputFocusToOtherWindow ();

	    w->destroy ();
	}
	break;
    case CirculateNotify:
	w = findWindow (event->xcirculate.window);
	if (w)
	    w->priv->circulate (&event->xcirculate);
	break;
    case ButtonPress:
	if (event->xbutton.button == Button1 ||
	    event->xbutton.button == Button2 ||
	    event->xbutton.button == Button3)
	{
	    w = findTopLevelWindow (event->xbutton.window);
	    if (w)
	    {
		if (priv->optionGetRaiseOnClick ())
		    w->updateAttributes (CompStackingUpdateModeAboveFullscreen);

	        if (w->id () != priv->activeWindow)
		    if (!(w->type () & CompWindowTypeDockMask))
			if (w->focus ())
			    w->moveInputFocusTo ();
	    }
	}

	if (priv->grabs.empty ())
	    XAllowEvents (priv->dpy, ReplayPointer, event->xbutton.time);

	break;
    case PropertyNotify:
	if (event->xproperty.atom == Atoms::winType)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
	    {
		unsigned int type;

		type = priv->getWindowType (w->id ());

		if (type != w->wmType ())
		{
		    if (w->isViewable ())
		    {
			if (w->type () == CompWindowTypeDesktopMask)
			    priv->desktopWindowCount--;
			else if (type == CompWindowTypeDesktopMask)
			    priv->desktopWindowCount++;
		    }

		    w->wmType () = type;

		    w->recalcType ();
		    w->recalcActions ();

		    if (type & (CompWindowTypeDockMask |
				CompWindowTypeDesktopMask))
			w->setDesktop (0xffffffff);

		    priv->updateClientList ();

		    matchPropertyChanged (w);
		}
	    }
	}
	else if (event->xproperty.atom == Atoms::winState)
	{
	    w = findWindow (event->xproperty.window);
	    if (w && !w->managed ())
	    {
		unsigned int state;

		state = priv->getWindowState (w->id ());
		state = CompWindow::constrainWindowState (state, w->actions ());

		/* EWMH suggests that we ignore changes
		   to _NET_WM_STATE_HIDDEN */
		if (w->state () & CompWindowStateHiddenMask)
		    state |= CompWindowStateHiddenMask;
		else
		    state &= ~CompWindowStateHiddenMask;

		w->changeState (state);
	    }
	}
	else if (event->xproperty.atom == XA_WM_NORMAL_HINTS)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
	    {
		w->priv->updateNormalHints ();
		w->recalcActions ();
	    }
	}
	else if (event->xproperty.atom == XA_WM_HINTS)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->updateWmHints ();
	}
	else if (event->xproperty.atom == XA_WM_TRANSIENT_FOR)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
	    {
		w->priv->updateTransientHint ();
		w->recalcActions ();
	    }
	}
	else if (event->xproperty.atom == Atoms::wmClientLeader)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->clientLeader = w->priv->getClientLeader ();
	}
	else if (event->xproperty.atom == Atoms::wmIconGeometry)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->updateIconGeometry ();
	}
	else if (event->xproperty.atom == Atoms::wmStrut ||
		 event->xproperty.atom == Atoms::wmStrutPartial)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
	    {
		if (w->updateStruts ())
		    updateWorkarea ();
	    }
	}
	else if (event->xproperty.atom == Atoms::mwmHints)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->updateMwmHints ();
	}
	else if (event->xproperty.atom == Atoms::wmProtocols)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->protocols = priv->getProtocols (w->id ());
	}
	else if (event->xproperty.atom == Atoms::wmIcon)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->freeIcons ();
	}
	else if (event->xproperty.atom == Atoms::startupId)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->updateStartupId ();
	}
	else if (event->xproperty.atom == XA_WM_CLASS)
	{
	    w = findWindow (event->xproperty.window);
	    if (w)
		w->priv->updateClassHints ();
	}
	break;
    case MotionNotify:
	break;
    case ClientMessage:
	if (event->xclient.message_type == Atoms::winActive)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
	    {
		/* use focus stealing prevention if request came
		   from an application  */
		if (event->xclient.data.l[0] != ClientTypeApplication ||
		    w->priv->allowWindowFocus (0, event->xclient.data.l[1]))
		{
		    w->activate ();
		}
	    }
	}
	else if (event->xclient.message_type == Atoms::winState)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
	    {
		unsigned long wState, state;
		int	      i;

		wState = w->state ();

		for (i = 1; i < 3; i++)
		{
		    state = priv->windowStateMask (event->xclient.data.l[i]);
		    if (state & ~CompWindowStateHiddenMask)
		    {

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD    1
#define _NET_WM_STATE_TOGGLE 2

			switch (event->xclient.data.l[0]) {
			case _NET_WM_STATE_REMOVE:
			    wState &= ~state;
			    break;
			case _NET_WM_STATE_ADD:
			    wState |= state;
			    break;
			case _NET_WM_STATE_TOGGLE:
			    wState ^= state;
			    break;
			}
		    }
		}

		wState = CompWindow::constrainWindowState (wState,
							   w->actions ());
		if (w->id () == priv->activeWindow)
		    wState &= ~CompWindowStateDemandsAttentionMask;

		if (wState != w->state ())
		{
		    CompStackingUpdateMode stackingUpdateMode;
		    unsigned long          dState = wState ^ w->state ();

		    stackingUpdateMode = CompStackingUpdateModeNone;

		    /* raise the window whenever its fullscreen state,
		       above/below state or maximization state changed */
		    if (dState & (CompWindowStateFullscreenMask |
				  CompWindowStateAboveMask |
				  CompWindowStateBelowMask |
				  CompWindowStateMaximizedHorzMask |
				  CompWindowStateMaximizedVertMask))
			stackingUpdateMode = CompStackingUpdateModeNormal;

		    w->changeState (wState);

		    w->updateAttributes (stackingUpdateMode);
		}
	    }
	}
	else if (event->xclient.message_type == Atoms::wmProtocols)
	{
	    if ((unsigned long) event->xclient.data.l[0] == Atoms::wmPing)
	    {
		w = findWindow (event->xclient.data.l[2]);
		if (w)
		    w->priv->handlePing (priv->lastPing);
	    }
	}
	else if (event->xclient.message_type == Atoms::closeWindow)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
		w->close (event->xclient.data.l[0]);
	}
	else if (event->xclient.message_type == Atoms::desktopGeometry)
	{
	    if (event->xclient.window == priv->root)
	    {
		CompOption::Value value;

		value.set ((int) (event->xclient.data.l[0] /
			   width ()));

		setOptionForPlugin ("core", "hsize", value);

		value.set ((int) (event->xclient.data.l[1] /
			   height ()));

		setOptionForPlugin ("core", "vsize", value);
	    }
	}
	else if (event->xclient.message_type == Atoms::moveResizeWindow)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
	    {
		unsigned int   xwcm = 0;
		XWindowChanges xwc;
		int            gravity;
		int	       value_mask;
		unsigned int   source;

		gravity = (event->xclient.data.l[0] & 0xFF);		
		value_mask = (event->xclient.data.l[0] & 0xF00) >> 8;
		source = (event->xclient.data.l[0] & 0xF000) >> 12;

		memset (&xwc, 0, sizeof (xwc));

		if (value_mask & CWX)
		{
		    xwcm |= CWX;
		    xwc.x = event->xclient.data.l[1];
		}

		if (value_mask & CWY)
		{
		    xwcm |= CWY;
		    xwc.y = event->xclient.data.l[2];
		}

		if (value_mask & CWWidth)
		{
		    xwcm |= CWWidth;
		    xwc.width = event->xclient.data.l[3];
		}

		if (value_mask & CWHeight)
		{
		    xwcm |= CWHeight;
		    xwc.height = event->xclient.data.l[4];
		}

		w->moveResize (&xwc, xwcm, gravity, source);
	    }
	}
	else if (event->xclient.message_type == Atoms::restackWindow)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
	    {
		/* TODO: other stack modes than Above and Below */
		if (event->xclient.data.l[1])
		{
		    CompWindow *sibling;

		    sibling = findWindow (event->xclient.data.l[1]);
		    if (sibling)
		    {
			if (event->xclient.data.l[2] == Above)
			    w->restackAbove (sibling);
			else if (event->xclient.data.l[2] == Below)
			    w->restackBelow (sibling);
		    }
		}
		else
		{
		    if (event->xclient.data.l[2] == Above)
			w->raise ();
		    else if (event->xclient.data.l[2] == Below)
			w->lower ();
		}
	    }
	}
	else if (event->xclient.message_type == Atoms::wmChangeState)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
	    {
		if (event->xclient.data.l[0] == IconicState)
		{
		    if (w->actions () & CompWindowActionMinimizeMask)
			w->minimize ();
		}
		else if (event->xclient.data.l[0] == NormalState)
		{
		    w->unminimize ();
		}
	    }
	}
	else if (event->xclient.message_type == Atoms::showingDesktop)
	{
	    if (event->xclient.window == priv->root ||
		event->xclient.window == None)
	    {
		if (event->xclient.data.l[0])
		    enterShowDesktopMode ();
		else
		    leaveShowDesktopMode (NULL);
	    }
	}
	else if (event->xclient.message_type == Atoms::numberOfDesktops)
	{
	    if (event->xclient.window == priv->root)
	    {
		CompOption::Value value;

		value.set ((int) event->xclient.data.l[0]);

		setOptionForPlugin ("core", "number_of_desktops", value);
	    }
	}
	else if (event->xclient.message_type == Atoms::currentDesktop)
	{
	    if (event->xclient.window == priv->root)
		priv->setCurrentDesktop (event->xclient.data.l[0]);
	}
	else if (event->xclient.message_type == Atoms::winDesktop)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
		w->setDesktop (event->xclient.data.l[0]);
	}
	else if (event->xclient.message_type == Atoms::wmFullscreenMonitors)
	{
	    w = findWindow (event->xclient.window);
	    if (w)
	    {
		CompFullscreenMonitorSet monitors;

		monitors.top    = event->xclient.data.l[0];
		monitors.bottom = event->xclient.data.l[1];
		monitors.left   = event->xclient.data.l[2];
		monitors.right  = event->xclient.data.l[3];

		w->priv->setFullscreenMonitors (&monitors);
	    }
	}
	break;
    case MappingNotify:
	modHandler->updateModifierMappings ();
	break;
    case MapRequest:
	/* Create the CompWindow structure here */
	w = NULL;

	foreach (CoreWindow *cw, priv->createdWindows)
	{
	    if (cw->priv->id == event->xmaprequest.window)
	    {
		/* Failure means the window has been destroyed, but
		 * still add it to the window list anyways since we
		 * will soon handle the DestroyNotify event for it
		 * and in between CreateNotify time and DestroyNotify
		 * time there might be ConfigureRequests asking us
		 * to stack windows relative to it
		 */
		if (!XGetWindowAttributes (screen->dpy (), cw->priv->id, &wa))
		    priv->setDefaultWindowAttributes (&wa);

		w = cw->manage (priv->getTopWindow (), wa);
		delete cw;
		break;
	    }
	}

	if (!w)
	    w = screen->findWindow (event->xmaprequest.window);

	if (w)
	{
	    XWindowAttributes attr;
	    bool              doMapProcessing = true;

	    /* We should check the override_redirect flag here, because the
	       client might have changed it while being unmapped. */
	    if (XGetWindowAttributes (priv->dpy, w->id (), &attr))
		w->priv->setOverrideRedirect (attr.override_redirect != 0);

	    if (w->state () & CompWindowStateHiddenMask)
		if (!w->minimized () && !w->inShowDesktopMode ())
		    doMapProcessing = false;

	    if (doMapProcessing)
		w->priv->processMap ();

	    w->priv->managed = true;

	    setWindowProp (w->id (), Atoms::winDesktop, w->desktop ());
	}
	else
	{
	    XMapWindow (priv->dpy, event->xmaprequest.window);
	}
	break;
    case ConfigureRequest:
	w = findWindow (event->xconfigurerequest.window);
	if (w && w->managed ())
	{
	    XWindowChanges xwc;

	    memset (&xwc, 0, sizeof (xwc));

	    xwc.x	     = event->xconfigurerequest.x;
	    xwc.y	     = event->xconfigurerequest.y;
	    xwc.width	     = event->xconfigurerequest.width;
	    xwc.height       = event->xconfigurerequest.height;
	    xwc.border_width = event->xconfigurerequest.border_width;

	    w->moveResize (&xwc, event->xconfigurerequest.value_mask,
			   0, ClientTypeUnknown);

	    if (event->xconfigurerequest.value_mask & CWStackMode)
	    {
		Window     above    = None;
		CompWindow *sibling = NULL;

		if (event->xconfigurerequest.value_mask & CWSibling)
		{
		    above   = event->xconfigurerequest.above;
		    sibling = findTopLevelWindow (above);
		}

		switch (event->xconfigurerequest.detail) {
		case Above:
		    if (w->priv->allowWindowFocus (NO_FOCUS_MASK, 0))
		    {
			if (above)
			{
			    if (sibling)
				w->restackAbove (sibling);
			}
			else
			    w->raise ();
		    }
		    break;
		case Below:
		    if (above)
		    {
			if (sibling)
			    w->restackBelow (sibling);
		    }
		    else
			w->lower ();
		    break;
		default:
		    /* no handling of the TopIf, BottomIf, Opposite cases -
		       there will hardly be any client using that */
		    break;
		}
	    }
	}
	else
	{
	    XWindowChanges xwc;
	    unsigned int   xwcm;

	    xwcm = event->xconfigurerequest.value_mask &
		(CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

	    xwc.x	     = event->xconfigurerequest.x;
	    xwc.y	     = event->xconfigurerequest.y;
	    xwc.width	     = event->xconfigurerequest.width;
	    xwc.height	     = event->xconfigurerequest.height;
	    xwc.border_width = event->xconfigurerequest.border_width;

	    if (w)
		w->configureXWindow (xwcm, &xwc);
	    else
		XConfigureWindow (priv->dpy, event->xconfigurerequest.window,
				  xwcm, &xwc);
	}
	break;
    case CirculateRequest:
	break;
    case FocusIn:
    {
        bool success = XGetWindowAttributes (priv->dpy, event->xfocus.window, &wa);

        /* If the call to XGetWindowAttributes failed it means
         * the window was destroyed, so track the focus change
         * anyways since we need to increment activeNum
         * and the passive button grabs and then we will
         * get the DestroyNotify later and change the focus
         * there
         */

        if (!success || wa.root == priv->root)
	{
	    if (event->xfocus.mode != NotifyGrab)
	    {
		w = findTopLevelWindow (event->xfocus.window);
		if (w && w->managed ())
		{
		    unsigned int state = w->state ();

		    if (w->id () != priv->activeWindow)
		    {
			CompWindow *active = screen->findWindow (priv->activeWindow);
			w->windowNotify (CompWindowNotifyFocusChange);

			priv->activeWindow = w->id ();
			w->priv->activeNum = priv->activeNum++;

			if (active)
			    active->priv->updatePassiveButtonGrabs ();

			w->priv->updatePassiveButtonGrabs ();

			priv->addToCurrentActiveWindowHistory (w->id ());

			XChangeProperty (priv->dpy , priv->root,
					 Atoms::winActive,
					 XA_WINDOW, 32, PropModeReplace,
					 (unsigned char *) &priv->activeWindow, 1);
		    }

		    state &= ~CompWindowStateDemandsAttentionMask;
		    w->changeState (state);

		    if (priv->nextActiveWindow == event->xfocus.window)
			priv->nextActiveWindow = None;
	        }
	    }
	    else
		priv->grabbed = true;
	}
	else
	{
	    CompWindow *w;

	    w = screen->findWindow (priv->activeWindow);

	    priv->nextActiveWindow = None;
	    priv->activeWindow = None;

	    if (w)
		w->priv->updatePassiveButtonGrabs ();
	}
    }
    break;
    case FocusOut:
	if (event->xfocus.mode == NotifyUngrab)
	    priv->grabbed = false;
	break;
    case EnterNotify:
	if (event->xcrossing.root == priv->root)
	    w = findTopLevelWindow (event->xcrossing.window);
	else
	    w = NULL;

	if (w && w->id () != priv->below)
	{
	    priv->below = w->id ();

	    if (!priv->optionGetClickToFocus () &&
		priv->grabs.empty ()                                 &&
		event->xcrossing.mode   != NotifyGrab                &&
		event->xcrossing.detail != NotifyInferior)
	    {
		bool raise;
		int  delay;

		raise = priv->optionGetAutoraise ();
		delay = priv->optionGetAutoraiseDelay ();

		if (priv->autoRaiseTimer.active () &&
		    priv->autoRaiseWindow != w->id ())
		{
		    priv->autoRaiseTimer.stop ();
		}

		if (w->type () & ~(CompWindowTypeDockMask |
				   CompWindowTypeDesktopMask))
		{
		    w->moveInputFocusTo ();

		    if (raise)
		    {
			if (delay > 0)
			{
			    priv->autoRaiseWindow = w->id ();
			    priv->autoRaiseTimer.start (
				boost::bind (autoRaiseTimeout, this),
				delay, (unsigned int) ((float) delay * 1.2));
			}
			else
			{
			    CompStackingUpdateMode mode =
				CompStackingUpdateModeNormal;

			    w->updateAttributes (mode);
			}
		    }
		}
	    }
	}
	break;
    case LeaveNotify:
	if (event->xcrossing.detail != NotifyInferior)
	{
	    if (event->xcrossing.window == priv->below)
		priv->below = None;
	}
	break;
    default:
	if (priv->shapeExtension &&
		 event->type == priv->shapeEvent + ShapeNotify)
	{
	    w = findWindow (((XShapeEvent *) event)->window);
	    if (w)
	    {
		if (w->mapNum ())
		    w->priv->updateRegion ();
	    }
	}
	else if (event->type == priv->syncEvent + XSyncAlarmNotify)
	{
	    XSyncAlarmNotifyEvent *sa;

	    sa = (XSyncAlarmNotifyEvent *) event;


	    foreach (w, priv->windows)
	    {
		if (w->priv->syncAlarm == sa->alarm)
		{
		    w->priv->handleSyncAlarm ();
		    break;
		}
	    }
	}
	break;
    }
}
