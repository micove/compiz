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

#ifndef _COMPSCREEN_H
#define _COMPSCREEN_H

#include <core/window.h>
#include <core/output.h>
#include <core/session.h>
#include <core/plugin.h>
#include <core/match.h>
#include <core/pluginclasses.h>
#include <core/region.h>
#include <core/modifierhandler.h>

class CompScreen;
class PrivateScreen;
class CompManager;

typedef std::list<CompWindow *> CompWindowList;
typedef std::vector<CompWindow *> CompWindowVector;

extern char       *backgroundImage;
extern bool       replaceCurrentWm;
extern bool       indirectRendering;
extern bool       noDetection;
extern bool       debugOutput;

extern CompScreen   *screen;
extern ModifierHandler *modHandler;

extern int lastPointerX;
extern int lastPointerY;
extern unsigned int lastPointerMods;
extern int pointerX;
extern int pointerY;
extern unsigned int pointerMods;

#define NOTIFY_CREATE_MASK (1 << 0)
#define NOTIFY_DELETE_MASK (1 << 1)
#define NOTIFY_MOVE_MASK   (1 << 2)
#define NOTIFY_MODIFY_MASK (1 << 3)

typedef boost::function<void (short int)> FdWatchCallBack;
typedef boost::function<void (const char *)> FileWatchCallBack;

typedef int CompFileWatchHandle;
typedef int CompWatchFdHandle;

/**
 * Information needed to invoke a CallBack when a file changes.
 */
struct CompFileWatch {
    CompString		path;
    int			mask;
    FileWatchCallBack   callBack;
    CompFileWatchHandle handle;
};
typedef std::list<CompFileWatch *> CompFileWatchList;

#define ACTIVE_WINDOW_HISTORY_SIZE 64
#define ACTIVE_WINDOW_HISTORY_NUM  32

/**
 * Information about the last activity with a window.
 */
struct CompActiveWindowHistory {
    Window id[ACTIVE_WINDOW_HISTORY_SIZE];
    int    x;
    int    y;
    int    activeNum;
};

/**
 * Interface to an abstract screen.
 */
class ScreenInterface : public WrapableInterface<CompScreen, ScreenInterface> {
    public:
	virtual void fileWatchAdded (CompFileWatch *fw);
	virtual void fileWatchRemoved (CompFileWatch *fw);

	virtual bool initPluginForScreen (CompPlugin *p);
	virtual void finiPluginForScreen (CompPlugin *p);

	virtual bool setOptionForPlugin (const char *plugin,
					 const char *name,
					 CompOption::Value &v);

	virtual void sessionEvent (CompSession::Event event,
				   CompOption::Vector &options);

	virtual void handleEvent (XEvent *event);
        virtual void handleCompizEvent (const char * plugin, const char *event,
					CompOption::Vector &options);

        virtual bool fileToImage (CompString &path, CompSize &size,
				  int &stride, void *&data);
	virtual bool imageToFile (CompString &path, CompString &format,
				  CompSize &size, int stride, void *data);

	virtual CompMatch::Expression *matchInitExp (const CompString& value);

	virtual void matchExpHandlerChanged ();
	virtual void matchPropertyChanged (CompWindow *window);

	virtual void logMessage (const char   *componentName,
				 CompLogLevel level,
				 const char   *message);

	virtual void enterShowDesktopMode ();
	virtual void leaveShowDesktopMode (CompWindow *window);

	virtual void outputChangeNotify ();
	virtual void addSupportedAtoms (std::vector<Atom>& atoms);
};

/**
 * A wrapping of the X display screen. This takes care of communication to the
 * X server.
 */
class CompScreen :
    public CompSize,
    public WrapableHandler<ScreenInterface, 18>,
    public PluginClassStorage,
    public CompOption::Class
{

    public:
	typedef void* GrabHandle;

    public:
	CompScreen ();
	~CompScreen ();

	bool init (const char *name);

	void eventLoop ();
	bool processEvents ();

	CompFileWatchHandle addFileWatch (const char        *path,
					  int               mask,
					  FileWatchCallBack callBack);

	void removeFileWatch (CompFileWatchHandle handle);

	const CompFileWatchList& getFileWatches () const;

	CompWatchFdHandle addWatchFd (int             fd,
				      short int       events,
				      FdWatchCallBack callBack);

	void removeWatchFd (CompWatchFdHandle handle);

	void storeValue (CompString key, CompPrivate value);
	bool hasValue (CompString key);
	CompPrivate getValue (CompString key);
	void eraseValue (CompString key);

	Display * dpy ();

	CompOption::Vector & getOptions ();

	bool setOption (const CompString &name, CompOption::Value &value);

	bool XRandr ();

	int randrEvent ();

	bool XShape ();

	int shapeEvent ();

	int syncEvent ();

	SnDisplay * snDisplay ();

	Window activeWindow ();

	Window autoRaiseWindow ();

	const char * displayString ();


	CompWindow * findWindow (Window id);

	CompWindow * findTopLevelWindow (Window id,
					 bool   override_redirect = false);

	bool readImageFromFile (CompString &name,
				CompString &pname,
				CompSize   &size,
				void       *&data);

	bool writeImageToFile (CompString &path,
			       const char *format,
			       CompSize   &size,
			       void       *data);

	unsigned int getWindowProp (Window       id,
				    Atom         property,
				    unsigned int defaultValue);


	void setWindowProp (Window       id,
			    Atom         property,
			    unsigned int value);


	unsigned short getWindowProp32 (Window         id,
					Atom           property,
					unsigned short defaultValue);


	void setWindowProp32 (Window         id,
			      Atom           property,
			      unsigned short value);

	Window root ();

	int xkbEvent ();

	XWindowAttributes attrib ();

	int screenNum ();

	CompWindowList & windows ();

	void warpPointer (int dx, int dy);

	Time getCurrentTime ();

	Window selectionWindow ();

	void forEachWindow (CompWindow::ForEach);

	void focusDefaultWindow ();

	void insertWindow (CompWindow *w, Window aboveId);

	void unhookWindow (CompWindow *w);

	Cursor normalCursor ();

	Cursor invisibleCursor ();

	/* Adds an X Pointer and Keyboard grab to the stack. Since
	 * compiz as a client only need to grab once, multiple clients
	 * can call this and all get events, but the pointer will
	 * be grabbed once and the actual grab refcounted */
	GrabHandle pushGrab (Cursor cursor, const char *name);

	/* Allows you to change the pointer of your grab */
	void updateGrab (GrabHandle handle, Cursor cursor);

	/* Removes your grab from the stack. Once the internal refcount
	 * reaches zero, the X Pointer and Keyboard are both ungrabbed
	 */
	void removeGrab (GrabHandle handle, CompPoint *restorePointer);

	/* Returns true if a grab other than the grabs specified here
	 * exists */
	bool otherGrabExist (const char *, ...);

	/* Returns true if the specified grab exists */
	bool grabExist (const char *);

	/* Returns true if the X Pointer and / or Keyboard is grabbed
	 * by anything (another application, pluigins etc) */
	bool grabbed ();

	const CompWindowVector & clientList (bool stackingOrder = true);

	bool addAction (CompAction *action);

	void removeAction (CompAction *action);

	void updateWorkarea ();

	void toolkitAction (Atom   toolkitAction,
			    Time   eventTime,
			    Window window,
			    long   data0,
			    long   data1,
			    long   data2);

	void runCommand (CompString command);

	void moveViewport (int tx, int ty, bool sync);

	void sendWindowActivationRequest (Window id);

	int outputDeviceForPoint (int x, int y);
	int outputDeviceForPoint (const CompPoint &point);

	CompRect getCurrentOutputExtents ();

	const CompRect & getWorkareaForOutput (unsigned int outputNum) const;

	void viewportForGeometry (const CompWindow::Geometry &gm,
				  CompPoint                   &viewport);

	int outputDeviceForGeometry (const CompWindow::Geometry& gm);

	CompPoint vp ();

	CompSize vpSize ();

	int desktopWindowCount ();
	unsigned int activeNum () const;

	CompOutput::vector & outputDevs ();
	CompOutput & currentOutputDev () const;

	const CompRect & workArea () const;

	unsigned int currentDesktop ();

	unsigned int nDesktop ();

	CompActiveWindowHistory *currentHistory ();

	bool shouldSerializePlugins () ;

	const CompRegion & region () const;

	bool hasOverlappingOutputs ();

	CompOutput & fullscreenOutput ();

	std::vector<XineramaScreenInfo> & screenInfo ();

	CompIcon *defaultIcon () const;

	bool updateDefaultIcon ();

	void updateSupportedWmHints ();

	static unsigned int allocPluginClassIndex ();
	static void freePluginClassIndex (unsigned int index);

	WRAPABLE_HND (0, ScreenInterface, void, fileWatchAdded, CompFileWatch *)
	WRAPABLE_HND (1, ScreenInterface, void, fileWatchRemoved, CompFileWatch *)

	WRAPABLE_HND (2, ScreenInterface, bool, initPluginForScreen,
		      CompPlugin *)
	WRAPABLE_HND (3, ScreenInterface, void, finiPluginForScreen,
		      CompPlugin *)

	WRAPABLE_HND (4, ScreenInterface, bool, setOptionForPlugin,
		      const char *, const char *, CompOption::Value &)

	WRAPABLE_HND (5, ScreenInterface, void, sessionEvent, CompSession::Event,
		      CompOption::Vector &)
	WRAPABLE_HND (6, ScreenInterface, void, handleEvent, XEvent *event)
	WRAPABLE_HND (7, ScreenInterface, void, handleCompizEvent,
		      const char *, const char *, CompOption::Vector &)

	WRAPABLE_HND (8, ScreenInterface, bool, fileToImage, CompString &,
		      CompSize &, int &, void *&);
	WRAPABLE_HND (9, ScreenInterface, bool, imageToFile, CompString &,
		      CompString &, CompSize &, int, void *);

	WRAPABLE_HND (10, ScreenInterface, CompMatch::Expression *,
		      matchInitExp, const CompString&);
	WRAPABLE_HND (11, ScreenInterface, void, matchExpHandlerChanged)
	WRAPABLE_HND (12, ScreenInterface, void, matchPropertyChanged,
		      CompWindow *)

	WRAPABLE_HND (13, ScreenInterface, void, logMessage, const char *,
		      CompLogLevel, const char*)
	WRAPABLE_HND (14, ScreenInterface, void, enterShowDesktopMode);
	WRAPABLE_HND (15, ScreenInterface, void, leaveShowDesktopMode,
		      CompWindow *);

	WRAPABLE_HND (16, ScreenInterface, void, outputChangeNotify);
	WRAPABLE_HND (17, ScreenInterface, void, addSupportedAtoms,
		      std::vector<Atom>& atoms);

	friend class CompTimer;
	friend class CompWindow;
	friend class PrivateWindow;
	friend class CoreWindow;
	friend class ModifierHandler;
	friend class CompEventSource;
	friend class CompTimeoutSource;
	friend class CompManager;
	friend class CompWatchFd;

    private:
	PrivateScreen *priv;

    public :

	static bool showDesktop (CompAction         *action,
				 CompAction::State  state,
				 CompOption::Vector &options);

	static bool windowMenu (CompAction         *action,
				CompAction::State  state,
				CompOption::Vector &options);

	static bool closeWin (CompAction         *action,
			      CompAction::State  state,
			      CompOption::Vector &options);

	static bool unmaximizeWin (CompAction         *action,
				   CompAction::State  state,
				   CompOption::Vector &options);

	static bool minimizeWin (CompAction         *action,
				 CompAction::State  state,
				 CompOption::Vector &options);

	static bool maximizeWin (CompAction         *action,
				 CompAction::State  state,
				 CompOption::Vector &options);

	static bool maximizeWinHorizontally (CompAction         *action,
					     CompAction::State  state,
					     CompOption::Vector &options);

	static bool maximizeWinVertically (CompAction         *action,
					   CompAction::State  state,
					   CompOption::Vector &options);

	static bool raiseWin (CompAction         *action,
			      CompAction::State  state,
			      CompOption::Vector &options);

	static bool lowerWin (CompAction         *action,
			      CompAction::State  state,
			      CompOption::Vector &options);

	static bool toggleWinMaximized (CompAction         *action,
					CompAction::State  state,
					CompOption::Vector &options);

	static bool toggleWinMaximizedHorizontally (CompAction         *action,
						    CompAction::State  state,
						    CompOption::Vector &options);

	static bool toggleWinMaximizedVertically (CompAction         *action,
					          CompAction::State  state,
					          CompOption::Vector &options);

	static bool shadeWin (CompAction         *action,
			      CompAction::State  state,
			      CompOption::Vector &options);

	static void
	compScreenSnEvent (SnMonitorEvent *event,
			   void           *userData);

	static int checkForError (Display *dpy);
};

#endif
