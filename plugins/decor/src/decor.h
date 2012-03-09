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

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <core/window.h>
#include <core/pluginclasshandler.h>

#include <composite/composite.h>
#include <opengl/opengl.h>
#include <core/atoms.h>
#include <core/windowextents.h>

#include "decor_options.h"

#define DECOR_SCREEN(s) DecorScreen *ds = DecorScreen::get(s)
#define DECOR_WINDOW(w) DecorWindow *dw = DecorWindow::get(w)

struct Vector {
    int	dx;
    int	dy;
    int	x0;
    int	y0;
};

/* FIXME: Remove */
#define DECOR_BARE   0
#define DECOR_ACTIVE 1
#define DECOR_NUM    2

class DecorTexture {

    public:
	DecorTexture (Pixmap pixmap);
	~DecorTexture ();

    public:
	bool            status;
	int             refCount;
        Pixmap          pixmap;
	Damage          damage;
	GLTexture::List textures;
};

class DecorWindow;

class Decoration {

    public:

	typedef boost::shared_ptr <Decoration> Ptr;

	static Decoration::Ptr create (Window        id,
				       long          *prop,
				       unsigned int  size,
				       unsigned int  type,
				       unsigned int  nOffset);

	Decoration (int   type,
		    const decor_extents_t &border,
		    const decor_extents_t &input,
		    const decor_extents_t &maxBorder,
		    const decor_extents_t &maxInput,
		    unsigned int frameType,
		    unsigned int frameState,
		    unsigned int frameActions,
		    unsigned int minWidth,
		    unsigned int minHeight,
		    Pixmap       pixmap,
		    const boost::shared_array <decor_quad_t> &quad,
		    unsigned int nQuad);

	~Decoration ();

    public:
	int                       refCount;
	DecorTexture              *texture;
	CompWindowExtents         output;
	CompWindowExtents         border;
	CompWindowExtents	  input;
	CompWindowExtents         maxBorder;
	CompWindowExtents	  maxInput;
	int                       minWidth;
	int                       minHeight;
	unsigned int		  frameType;
	unsigned int		  frameState;
	unsigned int		  frameActions;
	boost::shared_array <decor_quad_t> quad;
	int                       nQuad;
	int                       type;
};

class DecorationList
{
    public:
        bool updateDecoration  (Window id, Atom decorAtom);
	const Decoration::Ptr & findMatchingDecoration (CompWindow *w, bool sizeCheck);
        void clear ()
        {
	    mList.clear ();
        };

        DecorationList ();

	std::list <Decoration::Ptr> mList;
};

struct ScaledQuad {
    GLTexture::Matrix matrix;
    BoxRec            box;
    float             sx;
    float             sy;
};

class WindowDecoration {
    public:
	static WindowDecoration * create (const Decoration::Ptr &);
	static void destroy (WindowDecoration *);

    public:
	Decoration::Ptr decor;
	ScaledQuad *quad;
	int	   nQuad;
};

class DecorWindow;

class DecorScreen :
    public ScreenInterface,
    public CompositeScreenInterface,
    public PluginClassHandler<DecorScreen,CompScreen>,
    public DecorOptions
{
    public:
	DecorScreen (CompScreen *s);
	~DecorScreen ();

	bool setOption (const CompString &name, CompOption::Value &value);

	void handleEvent (XEvent *event);
	void matchPropertyChanged (CompWindow *);
	void addSupportedAtoms (std::vector<Atom>&);

	DecorTexture * getTexture (Pixmap);
	void releaseTexture (DecorTexture *);

	void checkForDm (bool);
	bool decoratorStartTimeout ();

	void updateDefaultShadowProperty ();

	bool registerPaintHandler (compiz::composite::PaintHandler *pHnd);
	void unregisterPaintHandler ();

    public:

	CompositeScreen *cScreen;

	std::list<DecorTexture *> textures;

	Atom supportingDmCheckAtom;
	Atom winDecorAtom;
	Atom decorAtom[DECOR_NUM];
	Atom inputFrameAtom;
	Atom outputFrameAtom;
	Atom decorTypeAtom;
	Atom decorTypePixmapAtom;
	Atom decorTypeWindowAtom;
	Atom requestFrameExtentsAtom;
	Atom shadowColorAtom;
	Atom shadowInfoAtom;
	Atom decorSwitchWindowAtom;

	Window dmWin;
	int    dmSupports;

	bool cmActive;

	DecorationList decor[DECOR_NUM];
	Decoration::Ptr     windowDefault;

	std::map<Window, DecorWindow *> frames;

	CompTimer decoratorStart;
};

class DecorWindow :
    public WindowInterface,
    public CompositeWindowInterface,
    public GLWindowInterface,
    public PluginClassHandler<DecorWindow,CompWindow>
{
    public:
	DecorWindow (CompWindow *w);
	~DecorWindow ();

	void getOutputExtents (CompWindowExtents&);
	void resizeNotify (int, int, int, int);
	void moveNotify (int, int, bool);
	void stateChangeNotify (unsigned int);
	void updateFrameRegion (CompRegion &region);

	bool damageRect (bool, const CompRect &);

	void computeShadowRegion ();

	bool glDraw (const GLMatrix &, GLFragment::Attrib &,
		     const CompRegion &, unsigned int);
	void glDecorate (const GLMatrix &, GLFragment::Attrib &,
		         const CompRegion &, unsigned int);

	void windowNotify (CompWindowNotify n);

	void updateDecoration ();

	void setDecorationMatrices ();

	void updateDecorationScale ();

	void updateFrame ();
	void updateInputFrame ();
	void updateOutputFrame ();
	void updateWindowRegions ();

	bool checkSize (const Decoration::Ptr &decor);

	int shiftX ();
	int shiftY ();

	bool update (bool);

	bool resizeTimeout ();

	void updateSwitcher ();
	void updateHandlers ();

	static bool matchType (CompWindow *w, unsigned int decorType);
	static bool matchState (CompWindow *w, unsigned int decorState);
	static bool matchActions (CompWindow *w, unsigned int decorActions);

    public:

	CompWindow      *window;
	GLWindow        *gWindow;
	CompositeWindow *cWindow;
	DecorScreen     *dScreen;

	WindowDecoration *wd;
	DecorationList	 decor;

	CompRegion frameRegion;
	CompRegion shadowRegion;
	CompRegion tmpRegion;

	Window inputFrame;
	Window outputFrame;
	Damage frameDamage;

	int    oldX;
	int    oldY;
	int    oldWidth;
	int    oldHeight;

	bool pixmapFailed;

	CompRegion::Vector regions;
	bool               updateReg;

	CompTimer resizeUpdate;
	CompTimer moveUpdate;

	bool	  unshading;
	bool	  shading;
	bool	  isSwitcher;

	bool      frameExtentsRequested;
};

class DecorPluginVTable :
    public CompPlugin::VTableForScreenAndWindow<DecorScreen, DecorWindow>
{
    public:

	bool init ();
};

