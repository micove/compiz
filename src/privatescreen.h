/*
 * Copyright © 2008 Dennis Kasprzyk
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Dennis Kasprzyk not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Dennis Kasprzyk makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DENNIS KASPRZYK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Dennis Kasprzyk <onestone@compiz-fusion.org>
 *          David Reveman <davidr@novell.com>
 */

#ifndef _PRIVATESCREEN_H
#define _PRIVATESCREEN_H

#include <core/screen.h>
#include <core/size.h>
#include <core/point.h>
#include <core/timer.h>
#include <core/plugin.h>
#include <time.h>
#include <boost/shared_ptr.hpp>

#include <glibmm/main.h>

#include "privatetimeoutsource.h"
#include "privateiosource.h"
#include "privateeventsource.h"
#include "privatesignalsource.h"

#include "core_options.h"

CompPlugin::VTable * getCoreVTable ();

class CoreWindow;

extern bool shutDown;
extern bool restartSignal;

extern CompWindow *lastFoundWindow;
extern bool	  useDesktopHints;

extern std::list <CompString> initialPlugins;


typedef struct _CompDelayedEdgeSettings
{
    CompAction::CallBack initiate;
    CompAction::CallBack terminate;

    unsigned int edge;
    unsigned int state;

    CompOption::Vector options;
} CompDelayedEdgeSettings;


#define SCREEN_EDGE_LEFT	0
#define SCREEN_EDGE_RIGHT	1
#define SCREEN_EDGE_TOP		2
#define SCREEN_EDGE_BOTTOM	3
#define SCREEN_EDGE_TOPLEFT	4
#define SCREEN_EDGE_TOPRIGHT	5
#define SCREEN_EDGE_BOTTOMLEFT	6
#define SCREEN_EDGE_BOTTOMRIGHT 7
#define SCREEN_EDGE_NUM		8

struct CompScreenEdge {
    Window	 id;
    unsigned int count;
};

struct CompGroup {
    unsigned int      refCnt;
    Window	      id;
};

struct CompStartupSequence {
    SnStartupSequence		*sequence;
    unsigned int		viewportX;
    unsigned int		viewportY;
};

namespace compiz
{
namespace core
{
namespace screen
{
    inline int wraparound_mod (int a, int b)
    {
	if (a < 0)
	    return (b - ((-a - 1) % (b))) - 1;
	else
	    return a % b;
    };
}
}

namespace X11
{
class PendingEvent {
public:
    PendingEvent (Display *, Window);
    virtual ~PendingEvent ();

    virtual bool match (XEvent *);
    unsigned int serial () { return mSerial; } // HACK: will be removed
    virtual void dump ();

    typedef boost::shared_ptr<PendingEvent> Ptr;

protected:

    virtual Window getEventWindow (XEvent *);

    unsigned int mSerial;
    Window       mWindow;
};

class PendingConfigureEvent :
    public PendingEvent
{
public:
    PendingConfigureEvent (Display *, Window, unsigned int, XWindowChanges *);
    virtual ~PendingConfigureEvent ();

    virtual bool match (XEvent *);
    bool matchVM (unsigned int valueMask);
    bool matchRequest (XWindowChanges &xwc, unsigned int);
    virtual void dump ();

    typedef boost::shared_ptr<PendingConfigureEvent> Ptr;

protected:

    virtual Window getEventWindow (XEvent *);

private:
    unsigned int mValueMask;
    XWindowChanges mXwc;
};

class PendingEventQueue
{
public:

    PendingEventQueue (Display *);
    virtual ~PendingEventQueue ();

    void add (PendingEvent::Ptr p);
    bool match (XEvent *);
    bool pending ();
    bool forEachIf (boost::function <bool (compiz::X11::PendingEvent::Ptr)>);
    void clear () { mEvents.clear (); } // HACK will be removed
    void dump ();

protected:
    bool removeIfMatching (const PendingEvent::Ptr &p, XEvent *);

private:
    std::list <PendingEvent::Ptr> mEvents;
};

}
}

namespace compiz
{
namespace private_screen
{

class WindowManager : boost::noncopyable
{
    public:

	WindowManager();

	CompGroup * addGroup (Window id);
	void removeGroup (CompGroup *group);
	CompGroup * findGroup (Window id);

	void eraseWindowFromMap (Window id);
	void removeDestroyed ();

    //private:
	Window activeWindow;
	Window nextActiveWindow;

	Window below;

	CompTimer autoRaiseTimer;
	Window    autoRaiseWindow;

	CompWindowList serverWindows;
	CompWindowList destroyedWindows;
	bool           stackIsFresh;

	CompWindow::Map windowsMap;
	std::list<CompGroup *> groups;

	std::map <CompWindow *, CompWindow *> detachedFrameWindows;

	CompWindowVector clientList;            /* clients in mapping order */
	CompWindowVector clientListStacking;    /* clients in stacking order */

	std::vector<Window> clientIdList;        /* client ids in mapping order */
	std::vector<Window> clientIdListStacking;/* client ids in stacking order */

	unsigned int pendingDestroys;
};

// data members that don't belong (these probably belong
// in CompScreenImpl as PrivateScreen doesn't use them)
struct OrphanData : boost::noncopyable
{
	OrphanData();

	Window	      edgeWindow;
	Window	      xdndWindow;
};

// Static member functions that don't belong (use no data,
// not invoked by PrivateScreen)
struct PseudoNamespace
{
    // AFAICS only used by CoreExp (in match.cpp)
    static unsigned int windowStateFromString (const char *str);
};

class PluginManager :
    public CoreOptions
{
    public:
	PluginManager();

	void updatePlugins ();

    //private:
	CompOption::Value plugin;
	bool	          dirtyPluginList;
	void *possibleTap;
};

class EventManager :
    public PluginManager,
    public ValueHolder
{
public:
	EventManager (CompScreen *screen);
	~EventManager ();

	bool init (const char *name);

	void handleSignal (int signum);

public:

	Glib::RefPtr <Glib::MainLoop>  mainloop;

	/* We cannot use RefPtrs. See
	 * https://bugzilla.gnome.org/show_bug.cgi?id=561885
	 */
	CompEventSource * source;
	CompTimeoutSource * timeout;
	CompSignalSource * sighupSource;
	CompSignalSource * sigtermSource;
	CompSignalSource * sigintSource;
	Glib::RefPtr <Glib::MainContext> ctx;

	CompFileWatchList   fileWatch;
	CompFileWatchHandle lastFileWatchHandle;

	std::list< CompWatchFd * > watchFds;
	CompWatchFdHandle        lastWatchFdHandle;

	CompTimer    pingTimer;

	CompTimer               edgeDelayTimer;
	CompDelayedEdgeSettings edgeDelaySettings;


	CompScreen  *screen;

	int          desktopWindowCount;
	unsigned int mapNum;


	std::list<CompGroup *> groups;

	CompIcon *defaultIcon;

	bool  tapGrab;

    private:
	virtual bool initDisplay (const char *name);
};

}} // namespace compiz::private_screen

class PrivateScreen :
    public compiz::private_screen::EventManager,
    public compiz::private_screen::WindowManager,
    public compiz::private_screen::OrphanData,
    public compiz::private_screen::PseudoNamespace
{

    public:
	class KeyGrab {
	    public:
		int          keycode;
		unsigned int modifiers;
		int          count;
	};

	class ButtonGrab {
	    public:
		int          button;
		unsigned int modifiers;
		int          count;
	};

	class Grab {
	    public:

		friend class CompScreenImpl;
	    private:
		Cursor     cursor;
		const char *name;
	};

    public:
	PrivateScreen (CompScreen *screen);
	~PrivateScreen ();

	bool setOption (const CompString &name, CompOption::Value &value);

	std::list <XEvent> queueEvents ();
	void processEvents ();

	bool triggerPress   (CompAction         *action,
	                     CompAction::State   state,
	                     CompOption::Vector &arguments);
	bool triggerRelease (CompAction         *action,
	                     CompAction::State   state,
	                     CompOption::Vector &arguments);

	bool triggerButtonPressBindings (CompOption::Vector &options,
					 XButtonEvent       *event,
					 CompOption::Vector &arguments);

	bool triggerButtonReleaseBindings (CompOption::Vector &options,
					   XButtonEvent       *event,
					   CompOption::Vector &arguments);

	bool triggerKeyPressBindings (CompOption::Vector &options,
				      XKeyEvent          *event,
				      CompOption::Vector &arguments);

	bool triggerKeyReleaseBindings (CompOption::Vector &options,
					XKeyEvent          *event,
					CompOption::Vector &arguments);

	bool triggerStateNotifyBindings (CompOption::Vector  &options,
					 XkbStateNotifyEvent *event,
					 CompOption::Vector  &arguments);

	bool triggerEdgeEnter (unsigned int       edge,
			       CompAction::State  state,
			       CompOption::Vector &arguments);

	void setAudibleBell (bool audible);

	bool handlePingTimeout ();

	bool handleActionEvent (XEvent *event);

	void handleSelectionRequest (XEvent *event);

	void handleSelectionClear (XEvent *event);

	bool desktopHintEqual (unsigned long *data,
			       int           size,
			       int           offset,
			       int           hintSize);

	void setDesktopHints ();

	void setVirtualScreenSize (int hsize, int vsize);

	void updateOutputDevices ();

	void detectOutputDevices ();

	void updateStartupFeedback ();

	void updateScreenEdges ();

	void reshape (int w, int h);

	bool handleStartupSequenceTimeout ();

	void addSequence (SnStartupSequence *sequence);

	void removeSequence (SnStartupSequence *sequence);

	void removeAllSequences ();

	void setSupportingWmCheck ();

	void getDesktopHints ();

	void grabUngrabOneKey (unsigned int modifiers,
			       int          keycode,
			       bool         grab);


	bool grabUngrabKeys (unsigned int modifiers,
			     int          keycode,
			     bool         grab);

	bool addPassiveKeyGrab (CompAction::KeyBinding &key);

	void removePassiveKeyGrab (CompAction::KeyBinding &key);

	void updatePassiveKeyGrabs ();

	bool addPassiveButtonGrab (CompAction::ButtonBinding &button);

	void removePassiveButtonGrab (CompAction::ButtonBinding &button);

	CompRect computeWorkareaForBox (const CompRect &box);

	void updateScreenInfo ();

	Window getActiveWindow (Window root);

	int getWmState (Window id);

	void setWmState (int state, Window id);

	unsigned int windowStateMask (Atom state);

	unsigned int getWindowState (Window id);

	void setWindowState (unsigned int state, Window id);

	unsigned int getWindowType (Window id);

	void getMwmHints (Window       id,
			  unsigned int *func,
			  unsigned int *decor);

	unsigned int getProtocols (Window id);

	bool readWindowProp32 (Window         id,
			       Atom           property,
			       unsigned short *returnValue);

	void setCurrentOutput (unsigned int outputNum);

	void configure (XConfigureEvent *ce);

	void updateClientList ();

	void applyStartupProperties (CompWindow *window);

	Window getTopWindow ();

	void setNumberOfDesktops (unsigned int nDesktop);

	void setCurrentDesktop (unsigned int desktop);

	void setCurrentActiveWindowHistory (int x, int y);

	void addToCurrentActiveWindowHistory (Window id);

	void enableEdge (int edge);

	void disableEdge (int edge);

	CompWindow *
	focusTopMostWindow ();

	bool
	createFailed ();
	
	void setDefaultWindowAttributes (XWindowAttributes *);

	static void compScreenSnEvent (SnMonitorEvent *event,
			   void           *userData);

    public:

	Display    *dpy;

	int syncEvent, syncError;

	bool randrExtension;
	int  randrEvent, randrError;

	bool shapeExtension;
	int  shapeEvent, shapeError;

	bool xkbExtension;
	int  xkbEvent, xkbError;

	bool xineramaExtension;
	int  xineramaEvent, xineramaError;

	std::vector<XineramaScreenInfo> screenInfo;

	SnDisplay *snDisplay;

	unsigned int lastPing;
	char   displayString[256];

	KeyCode escapeKeyCode;
	KeyCode returnKeyCode;

	std::list <CoreWindow *> createdWindows;
	CompWindowList windows;

	Colormap colormap;
	int      screenNum;

	CompPoint    vp;
	CompSize     vpSize;
	unsigned int nDesktop;
	unsigned int currentDesktop;
	CompRegion   region;

	Window	      root;

	XWindowAttributes attrib;
	Window            grabWindow;

	unsigned int activeNum;

	CompOutput::vector outputDevs;
	int	           currentOutputDev;
	CompOutput         fullscreenOutput;
	bool               hasOverlappingOutputs;

	CompActiveWindowHistory history[ACTIVE_WINDOW_HISTORY_NUM];
	int                     currentHistory;

	CompScreenEdge screenEdge[SCREEN_EDGE_NUM];

	SnMonitorContext                 *snContext;
	std::list<CompStartupSequence *> startupSequences;
	CompTimer                        startupSequenceTimer;

	Window wmSnSelectionWindow;
	Atom   wmSnAtom;
	Time   wmSnTimestamp;

	Cursor normalCursor;
	Cursor busyCursor;
	Cursor invisibleCursor;

	std::list<ButtonGrab> buttonGrabs;
	std::list<KeyGrab>    keyGrabs;

	std::list<Grab *> grabs;

	bool		      grabbed; /* true once we recieve a GrabNotify
					  on FocusOut and false on
					  UngrabNotify from FocusIn */

	CompRect workArea;

	unsigned int showingDesktopMask;

	unsigned long *desktopHintData;
	int           desktopHintSize;

	bool eventHandled;

	bool initialized;

    private:
	virtual bool initDisplay (const char *name);
};

class CompManager
{
    public:

	CompManager ();

	bool init ();
	void run ();
	void fini ();

	bool parseArguments (int, char **);
	void usage ();

	static bool initPlugin (CompPlugin *p);
	static void finiPlugin (CompPlugin *p);

    private:

	std::list <CompString> plugins;
	bool		       disableSm;
	char		       *clientId;
	char		       *displayName;
};

#endif
