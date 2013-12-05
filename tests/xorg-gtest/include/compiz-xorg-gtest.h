/*
 * Compiz XOrg GTest
 *
 * Copyright (C) 2012 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authored By:
 * Sam Spilsbury <sam.spilsbury@canonical.com>
 */
#ifndef _COMPIZ_XORG_GTEST_H
#define _COMPIZ_XORG_GTEST_H
#include <memory>
#include <list>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <xorg/gtest/xorg-gtest.h>

using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Matcher;

namespace compiz
{
    namespace testing
    {
	typedef  ::testing::MatcherInterface <const XEvent &> XEventMatcher;

	class PrivateClientMessageXEventMatcher;
	class ClientMessageXEventMatcher :
	    public compiz::testing::XEventMatcher
	{
	    public:

		ClientMessageXEventMatcher (Display *display,
					    Atom    message,
					    Window  target);

		virtual bool MatchAndExplain (const XEvent &event, MatchResultListener *listener) const;
		virtual void DescribeTo (std::ostream *os) const;

	    private:

		std::auto_ptr <PrivateClientMessageXEventMatcher> priv;
	};

	class PrivatePropertyNotifyXEventMatcher;
	class PropertyNotifyXEventMatcher :
	    public compiz::testing::XEventMatcher
	{
	    public:

		PropertyNotifyXEventMatcher (Display *dpy,
					     const std::string &propertyName);

		virtual bool MatchAndExplain (const XEvent &event, MatchResultListener *listener) const;
		virtual void DescribeTo (std::ostream *os) const;

	    private:

		std::auto_ptr <PrivatePropertyNotifyXEventMatcher> priv;
	};

	class PrivateConfigureNotifyXEventMatcher;
	class ConfigureNotifyXEventMatcher :
	    public compiz::testing::XEventMatcher
	{
	    public:

		ConfigureNotifyXEventMatcher (Window       above,
					      unsigned int border,
					      int          x,
					      int          y,
					      unsigned int width,
					      unsigned int height,
					      unsigned int mask);

		virtual bool MatchAndExplain (const XEvent &event, MatchResultListener *listener) const;
		virtual void DescribeTo (std::ostream *os) const;

	    private:

		std::auto_ptr <PrivateConfigureNotifyXEventMatcher> priv;
	};

	class PrivateShapeNotifyXEventMatcher;
	class ShapeNotifyXEventMatcher :
	    public compiz::testing::XEventMatcher
	{
	    public:

		ShapeNotifyXEventMatcher (int          kind,
					  int          x,
					  int          y,
					  unsigned int width,
					  unsigned int height,
					  Bool         shaped);

		virtual bool MatchAndExplain (const XEvent &event,
					      MatchResultListener *listener) const;
		virtual void DescribeTo (std::ostream *os) const;

	    private:

		std::auto_ptr <PrivateShapeNotifyXEventMatcher> priv;
	};

	const int          WINDOW_X = 0;
	const int          WINDOW_Y = 0;
	const unsigned int WINDOW_WIDTH = 640;
	const unsigned int WINDOW_HEIGHT = 480;

	Window CreateNormalWindow (Display *dpy);
	Window GetImmediateParent (Display *display,
				   Window  w,
				   Window  &rootReturn);
	Window GetTopmostNonRootParent (Display *display,
					Window  w);

	std::list <Window> NET_CLIENT_LIST_STACKING (Display *);
	bool AdvanceToNextEventOnSuccess (Display *dpy,
					  bool waitResult);
	bool WaitForEventOfTypeOnWindow (Display *dpy,
					 Window  w,
					 int     type,
					 int     ext,
					 int     extType,
					 int     timeout = 0);
	bool WaitForEventOfTypeOnWindowMatching (Display             *dpy,
						 Window              w,
						 int                 type,
						 int                 ext,
						 int                 extType,
						 const XEventMatcher &matcher,
						 int                 timeout = 0);

	void RelativeWindowGeometry (Display      *dpy,
				     Window       w,
				     int          &x,
				     int          &y,
				     unsigned int &width,
				     unsigned int &height,
				     unsigned int &border);

	void AbsoluteWindowGeometry (::Display    *display,
				     Window       window,
				     int          &x,
				     int          &y,
				     unsigned int &width,
				     unsigned int &height,
				     unsigned int &border);

	typedef void (*RetrievalFunc) (Display      *dpy,
				       Window       window,
				       int          &x,
				       int          &y,
				       unsigned int &width,
				       unsigned int &height,
				       unsigned int &border);

	class WindowGeometryMatcher :
	    public MatcherInterface <Window>
	{
	    public:

		WindowGeometryMatcher (Display             *dpy,
				       RetrievalFunc       func,
				       const Matcher <int> &x,
				       const Matcher <int> &y,
				       const Matcher <unsigned int> &width,
				       const Matcher <unsigned int> &height,
				       const Matcher <unsigned int> &border);

		bool MatchAndExplain (Window x, MatchResultListener *listener) const;
		void DescribeTo (std::ostream *os) const;

	    private:

		class Private;

		std::auto_ptr <Private> priv;
	};

	Matcher <Window>
	HasGeometry (Display             *dpy,
		     RetrievalFunc       func,
		     const Matcher <int> &x,
		     const Matcher <int> &y,
		     const Matcher <unsigned int> &width,
		     const Matcher <unsigned int> &height,
		     const Matcher <unsigned int> &border);

	class PrivateCompizProcess;
	class CompizProcess
	{
	    public:
		typedef enum _StartupFlags
		{
		    ReplaceCurrentWM = (1 << 0),
		    WaitForStartupMessage = (1 << 1),
		    ExpectStartupFailure = (1 << 2)
		} StartupFlags;

		typedef enum _PluginType
		{
		    Real = 0,
		    TestOnly = 1
		} PluginType;

		struct Plugin
		{
		    Plugin (const char *name,
			    PluginType type) :
			name (name),
			type (type)
		    {
		    }

		    std::string name;
		    PluginType  type;
		};

		typedef std::vector <Plugin> PluginList;

		CompizProcess (Display *dpy,
			       StartupFlags,
			       const PluginList &plugins,
			       int timeout = 0);
		~CompizProcess ();
		xorg::testing::Process::State State ();
		pid_t Pid ();

	    private:
		std::auto_ptr <PrivateCompizProcess> priv;
	};

	class PrivateCompizXorgSystemTest;
	class CompizXorgSystemTest :
	    public xorg::testing::Test
	{
	    public:

		CompizXorgSystemTest ();

		virtual void SetUp ();
		virtual void TearDown ();

		xorg::testing::Process::State CompizProcessState ();
		void StartCompiz (CompizProcess::StartupFlags     flags,
				  const CompizProcess::PluginList &plugins);

	    private:
		std::auto_ptr <PrivateCompizXorgSystemTest> priv;
	};

	class PrivateAutostartCompizXorgSystemTest;
	class AutostartCompizXorgSystemTest :
	    public CompizXorgSystemTest
	{
	    public:

		AutostartCompizXorgSystemTest ();

		virtual CompizProcess::StartupFlags GetStartupFlags ();
		virtual int GetEventMask () const;
		virtual CompizProcess::PluginList GetPluginList ();
		virtual void SetUp ();

	    private:
		std::auto_ptr <PrivateAutostartCompizXorgSystemTest> priv;
	};

	class PrivateAutostartCompizXorgSystemTestWithTestHelper;
	class AutostartCompizXorgSystemTestWithTestHelper :
	    public AutostartCompizXorgSystemTest
	{
	    public:

		AutostartCompizXorgSystemTestWithTestHelper ();

		virtual CompizProcess::PluginList GetPluginList ();

	    protected:

		virtual void SetUp ();

		Atom FetchAtom (const char *);
		std::vector <long> WaitForWindowCreation (Window w);
		bool IsOverrideRedirect (std::vector <long> &data);

		virtual int  GetEventMask () const;

	    private:

		std::auto_ptr <PrivateAutostartCompizXorgSystemTestWithTestHelper> priv;
	};
    }
}

#endif
