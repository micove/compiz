/*
 * Copyright © 2007 Novell, Inc.
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

#ifndef _COMPIZ_PLUGIN_H
#define _COMPIZ_PLUGIN_H

#include <core/string.h>
#include <core/action.h>
#include <core/option.h>
#include <core/privateunion.h>
#include <core/pluginclasshandler.h>

#include <string.h>

class CompScreen;
extern CompScreen *screen;

#include <map>

#define HOME_PLUGINDIR ".compiz-1/plugins"

#define COMPIZ_PLUGIN_20090315(name, classname)                           \
    CompPlugin::VTable * name##VTable = NULL;                             \
    extern "C" {                                                          \
        CompPlugin::VTable * getCompPluginVTable20090315_##name ()        \
	{                                                                 \
	    if (!name##VTable)                                            \
	    {                                                             \
	        name##VTable = new classname ();                          \
	        name##VTable->initVTable (TOSTRING (name), &name##VTable);\
                return name##VTable;                                      \
	    }                                                             \
            else                                                          \
                return name##VTable;                                      \
	}                                                                 \
    }

class CompPlugin;
class CompWindow;

typedef bool (*LoadPluginProc) (CompPlugin *p,
				const char *path,
				const char *name);

typedef void (*UnloadPluginProc) (CompPlugin *p);

typedef CompStringList (*ListPluginsProc) (const char *path);

extern LoadPluginProc   loaderLoadPlugin;
extern UnloadPluginProc loaderUnloadPlugin;
extern ListPluginsProc  loaderListPlugins;

namespace compiz
{
namespace plugin
{
namespace internal
{
class VTableBase;

/**
 * Provide a definition for PluginKey, with a protected
 * constructor which is a friend of VTableBase */
class PluginKey
{
    protected:

	PluginKey () {}

	friend class VTableBase;
};

class VTableBase :
    public PluginKey
{
    public:

	virtual ~VTableBase () {};
};
}
}
}

/**
 * Base plug-in interface for Compiz. All plugins must implement this
 * interface, which provides basics for loading, unloading, options,
 * and linking together the plugin and Screen(s).
 */
class CompPlugin {
    public:
	class VTable :
	    public compiz::plugin::internal::VTableBase
	{
	    public:

		VTable ();
		virtual ~VTable ();

		void initVTable (CompString         name,
				 CompPlugin::VTable **self = NULL);
		
		/**
		 * Gets the name of this compiz plugin
		 */
		const CompString &name () const;

		virtual bool init () = 0;
		virtual void fini ();

		/* Mark the plugin as ready to be instantiated */
		virtual void markReadyToInstantiate () = 0;

		/* Mark the plugin as disallowing further instantiation */
		virtual void markNoFurtherInstantiation () = 0;

		virtual bool initScreen (CompScreen *s);

		virtual void finiScreen (CompScreen *s);

		virtual bool initWindow (CompWindow *w);

		virtual void finiWindow (CompWindow *w);

		virtual CompOption::Vector & getOptions ();

		virtual bool setOption (const CompString  &name,
					CompOption::Value &value);

		virtual CompAction::Vector & getActions ();
	    private:
		CompString   mName;
		VTable       **mSelf;
        };

	/**
	 * TODO (or not?)
	 */
	template <typename T, typename T2, int ABI = 0>
	class VTableForScreenAndWindow :
	    public VTable
	{
	    public:

		bool initScreen (CompScreen *s);
		void finiScreen (CompScreen *s);
		bool initWindow (CompWindow *w);
		void finiWindow (CompWindow *w);
		CompOption::Vector & getOptions ();
		bool setOption (const CompString &name,
				CompOption::Value &value);
		CompAction::Vector & getActions ();

	    private:

		/* Mark the plugin as ready to be instantiated */
		void markReadyToInstantiate ();

		/* Mark the plugin as disallowing further instantiation */
		void markNoFurtherInstantiation ();
	};

	/**
	 * TODO (or not?)
	 */
	template <typename T, int ABI = 0>
	class VTableForScreen :
	    public VTable
	{
	    public:

		bool initScreen (CompScreen *s);
		void finiScreen (CompScreen *s);
		CompOption::Vector & getOptions ();
		bool setOption (const CompString &name, CompOption::Value &value);
		CompAction::Vector & getActions ();

	    private:

		/* Mark the plugin as ready to be instantiated */
		void markReadyToInstantiate ();

		/* Mark the plugin as disallowing further instantiation */
		void markNoFurtherInstantiation ();
	};
	
	typedef std::map<CompString, CompPlugin *> Map;
	typedef std::list<CompPlugin *> List;

    public:
        CompPrivate devPrivate;
        CompString  devType;
        VTable      *vTable;

    public:

	static bool screenInitPlugins (CompScreen *s);

	static void screenFiniPlugins (CompScreen *s);

	static bool windowInitPlugins (CompWindow *w);

	static void windowFiniPlugins (CompWindow *w);
	
	/**
	 * Finds a plugin by name. (TODO Does it have to be loaded?)
	 */
	static CompPlugin *find (const char *name);
	
	/**
	 * Load a compiz plugin. Loading a plugin that has
	 * already been loaded will return the existing instance.
	 */
	static CompPlugin *load (const char *plugin);
	
	/**
	 * Unload a compiz plugin. Unloading a plugin multiple times has no
	 * effect, and you can't unload a plugin that hasn't been loaded already
	 * with CompPlugin::load()
	 */
	static void unload (CompPlugin *p);
	
	/**
	 * Adds a plugin onto the working set of active plugins. If the plugin fails to initPlugin
	 * this will return false and the plugin will not be added to the working set.
	 */
	static bool push (CompPlugin *p);
	
	/**
	 * Removes the last activated plugin with CompPlugin::push() and returns
	 * the removed plugin, or NULL if there was none.
	 */
	static CompPlugin *pop (void);
	
	/**
	 * Gets a list of the working set of plugins that have been CompPlugin::push() but not
	 * CompPlugin::pop()'d.
	 */
	static List & getPlugins ();

	/**
	 * Gets the Application Binary Interface (ABI) version of a (un)loaded plugin.
	 */
	static int getPluginABI (const char *name);
	
	/**
	 * Verifies a signature to ensure that the plugin conforms to the Application Binary
	 * Interface (ABI) 
	 */
	static bool checkPluginABI (const char *name,
				    int	        abi);

};

/**
 * Mark the plugin class handlers as ready to be initialized
 */
template <typename T, typename T2, int ABI>
void
CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::markReadyToInstantiate ()
{
    namespace cpi = compiz::plugin::internal;

    typedef typename T::ClassPluginBaseType ScreenBase;
    typedef typename T2::ClassPluginBaseType WindowBase;

    typedef cpi::LoadedPluginClassBridge <T, ScreenBase, ABI> ScreenBridge;
    typedef cpi::LoadedPluginClassBridge <T2, WindowBase, ABI> WindowBridge;

    ScreenBridge::allowInstantiations (*this);
    WindowBridge::allowInstantiations (*this);
}

/**
 * Allow no further class handler initialization
 */
template <typename T, typename T2, int ABI>
void
CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::markNoFurtherInstantiation ()
{
    namespace cpi = compiz::plugin::internal;

    typedef typename T::ClassPluginBaseType ScreenBase;
    typedef typename T2::ClassPluginBaseType WindowBase;

    typedef cpi::LoadedPluginClassBridge <T, ScreenBase, ABI> ScreenBridge;
    typedef cpi::LoadedPluginClassBridge <T2, WindowBase, ABI> WindowBridge;

    ScreenBridge::disallowInstantiations (*this);
    WindowBridge::disallowInstantiations (*this);
}

template <typename T, typename T2, int ABI>
bool
CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::initScreen (CompScreen *s)
{
    T * ps = T::get (s);
    if (!ps)
	return false;

    return true;
}

template <typename T, typename T2, int ABI>
void
CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::finiScreen (CompScreen *s)
{
    T * ps = T::get (s);
    delete ps;
}

template <typename T, typename T2, int ABI>
bool
CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::initWindow (CompWindow *w)
{
    T2 * pw = T2::get (w);
    if (!pw)
	return false;

    return true;
}

template <typename T, typename T2, int ABI>
void
CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::finiWindow (CompWindow *w)
{
    T2 * pw = T2::get (w);
    delete pw;
}

template <typename T, typename T2, int ABI>
CompOption::Vector & CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::getOptions ()
{
    CompOption::Class *oc = dynamic_cast<CompOption::Class *> (T::get (screen));
    if (!oc)
	return noOptions ();
    return oc->getOptions ();
}

template <typename T, typename T2, int ABI>
bool
CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::setOption (const CompString  &name,
							     CompOption::Value &value)
{
    CompOption::Class *oc = dynamic_cast<CompOption::Class *> (T::get (screen));
    if (!oc)
	return false;
    return oc->setOption (name, value);
}

template <typename T, typename T2, int ABI>
CompAction::Vector & CompPlugin::VTableForScreenAndWindow<T, T2, ABI>::getActions ()
{
    CompAction::Container *ac = dynamic_cast<CompAction::Container *> (T::get (screen));
    if (!ac)
	return noActions ();
    return ac->getActions ();
}

/**
 * Mark the plugin class handlers as ready to be initialized
 */
template <typename T, int ABI>
void
CompPlugin::VTableForScreen<T, ABI>::markReadyToInstantiate ()
{
    namespace cpi = compiz::plugin::internal;

    typedef typename T::ClassPluginBaseType ScreenBase;
    typedef cpi::LoadedPluginClassBridge <T, ScreenBase, ABI> ScreenBridge;

    ScreenBridge::allowInstantiations (*this);
}

/**
 * Allow no further class handler initialization
 */
template <typename T, int ABI>
void
CompPlugin::VTableForScreen<T, ABI>::markNoFurtherInstantiation ()
{
    namespace cpi = compiz::plugin::internal;

    typedef typename T::ClassPluginBaseType ScreenBase;
    typedef cpi::LoadedPluginClassBridge <T, ScreenBase, ABI> ScreenBridge;

    ScreenBridge::disallowInstantiations (*this);
}

template <typename T, int ABI>
bool
CompPlugin::VTableForScreen<T, ABI>::initScreen (CompScreen *s)
{
    T * ps = new T (s);
    if (ps->loadFailed ())
    {
	delete ps;
	return false;
    }
    return true;
}

template <typename T, int ABI>
void CompPlugin::VTableForScreen<T, ABI>::finiScreen (CompScreen *s)
{
    T * ps = T::get (s);
    delete ps;
}

template <typename T, int ABI>
CompOption::Vector &
CompPlugin::VTableForScreen<T, ABI>::getOptions ()
{
    CompOption::Class *oc = dynamic_cast<CompOption::Class *> (T::get (screen));
    if (!oc)
	return noOptions ();
    return oc->getOptions ();
}

template <typename T, int ABI>
bool
CompPlugin::VTableForScreen<T, ABI>::setOption (const CompString  &name,
						CompOption::Value &value)
{
    CompOption::Class *oc = dynamic_cast<CompOption::Class *> (T::get (screen));
    if (!oc)
	return false;
    return oc->setOption (name, value);
}

template <typename T, int ABI>
CompAction::Vector &
CompPlugin::VTableForScreen<T, ABI>::getActions ()
{
    CompAction::Container *ac = dynamic_cast<CompAction::Container *> (T::get (screen));
    if (!ac)
	return noActions ();
    return ac->getActions ();
}

typedef CompPlugin::VTable *(*PluginGetInfoProc) (void);

#endif
