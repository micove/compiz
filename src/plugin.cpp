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

#include "core/plugin.h"
#include "privatescreen.h"

#include <boost/scoped_array.hpp>
#include <boost/foreach.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <set>

#define foreach BOOST_FOREACH


CompPlugin::Map pluginsMap;
CompPlugin::List plugins;

class CorePluginVTable : public CompPlugin::VTable
{
    public:

	bool init ();

	CompOption::Vector & getOptions ();

	bool setOption (const CompString  &name,
			CompOption::Value &value);
};

COMPIZ_PLUGIN_20090315 (core, CorePluginVTable)

CompPlugin::VTable * getCoreVTable ()
{
    if (!coreVTable)
    {
	return getCompPluginVTable20090315_core ();
    }

    return coreVTable;
}

bool
CorePluginVTable::init ()
{
    return true;
}

CompOption::Vector &
CorePluginVTable::getOptions ()
{
    return screen->getOptions ();
}

bool
CorePluginVTable::setOption (const CompString  &name,
			     CompOption::Value &value)
{
    return screen->setOption (name, value);
}

static bool
cloaderLoadPlugin (CompPlugin *p,
		   const char *path,
		   const char *name)
{
    if (path)
	return false;

    if (strcmp (name, getCoreVTable ()->name ().c_str ()))
	return false;

    p->vTable	      = getCoreVTable ();
    p->devPrivate.ptr = NULL;
    p->devType	      = "cloader";

    return true;
}

static void
cloaderUnloadPlugin (CompPlugin *p)
{
    delete p->vTable;
}

static CompStringList
cloaderListPlugins (const char *path)
{
    CompStringList rv;

    if (path)
	return CompStringList ();

    rv.push_back (CompString (getCoreVTable ()->name ()));

    return rv;
}

static bool
dlloaderLoadPlugin (CompPlugin *p,
		    const char *path,
		    const char *name)
{
    CompString  file;
    void        *dlhand;
    bool        loaded = false;
    struct stat fileInfo;

    if (cloaderLoadPlugin (p, path, name))
	return true;

    if (path)
    {
	file  = path;
	file += "/";
    }

    file += "lib";
    file += name;
    file += ".so";

    if (stat (file.c_str (), &fileInfo) != 0)
    {
	/* file likely not present */
	compLogMessage ("core", CompLogLevelDebug,
			"Could not stat() file %s : %s",
			file.c_str (), strerror (errno));
	return false;
    }

    int open_flags = RTLD_NOW;
#ifdef DEBUG
    // Do not unload the library during dlclose.
    open_flags |= RTLD_NODELETE;
    // Make the symbols available globally
    open_flags |= RTLD_GLOBAL;
#endif
    dlhand = dlopen (file.c_str (), open_flags);
    if (dlhand)
    {
	PluginGetInfoProc getInfo;
	char		  *error;
	char              sym[1024];

	dlerror ();

	snprintf (sym, 1024, "getCompPluginVTable20090315_%s", name);
	getInfo = (PluginGetInfoProc) dlsym (dlhand, sym);

	error = dlerror ();
	if (error)
	{
	    compLogMessage ("core", CompLogLevelError, "dlsym: %s", error);
	    getInfo = 0;
	}

	if (getInfo)
	{
	    p->vTable = (*getInfo) ();
	    if (!p->vTable)
	    {
		compLogMessage ("core", CompLogLevelError,
				"Couldn't get vtable from '%s' plugin",
				file.c_str ());
	    }
	    else
	    {
		p->devPrivate.ptr = dlhand;
		p->devType	  = "dlloader";
		loaded            = true;
	    }
	}
    }
    else
    {
	compLogMessage ("core", CompLogLevelError,
			"Couldn't load plugin '%s' : %s",
			file.c_str (), dlerror ());
    }

    if (!loaded && dlhand)
	dlclose (dlhand);

    return loaded;
}

static void
dlloaderUnloadPlugin (CompPlugin *p)
{
    if (p->devType == "dlloader")
    {
	delete p->vTable;
	dlclose (p->devPrivate.ptr);
    }
    else
	cloaderUnloadPlugin (p);
}

static int
dlloaderFilter (const struct dirent *name)
{
    int length = strlen (name->d_name);

    if (length < 7)
	return 0;

    if (strncmp (name->d_name, "lib", 3) ||
	strncmp (name->d_name + length - 3, ".so", 3))
	return 0;

    return 1;
}

static CompStringList
dlloaderListPlugins (const char *path)
{
    struct dirent **nameList;
    char	  name[1024];
    int		  length, nFile, i;

    CompStringList rv = cloaderListPlugins (path);

    if (!path)
	path = ".";

    nFile = scandir (path, &nameList, dlloaderFilter, alphasort);
    if (!nFile)
	return rv;

    for (i = 0; i < nFile; i++)
    {
	length = strlen (nameList[i]->d_name);

	strncpy (name, nameList[i]->d_name + 3, length - 6);
	name[length - 6] = '\0';

	rv.push_back (CompString (name));
    }

    return rv;
}

LoadPluginProc   loaderLoadPlugin   = dlloaderLoadPlugin;
UnloadPluginProc loaderUnloadPlugin = dlloaderUnloadPlugin;
ListPluginsProc  loaderListPlugins  = dlloaderListPlugins;


bool
CompManager::initPlugin (CompPlugin *p)
{

    if (!p->vTable->init ())
    {
	compLogMessage ("core", CompLogLevelError,
			"InitPlugin '%s' failed", p->vTable->name ().c_str ());
	return false;
    }

    if (screen && screen->displayInitialised())
    {
	if (!p->vTable->initScreen (screen))
	{
	    compLogMessage (p->vTable->name ().c_str (), CompLogLevelError,
                            "initScreen failed");
	    p->vTable->fini ();
	    return false;
	}
	if (!screen->initPluginForScreen (p))
	{
	    p->vTable->fini ();
	    return false;
	}
    }

    return true;
}

void
CompManager::finiPlugin (CompPlugin *p)
{

    if (screen)
    {
	screen->finiPluginForScreen (p);
	p->vTable->finiScreen (screen);
    }

    p->vTable->fini ();
}

bool
CompScreen::initPluginForScreen (CompPlugin *p)
{
    WRAPABLE_HND_FUNCTN_RETURN (bool, initPluginForScreen, p)
    return _initPluginForScreen (p);
}

bool
CompScreenImpl::_initPluginForScreen (CompPlugin *p)
{
    bool status               = true;
    CompWindowList::iterator it, fail;
    CompWindow               *w;

    it = fail = priv->windows.begin ();
    for (;it != priv->windows.end (); it++)
    {
	w = *it;
	if (!p->vTable->initWindow (w))
	{
	    compLogMessage (p->vTable->name ().c_str (), CompLogLevelError,
                            "initWindow failed");
            fail   = it;
            status = false;
	}
    }

    it = priv->windows.begin ();
    for (;it != fail; it++)
    {
	w = *it;
	p->vTable->finiWindow (w);
    }

    return status;
}

void
CompScreen::finiPluginForScreen (CompPlugin *p)
{
    WRAPABLE_HND_FUNCTN (finiPluginForScreen, p)
    _finiPluginForScreen (p);
}

void
CompScreenImpl::_finiPluginForScreen (CompPlugin *p)
{
    foreach (CompWindow *w, priv->windows)
	p->vTable->finiWindow (w);
}

bool
CompPlugin::screenInitPlugins (CompScreen *s)
{
    CompPlugin::List::reverse_iterator it = plugins.rbegin ();

    CompPlugin *p = NULL;

    /* Plugins is a btf list, so iterate it in reverse */
    while (it != plugins.rend ())
    {
	p = (*it);

	if (p->vTable->initScreen (s))
	    s->initPluginForScreen (p);

	it++;
    }

    return true;
}

void
CompPlugin::screenFiniPlugins (CompScreen *s)
{
    foreach (CompPlugin *p, plugins)
    {
	s->finiPluginForScreen (p);
	p->vTable->finiScreen (s);
    }

}

bool
CompPlugin::windowInitPlugins (CompWindow *w)
{
    bool status = true;

    for (List::reverse_iterator rit = plugins.rbegin ();
         rit != plugins.rend (); ++rit)
    {
	status &= (*rit)->vTable->initWindow (w);
    }

    return status;
}

void
CompPlugin::windowFiniPlugins (CompWindow *w)
{
    foreach (CompPlugin *p, plugins)
    {
	p->vTable->finiWindow (w);
    }
}


CompPlugin *
CompPlugin::find (const char *name)
{
    CompPlugin::Map::iterator it = pluginsMap.find (name);

    if (it != pluginsMap.end ())
        return it->second;

    return NULL;
}

void
CompPlugin::unload (CompPlugin *p)
{
    loaderUnloadPlugin (p);
    delete p;
}

CompPlugin *
CompPlugin::load (const char *name)
{
    std::auto_ptr<CompPlugin>p(new CompPlugin ());

    p->devPrivate.uval = 0;
    p->devType	       = "";
    p->vTable	       = 0;


    if (char* home = getenv ("HOME"))
    {
        boost::scoped_array<char> plugindir(new char [strlen (home) + strlen (HOME_PLUGINDIR) + 3]);
        sprintf (plugindir.get(), "%s/%s", home, HOME_PLUGINDIR);

        if (loaderLoadPlugin (p.get(), plugindir.get(), name))
            return p.release();
    }

    if (loaderLoadPlugin (p.get(), PLUGINDIR, name))
        return p.release();

    if (loaderLoadPlugin (p.get(), NULL, name))
        return p.release();

    compLogMessage ("core", CompLogLevelError,
		    "Couldn't load plugin '%s'", name);

    return 0;
}

bool
CompPlugin::push (CompPlugin *p)
{
    const char *name = p->vTable->name ().c_str ();

    std::pair<CompPlugin::Map::iterator, bool> insertRet =
        pluginsMap.insert (std::pair<const char *, CompPlugin *> (name, p));

    if (!insertRet.second)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Plugin '%s' already active",
			p->vTable->name ().c_str ());

	return false;
    }

    plugins.push_front (p);

    if (!CompManager::initPlugin (p))
    {
	compLogMessage ("core", CompLogLevelError,
			"Couldn't activate plugin '%s'", name);

        pluginsMap.erase (name);
	plugins.pop_front ();

	return false;
    }

    return true;
}

CompPlugin *
CompPlugin::pop (void)
{
    if (plugins.empty ())
	return NULL;

    CompPlugin *p = plugins.front ();

    if (!p)
	return 0;

    pluginsMap.erase (p->vTable->name ().c_str ());

    CompManager::finiPlugin (p);

    plugins.pop_front ();

    return p;
}

CompPlugin::List &
CompPlugin::getPlugins (void)
{
    return plugins;
}

CompStringList
CompPlugin::availablePlugins ()
{
    CompStringList homeList;

    if (char* home = getenv ("HOME"))
    {
        boost::scoped_array<char> plugindir(new char [strlen (home) + strlen (HOME_PLUGINDIR) + 3]);
        sprintf (plugindir.get(), "%s/%s", home, HOME_PLUGINDIR);

	homeList = loaderListPlugins (plugindir.get());
    }

    std::set<CompString> set;

    CompStringList pluginList  = loaderListPlugins (PLUGINDIR);
    CompStringList currentList = loaderListPlugins (0);

    std::copy(homeList.begin(), homeList.end(), std::inserter(set, set.end()));
    std::copy(pluginList.begin(), pluginList.end(), std::inserter(set, set.end()));
    std::copy(currentList.begin(), currentList.end(), std::inserter(set, set.end()));

    return CompStringList(set.begin(), set.end());
}

int
CompPlugin::getPluginABI (const char *name)
{
    CompPlugin *p = find (name);
    CompString s = name;

    if (!p)
	return 0;

    s += "_ABI";

    if (!screen->hasValue (s))
	return 0;

    return screen->getValue (s).uval;
}

bool
CompPlugin::checkPluginABI (const char *name,
			    int        abi)
{
    int pluginABI;

    pluginABI = getPluginABI (name);
    if (!pluginABI)
    {
	compLogMessage ("core", CompLogLevelError,
			"Plugin '%s' not loaded.\n", name);
	return false;
    }
    else if (pluginABI != abi)
    {
	compLogMessage ("core", CompLogLevelError,
			"Plugin '%s' has ABI version '%d', expected "
			"ABI version '%d'.\n",
			name, pluginABI, abi);
	return false;
    }

    return true;
}

CompPlugin::VTable::VTable () :
    mName (""),
    mSelf (NULL)
{
}

CompPlugin::VTable::~VTable ()
{
    if (mSelf)
	*mSelf = NULL;
}

void
CompPlugin::VTable::initVTable (CompString         name,
				CompPlugin::VTable **self)
{
    mName = name;
    if (self)
    {
	mSelf = self;
	*mSelf = this;
    }
}

const CompString
CompPlugin::VTable::name () const
{
    return mName;
}

void
CompPlugin::VTable::fini ()
{
}

bool
CompPlugin::VTable::initScreen (CompScreen *)
{
    return true;
}

void
CompPlugin::VTable::finiScreen (CompScreen *)
{
}

bool
CompPlugin::VTable::initWindow (CompWindow *)
{
    return true;
}

void
CompPlugin::VTable::finiWindow (CompWindow *)
{
}

CompOption::Vector &
CompPlugin::VTable::getOptions ()
{
    return noOptions ();
}

bool
CompPlugin::VTable::setOption (const CompString  &name,
			       CompOption::Value &value)
{
    return false;
}
