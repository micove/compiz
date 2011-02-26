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

#ifndef _COMPMETADATA_H
#define _COMPMETADATA_H

#include <vector>

#include <libxml/parser.h>

#include <core/action.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY (x)
#define MINTOSTRING(x) "<min>" TOSTRING (x) "</min>"
#define MAXTOSTRING(x) "<max>" TOSTRING (x) "</max>"
#define RESTOSTRING(min, max) MINTOSTRING (min) MAXTOSTRING (max)

class PrivateMetadata;

class CompMetadata {
    public:
	struct OptionInfo {
	    const char           *name;
	    const char           *type;
	    const char           *data;
	    CompAction::CallBack initiate;
	    CompAction::CallBack terminate;
	};
    public:
        CompMetadata (CompString       plugin = "core",
		      const OptionInfo *optionInfo = NULL,
		      unsigned int     nOptionInfo = 0);
	~CompMetadata ();

	std::vector<xmlDoc *> &doc ();

	bool addFromFile (CompString file, bool prepend = false);
	bool addFromString (CompString string, bool prepend = false);
	bool addFromIO (xmlInputReadCallback  ioread,
		        xmlInputCloseCallback ioclose,
		        void                  *ioctx,
		        bool                  prepend = false);
	bool addFromOptionInfo (const OptionInfo *optionInfo,
		                unsigned int     nOptionInfo,
				bool             prepend = false);

	bool initOption (CompOption *option,
			 CompString name);

	bool initOptions (const OptionInfo   *info,
			  unsigned int       nOptions,
			  CompOption::Vector &options);



	CompString getShortPluginDescription ();

	CompString getLongPluginDescription ();

	CompString getShortOptionDescription (CompOption *option);

	CompString getLongOptionDescription (CompOption *option);
	
	CompString getStringFromPath (CompString path);

	static unsigned int readXmlChunk (const char   *src,
					  unsigned int *offset,
					  char         *buffer,
					  unsigned int length);

	static unsigned int
	readXmlChunkFromOptionInfo (const CompMetadata::OptionInfo *info,
				    unsigned int                   *offset,
				    char                           *buffer,
				    unsigned int                   length);

    private:
	PrivateMetadata *priv;
};

#endif
