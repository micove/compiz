/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2005 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <core/core.h>
#include <core/atoms.h>
#include <core/timer.h>
#include <core/pluginclasshandler.h>

#include "place_options.h"

class PlaceScreen :
    public PluginClassHandler<PlaceScreen, CompScreen>,
    public ScreenInterface,
    public PlaceOptions
{
    public:
	PlaceScreen (CompScreen *);
	~PlaceScreen ();

	void handleEvent (XEvent *event);
	void doHandleScreenSizeChange (bool, int, int);
	bool handleScreenSizeChangeFallback (int width, int height);
	void handleScreenSizeChange (int width, int height);
	bool getPointerPosition (CompPoint &p);
	void addSupportedAtoms (std::vector<Atom>&);
	
	CompSize mPrevSize;
	int	 mStrutWindowCount;
	CompTimer mResChangeFallbackHandle;
	
	Atom fullPlacementAtom;
};

#define PLACE_SCREEN(s)						       \
    PlaceScreen *ps = PlaceScreen::get (s)

class PlaceWindow :
    public PluginClassHandler<PlaceWindow, CompWindow>,
    public WindowInterface
{
    public:
	PlaceWindow (CompWindow *w);
	~PlaceWindow ();

	bool place (CompPoint &pos);
	
	CompRect
	doValidateResizeRequest (unsigned int &,
				      XWindowChanges *,
				      unsigned int,
				      bool);
	void validateResizeRequest (unsigned int   &mask,
				    XWindowChanges *xwc,
				    unsigned int   source);
	void grabNotify (int, int, unsigned int, unsigned int);    
	bool mSavedOriginal;
	CompRect mOrigVpRelRect;
	CompPoint mPrevServer;
	

    private:
	typedef enum {
	    NoPlacement = 0,
	    PlaceOnly,
	    ConstrainOnly,
	    PlaceAndConstrain,
	    PlaceOverParent,
	    PlaceCenteredOnScreen
	} PlacementStrategy;

	void doPlacement (CompPoint &pos);
	bool windowIsPlaceRelevant (CompWindow *w);
	bool hasUserDefinedPosition (bool);
	PlacementStrategy getStrategy ();
	const CompOutput & getPlacementOutput (int		 mode,
					       PlacementStrategy strategy,
					       CompPoint pos);
	int getPlacementMode ();

	void sendMaximizationRequest ();
	void constrainToWorkarea (const CompRect& workArea, CompPoint& pos);

	void placeCascade (const CompRect& workArea, CompPoint& pos);
	void placeCentered (const CompRect& workArea, CompPoint& pos);
	void placeRandom (const CompRect& workArea, CompPoint& pos);
	void placePointer (const CompRect& workArea, CompPoint& pos);
	void placeSmart (const CompRect& workArea, CompPoint& pos);

	bool cascadeFindFirstFit (const CompWindowList& windows,
				  const CompRect& workArea,
				  CompPoint &pos);
	void cascadeFindNext (const CompWindowList& windows,
			      const CompRect& workArea,
			      CompPoint &pos);

	bool matchPosition (CompPoint &pos, bool& keepInWorkarea);
	bool matchViewport (CompPoint &pos);

	bool matchXYValue (CompOption::Value::Vector &matches,
			   CompOption::Value::Vector &xValues,
			   CompOption::Value::Vector &yValues,
			   CompPoint &pos,
			   CompOption::Value::Vector *constrainValues = NULL,
			   bool *keepInWorkarea = NULL);

	CompWindow  *window;
	PlaceScreen *ps;
};

#define PLACE_WINDOW(w)							       \
    PlaceWindow *pw = PlaceWindow::get (w)

class PlacePluginVTable :
    public CompPlugin::VTableForScreenAndWindow<PlaceScreen, PlaceWindow>
{
    public:
	bool init ();
};
