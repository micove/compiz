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
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <iostream>
#include <fstream>

#include <core/core.h>
#include <core/pluginclasshandler.h>

#define HOME_OPTIONDIR     ".compiz/options"
#define CORE_NAME           "general"
#define FILE_SUFFIX         ".conf"

class IniScreen :
    public ScreenInterface,
    public PluginClassHandler<IniScreen, CompScreen>
{
    public:
	IniScreen (CompScreen *screen);
	~IniScreen ();

	bool initPluginForScreen (CompPlugin *p);
	bool setOptionForPlugin (const char        *plugin,
				 const char        *name,
				 CompOption::Value &v);

	void updateDirectoryWatch (const CompString&);

    private:
	CompFileWatchHandle directoryWatchHandle;

	void fileChanged (const char *name);

	bool blockWrites;

    public:
	static CompString getHomeDir ();
	static bool createDir (const CompString& path);
};

class IniPluginVTable :
    public CompPlugin::VTableForScreen<IniScreen>
{
    public:

	bool init ();
};

class IniFile
{
    public:
	IniFile (CompPlugin *p);
	~IniFile ();

	void load ();
	void save ();

    private:
	CompPlugin *plugin;
	CompString filePath;

	std::fstream optionFile;

	bool open (bool write);

	CompString optionValueToString (CompOption::Value &value,
					CompOption::Type  type);
	CompString optionToString (CompOption &option,
				   bool       &valid);
	bool stringToOption (CompOption *option,
			     CompString &valueString);
	bool stringToOptionValue (CompString        &string,
				  CompOption::Type  type,
				  CompOption::Value &value);

	bool validItemType (CompOption::Type type);
	bool validListItemType (CompOption::Type type);
};


