/*
 * Copyright © 2008 Danny Baumann
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Danny Baumann not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Danny Baumann makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DANNY BAUMANN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DANNY BAUMANN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Danny Baumann <dannybaumann@web.de>
 *
 * based on ini.c by :
 *                       Mike Dransfield <mike@blueroot.co.uk>
 */

#include "ini.h"
#include <errno.h>
#include <boost/lexical_cast.hpp>

COMPIZ_PLUGIN_20090315 (ini, IniPluginVTable)

IniFile::IniFile (CompPlugin *p) :
    plugin (p)
{
}

IniFile::~IniFile ()
{
    if (optionFile.is_open ())
	optionFile.close ();
}

bool
IniFile::open (bool write)
{
    CompString              homeDir;
    std::ios_base::openmode mode;

    if (optionFile.is_open ())
	optionFile.close ();

    homeDir = IniScreen::getHomeDir ();
    if (homeDir.empty ())
	return false;

    filePath = homeDir;
    if (plugin->vTable->name () == "core")
	filePath += "general";
    else
	filePath += plugin->vTable->name ();
    filePath += FILE_SUFFIX;

    mode = write ? std::ios::out : std::ios::in;
    optionFile.open (filePath.c_str (), mode);

    return !optionFile.fail ();
}

void
IniFile::load ()
{
    bool resave = false;

    if (!plugin)
	return;

    CompOption::Vector& options = plugin->vTable->getOptions ();
    if (options.empty ())
	return;

    if (!open (false))
    {
	compLogMessage ("ini", CompLogLevelWarn,
			"Could not open config for plugin %s - using defaults.",
			plugin->vTable->name ().c_str ());

	save ();

	if (!open (false))
	    return;
    }
    else
    {
	CompString   line, optionValue;
	CompOption   *option;
	size_t       pos;

	while (std::getline (optionFile, line))
	{
	    pos = line.find_first_of ('=');
	    if (pos == CompString::npos)
		continue;

	    option = CompOption::findOption (options, line.substr (0, pos));
	    if (!option)
		continue;

	    optionValue = line.substr (pos + 1);
	    if (!stringToOption (option, optionValue))
		resave = true;
	}
    }

    /* re-save whole file if we encountered invalid lines */
    if (resave)
	save ();
}

void
IniFile::save ()
{
    if (!plugin)
	return;

    CompOption::Vector& options = plugin->vTable->getOptions ();
    if (options.empty ())
	return;

    if (!open (true))
    {
	IniScreen  *is = IniScreen::get (screen);
	CompString homeDir;

	homeDir = is->getHomeDir ();
	is->createDir (homeDir);
	is->updateDirectoryWatch (homeDir);
    }

    if (!open (true))
    {
	compLogMessage ("ini", CompLogLevelError,
			"Failed to write to config file %s, please "
			"check if you have sufficient permissions.",
			filePath.c_str ());
	return;
    }

    foreach (CompOption& option, options)
    {
	CompString optionValue;
	bool       valid;

	optionValue = optionToString (option, valid);
	if (valid)
	    optionFile << option.name () << "=" << optionValue << std::endl;
    }
}

CompString
IniFile::optionValueToString (CompOption::Value &value,
			      CompOption::Type  type)
{
    CompString retval;

    switch (type) {
    case CompOption::TypeBool:
	retval = value.b () ? "true" : "false";
	break;
    case CompOption::TypeInt:
	retval = boost::lexical_cast<CompString> (value.i ());
	break;
    case CompOption::TypeFloat:
	retval = boost::lexical_cast<CompString> (value.f ());
	break;
    case CompOption::TypeString:
	retval = value.s ();
	break;
    case CompOption::TypeColor:
	retval = CompOption::colorToString (value.c ());
	break;
    case CompOption::TypeKey:
	retval = value.action ().keyToString ();
	break;
    case CompOption::TypeButton:
	retval = value.action ().buttonToString ();
	break;
    case CompOption::TypeEdge:
	retval = value.action ().edgeMaskToString ();
	break;
    case CompOption::TypeBell:
	retval = value.action ().bell () ? "true" : "false";
	break;
    case CompOption::TypeMatch:
	retval = value.match ().toString ();
	break;
    default:
    	break;
    }

    return retval;
}

bool
IniFile::validItemType (CompOption::Type type)
{
    switch (type) {
    case CompOption::TypeBool:
    case CompOption::TypeInt:
    case CompOption::TypeFloat:
    case CompOption::TypeString:
    case CompOption::TypeColor:
    case CompOption::TypeKey:
    case CompOption::TypeButton:
    case CompOption::TypeEdge:
    case CompOption::TypeBell:
    case CompOption::TypeMatch:
	return true;
    default:
	break;
    }

    return false;
}

bool
IniFile::validListItemType (CompOption::Type type)
{
    switch (type) {
    case CompOption::TypeBool:
    case CompOption::TypeInt:
    case CompOption::TypeFloat:
    case CompOption::TypeString:
    case CompOption::TypeColor:
    case CompOption::TypeMatch:
	return true;
    default:
	break;
    }

    return false;
}

CompString
IniFile::optionToString (CompOption &option,
			 bool       &valid)
{
    CompString       retval;
    CompOption::Type type;

    valid = true;
    type  = option.type ();

    if (validItemType (type))
    {
	retval = optionValueToString (option.value (), option.type ());
    }
    else if (type == CompOption::TypeList)
    {
	type = option.value ().listType ();
	if (validListItemType (type))
	{
	    CompOption::Value::Vector& list = option.value ().list ();

	    foreach (CompOption::Value& listOption, list)
	    {
		retval += optionValueToString (listOption, type);
		retval += ",";
	    }

	    /* strip off dangling comma at the end */
	    if (!retval.empty ())
		retval.erase (retval.length () - 1);
	}
	else
	{
	    compLogMessage ("ini", CompLogLevelWarn,
			    "Unknown list option type %d on option %s.",
			    type, option.name ().c_str ());
	    valid = false;
	}
    }
    else
    {
	compLogMessage ("ini", CompLogLevelWarn,
			"Unknown option type %d found on option %s.",
			type, option.name ().c_str ());
	valid = false;
    }

    return retval;
}

bool
IniFile::stringToOptionValue (CompString        &string,
			      CompOption::Type  type,
			      CompOption::Value &value)
{
    bool retval = true;

    switch (type) {
    case CompOption::TypeBool:
	if (string == "true")
	    value.set (true);
	else if (string == "false")
	    value.set (false);
	else
	    retval = false;
	break;
    case CompOption::TypeInt:
	try
	{
	    int intVal;
	    intVal = boost::lexical_cast<int> (string);
	    value.set (intVal);
	}
	catch (boost::bad_lexical_cast)
	{
	    retval = false;
	};
	break;
    case CompOption::TypeFloat:
	try
	{
	    float floatVal;
	    floatVal = boost::lexical_cast<float> (string);
	    value.set (floatVal);
	}
	catch (boost::bad_lexical_cast)
	{
	    retval = false;
	};
	break;
    case CompOption::TypeString:
	value.set (string);
	break;
    case CompOption::TypeColor:
	{
	    unsigned short c[4];
	    retval = CompOption::stringToColor (string, c);
	    if (retval)
		value.set (c);
	}
	break;
    case CompOption::TypeKey:
    case CompOption::TypeButton:
    case CompOption::TypeEdge:
    case CompOption::TypeBell:
	{
	    CompAction action;

	    switch (type) {
	    case CompOption::TypeKey:
		retval = action.keyFromString (string);
		break;
	    case CompOption::TypeButton:
		retval = action.buttonFromString (string);
		break;
	    case CompOption::TypeEdge:
		retval = action.edgeMaskFromString (string);
		break;
	    case CompOption::TypeBell:
		if (string == "true")
		    action.setBell (true);
		else if (string == "false")
		    action.setBell (false);
		else
		    retval = false;
	    	break;
	    default:
	    	break;
	    }

	    if (retval)
		value.set (action);
	}
	break;
    case CompOption::TypeMatch:
	{
	    CompMatch match (string);
	    value.set (match);
	}
	break;
    default:
    	break;
    }

    return retval;
}

bool
IniFile::stringToOption (CompOption *option,
			 CompString &valueString)
{
    CompOption::Value value;
    bool              valid = false;
    CompOption::Type  type = option->type ();

    if (validItemType (type))
    {
	valid = stringToOptionValue (valueString, option->type (), value);
    }
    else if (type == CompOption::TypeList)
    {
	type = option->value ().listType ();
	if (validListItemType (type))
	{
	    CompString                listItem;
	    size_t                    delim, pos = 0;
	    CompOption::Value         item;
	    CompOption::Value::Vector list;

	    do
	    {
		delim = valueString.find_first_of (',', pos);

		if (delim != CompString::npos)
		    listItem = valueString.substr (pos, delim - pos);
		else
		    listItem = valueString.substr (pos);

		valid = stringToOptionValue (listItem, type, item);
		if (valid)
		    list.push_back (item);

		pos = delim + 1;
	    }
	    while (delim != CompString::npos);

	    value.set (type, list);
	    valid = true;
	}
    }

    if (valid)
	screen->setOptionForPlugin (plugin->vTable->name ().c_str (),
				    option->name ().c_str (), value);

    return valid;
}

void
IniScreen::fileChanged (const char *name)
{
    CompString   fileName, plugin;
    unsigned int length;
    CompPlugin   *p;

    if (!name || strlen (name) <= strlen (FILE_SUFFIX))
	return;

    fileName = name;

    length = fileName.length () - strlen (FILE_SUFFIX);
    if (strcmp (fileName.c_str () + length, FILE_SUFFIX) != 0)
	return;

    plugin = fileName.substr (0, length);
    p = CompPlugin::find (plugin == "general" ? "core" : plugin.c_str ());
    if (p)
    {
	IniFile ini (p);

	blockWrites = true;
	ini.load ();
	blockWrites = false;
    }
}

CompString
IniScreen::getHomeDir ()
{
    char       *home = getenv ("HOME");
    CompString retval;

    if (home)
    {
	retval += home;
	retval += "/";
	retval += HOME_OPTIONDIR;
	retval += "/";
    }

    return retval;
}

bool
IniScreen::createDir (const CompString& path)
{
    size_t pos;

    if (mkdir (path.c_str (), 0700) == 0)
	return true;

    /* did it already exist? */
    if (errno == EEXIST)
	return true;

    /* was parent present? if yes, fail */
    if (errno != ENOENT)
	return false;

    /* skip last character which may be a '/' */
    pos = path.rfind ('/', path.size () - 2);
    if (pos == CompString::npos)
	return false;

    if (!createDir (path.substr (0, pos)))
	return false;

    return (mkdir (path.c_str (), 0700) == 0);
}

void
IniScreen::updateDirectoryWatch (const CompString& path)
{
    int mask = NOTIFY_CREATE_MASK | NOTIFY_DELETE_MASK | NOTIFY_MODIFY_MASK;

    if (directoryWatchHandle)
	screen->removeFileWatch (directoryWatchHandle);

    directoryWatchHandle =
	screen->addFileWatch (path.c_str (), mask,
			      boost::bind (&IniScreen::fileChanged, this, _1));
}

bool
IniScreen::setOptionForPlugin (const char        *plugin,
			       const char        *name,
			       CompOption::Value &v)
{
    bool status = screen->setOptionForPlugin (plugin, name, v);

    if (status && !blockWrites)
    {
	CompPlugin *p;

	p = CompPlugin::find (plugin);
	if (p)
	{
	    CompOption *o;

	    o = CompOption::findOption (p->vTable->getOptions (), name);
	    if (o && (o->value () != v))
	    {
		IniFile ini (p);
		ini.save ();
	    }
	}
    }

    return status;
}

bool
IniScreen::initPluginForScreen (CompPlugin *p)
{
    bool status = screen->initPluginForScreen (p);

    if (status)
    {
	IniFile ini (p);

	blockWrites = true;
	ini.load ();
	blockWrites = false;
    }

    return status;
}

IniScreen::IniScreen (CompScreen *screen) :
    PluginClassHandler<IniScreen, CompScreen> (screen),
    directoryWatchHandle (0),
    blockWrites (false)
{
    CompString homeDir;

    homeDir = getHomeDir ();
    if (homeDir.empty () || !createDir (homeDir))
    {
	setFailed ();
	return;
    }

    updateDirectoryWatch (homeDir);

    IniFile ini (CompPlugin::find ("core"));
    ini.load ();

    ScreenInterface::setHandler (screen, true);
}

IniScreen::~IniScreen ()
{
    if (directoryWatchHandle)
	screen->removeFileWatch (directoryWatchHandle);
}

bool
IniPluginVTable::init ()
{
    if (!CompPlugin::checkPluginABI ("core", CORE_ABIVERSION))
	return false;

    return true;
}


