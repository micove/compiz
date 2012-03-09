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
#include <core/valueholder.h>

#include <boost/scoped_ptr.hpp>

class CompScreenImpl;
class PrivateScreen;
class CompManager;
class CoreWindow;

typedef std::list<CompWindow *> CompWindowList;
typedef std::vector<CompWindow *> CompWindowVector;

extern bool       replaceCurrentWm;
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

class CompScreen :
    public WrapableHandler<ScreenInterface, 18>,
    public PluginClassStorage, // TODO should be an interface here
    public CompSize,
    public CompOption::Class   // TODO should be an interface here
{
public:
    typedef void* GrabHandle;

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

    unsigned int allocPluginClassIndex ();
    void freePluginClassIndex (unsigned int index);
    static int checkForError (Display *dpy);


    // Interface hoisted from CompScreen
    virtual bool updateDefaultIcon () = 0;
    virtual Display * dpy () = 0;
    virtual Window root () = 0;
    virtual const CompSize  & vpSize () const = 0;
    virtual void forEachWindow (CompWindow::ForEach) =0;
    virtual CompWindowList & windows () = 0;
    virtual void moveViewport (int tx, int ty, bool sync) = 0;
    virtual const CompPoint & vp () const = 0;
    virtual void updateWorkarea () = 0;
    virtual bool addAction (CompAction *action) = 0;
    virtual CompWindow * findWindow (Window id) = 0;

    virtual CompWindow * findTopLevelWindow (
	    Window id, bool   override_redirect = false) = 0;
    virtual void toolkitAction (
	    Atom   toolkitAction,
	    Time   eventTime,
	    Window window,
	    long   data0,
	    long   data1,
	    long   data2) = 0;
    virtual unsigned int showingDesktopMask() const = 0;

    virtual bool grabsEmpty() const = 0;
    virtual void sizePluginClasses(unsigned int size) = 0;
    virtual CompOutput::vector & outputDevs () = 0;
    virtual void setWindowState (unsigned int state, Window id) = 0;
    virtual bool XShape () = 0;
    virtual std::vector<XineramaScreenInfo> & screenInfo () = 0;
    virtual CompWindowList & serverWindows () = 0;
    virtual void setWindowProp (Window       id,
			    Atom         property,
			    unsigned int value) = 0;
    virtual Window activeWindow () = 0;
    virtual unsigned int currentDesktop () = 0;
    virtual CompActiveWindowHistory *currentHistory () = 0;
    virtual void focusDefaultWindow () = 0;
    virtual Time getCurrentTime () = 0;
    virtual unsigned int getWindowProp (Window       id,
				    Atom         property,
				    unsigned int defaultValue) = 0;
    virtual void insertServerWindow (CompWindow *w, Window aboveId) = 0;
    virtual void insertWindow (CompWindow *w, Window aboveId) = 0;
    virtual unsigned int nDesktop () = 0;
    virtual int outputDeviceForGeometry (const CompWindow::Geometry& gm) = 0;
    virtual int screenNum () = 0;
    virtual void unhookServerWindow (CompWindow *w) = 0;
    virtual void unhookWindow (CompWindow *w) = 0;
    virtual void viewportForGeometry (const CompWindow::Geometry &gm,
				  CompPoint                   &viewport) = 0;

    virtual void removeFromCreatedWindows(CoreWindow *cw) = 0;
    virtual void addToDestroyedWindows(CompWindow * cw) = 0;
    virtual const CompRect & workArea () const = 0;
    virtual void removeAction (CompAction *action) = 0;
    virtual CompOption::Vector & getOptions () = 0;
    virtual bool setOption (const CompString &name, CompOption::Value &value) = 0;
    virtual void storeValue (CompString key, CompPrivate value) = 0;
    virtual bool hasValue (CompString key) = 0;
    virtual CompPrivate getValue (CompString key) = 0;
    virtual void eraseValue (CompString key) = 0;
    virtual CompWatchFdHandle addWatchFd (int             fd,
				      short int       events,
				      FdWatchCallBack callBack) = 0;
    virtual void removeWatchFd (CompWatchFdHandle handle) = 0;
    virtual void eventLoop () = 0;

    virtual CompFileWatchHandle addFileWatch (const char        *path,
					  int               mask,
					  FileWatchCallBack callBack) = 0;
    virtual void removeFileWatch (CompFileWatchHandle handle) = 0;
    virtual const CompFileWatchList& getFileWatches () const = 0;
    virtual void updateSupportedWmHints () = 0;

    virtual CompWindowList & destroyedWindows () = 0;
    virtual const CompRegion & region () const = 0;
    virtual bool hasOverlappingOutputs () = 0;
    virtual CompOutput & fullscreenOutput () = 0;
    virtual void setWindowProp32 (Window         id,
			      Atom           property,
			      unsigned short value) = 0;
    virtual unsigned short getWindowProp32 (Window         id,
					Atom           property,
					unsigned short defaultValue) = 0;
    virtual bool readImageFromFile (CompString &name,
				CompString &pname,
				CompSize   &size,
				void       *&data) = 0;
    virtual int desktopWindowCount () = 0;
    virtual XWindowAttributes attrib () = 0;
    virtual CompIcon *defaultIcon () const = 0;
    virtual bool otherGrabExist (const char *, ...) = 0;
    virtual GrabHandle pushGrab (Cursor cursor, const char *name) = 0;
    virtual void removeGrab (GrabHandle handle, CompPoint *restorePointer) = 0;
    virtual bool writeImageToFile (CompString &path,
			       const char *format,
			       CompSize   &size,
			       void       *data) = 0;
    virtual void runCommand (CompString command) = 0;
    virtual bool shouldSerializePlugins () = 0;
    virtual const CompRect & getWorkareaForOutput (unsigned int outputNum) const = 0;
    virtual CompOutput & currentOutputDev () const = 0;
    virtual bool grabExist (const char *) = 0;
    virtual Cursor invisibleCursor () = 0;
    virtual unsigned int activeNum () const = 0;
    virtual void sendWindowActivationRequest (Window id) = 0;
    virtual const CompWindowVector & clientList (bool stackingOrder = true) = 0;
    virtual int outputDeviceForPoint (const CompPoint &point) = 0;
    virtual int outputDeviceForPoint (int x, int y) = 0;
    virtual int xkbEvent () = 0;
    virtual void warpPointer (int dx, int dy) = 0;
    virtual void updateGrab (GrabHandle handle, Cursor cursor) = 0;
    virtual int shapeEvent () = 0;

    virtual int syncEvent () = 0;
    virtual Window autoRaiseWindow () = 0;

    virtual const char * displayString () = 0;
    virtual CompRect getCurrentOutputExtents () = 0;
    virtual Cursor normalCursor () = 0;
    virtual bool grabbed () = 0;
    virtual SnDisplay * snDisplay () = 0;

    friend class CompWindow; // TODO get rid of friends
    friend class PrivateWindow; // TODO get rid of friends
    friend class ModifierHandler; // TODO get rid of friends
    friend class CompManager; // TODO get rid of friends

    virtual void processEvents () = 0;
    virtual void alwaysHandleEvent (XEvent *event) = 0;

    bool displayInitialised() const;
protected:
	CompScreen();
	boost::scoped_ptr<PrivateScreen> priv; // TODO should not be par of interface

private:
    // The "wrapable" functions delegate to these (for mocking)
    virtual bool _initPluginForScreen(CompPlugin *) = 0;
    virtual void _finiPluginForScreen(CompPlugin *) = 0;
    virtual bool _setOptionForPlugin(const char *, const char *, CompOption::Value &) = 0;
    virtual void _handleEvent(XEvent *event) = 0;
    virtual void _logMessage(const char *, CompLogLevel, const char*) = 0;
    virtual void _enterShowDesktopMode() = 0;
    virtual void _leaveShowDesktopMode(CompWindow *) = 0;
    virtual void _addSupportedAtoms(std::vector<Atom>& atoms) = 0;

    virtual void _fileWatchAdded(CompFileWatch *) = 0;
    virtual void _fileWatchRemoved(CompFileWatch *) = 0;
    virtual void _sessionEvent(CompSession::Event, CompOption::Vector &) = 0;
    virtual void _handleCompizEvent(const char *, const char *, CompOption::Vector &) = 0;
    virtual bool _fileToImage(CompString &, CompSize &, int &, void *&) = 0;
    virtual bool _imageToFile(CompString &, CompString &, CompSize &, int, void *) = 0;
    virtual CompMatch::Expression * _matchInitExp(const CompString&) = 0;
    virtual void _matchExpHandlerChanged() = 0;
    virtual void _matchPropertyChanged(CompWindow *) = 0;
    virtual void _outputChangeNotify() = 0;
};

/**
 * A wrapping of the X display screen. This takes care of communication to the
 * X server.
 */
class CompScreenImpl : public CompScreen
{
    public:
	CompScreenImpl ();
	~CompScreenImpl ();

	bool init (const char *name);

	void eventLoop ();

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
	CompWindowList & serverWindows ();
	CompWindowList & destroyedWindows ();

	void warpPointer (int dx, int dy);

	Time getCurrentTime ();

	Window selectionWindow ();

	void forEachWindow (CompWindow::ForEach);

	void focusDefaultWindow ();

	void insertWindow (CompWindow *w, Window aboveId);
	void unhookWindow (CompWindow *w);

	void insertServerWindow (CompWindow *w, Window aboveId);
	void unhookServerWindow (CompWindow *w);

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

	const CompPoint & vp () const;

	const CompSize  & vpSize () const;

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

	unsigned int showingDesktopMask() const;
	virtual bool grabsEmpty() const;
	virtual void sizePluginClasses(unsigned int size);
	virtual void setWindowState (unsigned int state, Window id);
	virtual void removeFromCreatedWindows(CoreWindow *cw);
	virtual void addToDestroyedWindows(CompWindow * cw);
	virtual void processEvents ();
	virtual void alwaysHandleEvent (XEvent *event);

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

    private:
        virtual bool _setOptionForPlugin(const char *, const char *, CompOption::Value &);
        virtual bool _initPluginForScreen(CompPlugin *);
        virtual void _finiPluginForScreen(CompPlugin *);
        virtual void _handleEvent(XEvent *event);
        virtual void _logMessage(const char *, CompLogLevel, const char*);
        virtual void _enterShowDesktopMode();
        virtual void _leaveShowDesktopMode(CompWindow *);
        virtual void _addSupportedAtoms(std::vector<Atom>& atoms);

        // These are stubs - but allow mocking of AbstractCompWindow
        virtual void _fileWatchAdded(CompFileWatch *);
        virtual void _fileWatchRemoved(CompFileWatch *);
        virtual void _sessionEvent(CompSession::Event, CompOption::Vector &);
        virtual void _handleCompizEvent(const char *, const char *, CompOption::Vector &);
        virtual bool _fileToImage(CompString &, CompSize &, int &, void *&);
        virtual bool _imageToFile(CompString &, CompString &, CompSize &, int, void *);
        virtual CompMatch::Expression * _matchInitExp(const CompString&);
        virtual void _matchExpHandlerChanged();
        virtual void _matchPropertyChanged(CompWindow *);
        virtual void _outputChangeNotify();

};

#endif
