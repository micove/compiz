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

#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <locale.h>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <compiz.h>
#include <core/metadata.h>
#include <core/screen.h>
#include "privatescreen.h"
#include "privatemetadata.h"

#define HOME_METADATADIR ".compiz/metadata"
#define EXTENSION ".xml"

static xmlDoc *
readXmlFile (CompString name,
	     CompString path = "")
{
    CompString file;
    xmlDoc *doc = NULL;
    FILE   *fp;

    if (path.size ())
	file = compPrintf ("%s/%s%s", path.c_str (), name.c_str (), EXTENSION);
    else
	file = compPrintf ("%s%s", name.c_str (), EXTENSION);

    fp = fopen (file.c_str (), "r");
    if (!fp)
    {
	return NULL;
    }

    fclose (fp);

    doc = xmlReadFile (file.c_str (), NULL, 0);

    return doc;
}

typedef struct _CompIOCtx {
    unsigned int                   offset;
    const char                     *name;
    const CompMetadata::OptionInfo *oInfo;
    unsigned int                   nOInfo;
} CompIOCtx;

typedef struct _CompXPath {
    xmlXPathObjectPtr  obj;
    xmlXPathContextPtr ctx;
    xmlDocPtr	       doc;
} CompXPath;


static int
readPluginXmlCallback (void *context,
		       char *buffer,
		       int  length)
{
    CompIOCtx    *ctx = (CompIOCtx *) context;
    unsigned int offset = ctx->offset;
    unsigned int i, j;

    i = CompMetadata::readXmlChunk ("<compiz><plugin name=\"", &offset,
				    buffer, length);
    i += CompMetadata::readXmlChunk (ctx->name, &offset, buffer + i,
				     length - i);
    i += CompMetadata::readXmlChunk ("\">", &offset, buffer + i, length - i);

    if (ctx->nOInfo)
    {
	i += CompMetadata::readXmlChunk ("<options>", &offset, buffer + i,
					 length - i);

	for (j = 0; j < ctx->nOInfo; j++)
	    i += CompMetadata::readXmlChunkFromOptionInfo (
		   &ctx->oInfo[j], &offset, buffer + i, length - i);

	i += CompMetadata::readXmlChunk ("</options>", &offset, buffer + i,
					 length - i);
    }

    i += CompMetadata::readXmlChunk ("</plugin></compiz>", &offset, buffer + i,
				     length - i);

    if (!offset && length > (int)i)
	buffer[i++] = '\0';

    ctx->offset += i;

    return i;
}

static bool
initXPathFromMetadataPath (CompXPath	 *xPath,
			   CompMetadata  *metadata,
			   const xmlChar *path)
{
    xmlXPathObjectPtr  obj;
    xmlXPathContextPtr ctx;

    foreach (xmlDoc *doc, metadata->doc ())
    {
	ctx = xmlXPathNewContext (doc);
	if (ctx)
	{
	    obj = xmlXPathEvalExpression (path, ctx);
	    if (obj)
	    {
		if (obj->nodesetval && obj->nodesetval->nodeNr)
		{
		    xPath->ctx = ctx;
		    xPath->obj = obj;
		    xPath->doc = doc;

		    return true;
		}

		xmlXPathFreeObject (obj);
	    }

	    xmlXPathFreeContext (ctx);
	}
    }

    return false;
}

static bool
initXPathFromMetadataPathElement (CompXPath	*xPath,
				  CompMetadata  *metadata,
				  const xmlChar *path,
				  const xmlChar *element)
{
    char str[1024];

    snprintf (str, 1024, "%s/%s", path, element);

    return initXPathFromMetadataPath (xPath, metadata, BAD_CAST str);
}

static void
finiXPath (CompXPath *xPath)
{
    xmlXPathFreeObject (xPath->obj);
    xmlXPathFreeContext (xPath->ctx);
}

static CompOption::Type
getOptionType (const char *name)
{
    static struct _TypeMap {
	const char       *name;
	CompOption::Type type;
    } map[] = {
	{ "int",    CompOption::TypeInt    },
	{ "float",  CompOption::TypeFloat  },
	{ "string", CompOption::TypeString },
	{ "color",  CompOption::TypeColor  },
	{ "action", CompOption::TypeAction },
	{ "key",    CompOption::TypeKey    },
	{ "button", CompOption::TypeButton },
	{ "edge",   CompOption::TypeEdge   },
	{ "bell",   CompOption::TypeBell   },
	{ "match",  CompOption::TypeMatch  },
	{ "list",   CompOption::TypeList   }
    };
    unsigned int i;

    for (i = 0; i < sizeof (map) / sizeof (map[0]); i++)
	if (strcasecmp (name, map[i].name) == 0)
	    return map[i].type;

    return CompOption::TypeBool;
}

static void
initBoolValue (CompOption::Value &v,
	       xmlDocPtr         doc,
	       xmlNodePtr        node)
{
    xmlChar *value;

    v.set (false);

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	if (strcasecmp ((char *) value, "true") == 0)
	    v.set (true);

	xmlFree (value);
    }
}

static void
initIntValue (CompOption::Value       &v,
	      CompOption::Restriction &r,
	      xmlDocPtr               doc,
	      xmlNodePtr              node)
{
    xmlChar *value;

    v.set ((r.iMin () + r.iMax ()) / 2);

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	int i = strtol ((char *) value, NULL, 0);

	if (r.inRange (i))
	    v.set (i);

	xmlFree (value);
    }
}

static void
initFloatValue (CompOption::Value       &v,
		CompOption::Restriction &r,
		xmlDocPtr               doc,
		xmlNodePtr              node)
{
    xmlChar *value;
    char *loc;

    v.set ((r.fMin () + r.fMax ()) / 2);

    if (!doc)
	return;

    loc = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "C");
    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	float f = strtod ((char *) value, NULL);

	if (r.inRange (f))
	    v.set (f);

	xmlFree (value);
    }
    setlocale (LC_NUMERIC, loc);
}

static void
initStringValue (CompOption::Value       &v,
		 CompOption::Restriction &r,
		 xmlDocPtr               doc,
		 xmlNodePtr              node)
{
    xmlChar *value;

    v.set ("");

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	v.set (CompString ((char *) value));
	xmlFree (value);
    }
}

static void
initColorValue (CompOption::Value &v,
		xmlDocPtr         doc,
		xmlNodePtr        node)
{
    xmlNodePtr child;

    unsigned short c[4] = { 0x0000, 0x0000, 0x0000, 0xffff};
    v.set (c);

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	xmlChar *value;
	int	index;

	if (!xmlStrcmp (child->name, BAD_CAST "red"))
	    index = 0;
	else if (!xmlStrcmp (child->name, BAD_CAST "green"))
	    index = 1;
	else if (!xmlStrcmp (child->name, BAD_CAST "blue"))
	    index = 2;
	else if (!xmlStrcmp (child->name, BAD_CAST "alpha"))
	    index = 3;
	else
	    continue;

	value = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
	if (value)
	{
	    int color = strtol ((char *) value, NULL , 0);

	    c[index] = MAX (0, MIN (0xffff, color));

	    xmlFree (value);
	}
    }
    v.set (c);
}

static void
initActionValue (CompOption::Value &v,
		 CompAction::State state,
		 xmlDocPtr         doc,
		 xmlNodePtr        node)
{
    v.set (CompAction ());
    v.action ().setState (state);
}

static void
initKeyValue (CompOption::Value &v,
	      CompAction::State state,
	      xmlDocPtr         doc,
	      xmlNodePtr        node)
{
    xmlChar    *value;
    CompAction action;

    action.setState (state | CompAction::StateInitKey);

    if (doc)
    {
	value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	if (value)
	{
	    char *binding = (char *) value;

	    if (strcasecmp (binding, "disabled") && *binding)
		action.keyFromString (binding);

	    xmlFree (value);
	}

	v.set (action);

	if (state & CompAction::StateAutoGrab)
	    screen->addAction (&v.action ());
    }
    else
	v.set (action);
}

static void
initButtonValue (CompOption::Value &v,
		 CompAction::State state,
		 xmlDocPtr         doc,
		 xmlNodePtr        node)
{
    xmlChar    *value;
    CompAction action;

    action.setState (state | CompAction::StateInitButton |
		     CompAction::StateInitEdge);

    if (doc)
    {
	value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	if (value)
	{
	    char *binding = (char *) value;

	    if (strcasecmp (binding, "disabled") && *binding)
		action.buttonFromString (binding);

	    xmlFree (value);
	}

	v.set (action);

	if (state & CompAction::StateAutoGrab)
	    screen->addAction (&v.action ());
    }
    else
	v.set (action);
}

static void
initEdgeValue (CompOption::Value &v,
	       CompAction::State state,
	       xmlDocPtr         doc,
	       xmlNodePtr        node)
{
    xmlNodePtr   child;
    xmlChar      *value;
    CompAction   action;
    unsigned int edge = 0;

    action.setState (state | CompAction::StateInitEdge);

    if (doc)
    {
	for (child = node->xmlChildrenNode; child; child = child->next)
	{
	    value = xmlGetProp (child, BAD_CAST "name");
	    if (value)
	    {
		int i;

		for (i = 0; i < SCREEN_EDGE_NUM; i++)
		    if (strcasecmp ((char *) value,
				    CompAction::edgeToString (i).c_str ()) == 0)
			edge |= (1 << i);

		xmlFree (value);
	    }
	}

	action.setEdgeMask (edge);
	v.set (action);

	if (state & CompAction::StateAutoGrab)
	    screen->addAction (&v.action ());
    }
    else
    {
	action.setEdgeMask (edge);
	v.set (action);
    }
}

static void
initBellValue (CompOption::Value &v,
	       CompAction::State state,
	       xmlDocPtr         doc,
	       xmlNodePtr        node)
{
    xmlChar    *value;
    CompAction action;

    action.setState (state | CompAction::StateInitBell);

    if (doc)
    {
	value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	if (value)
	{
	    if (strcasecmp ((char *) value, "true") == 0)
		action.setBell (true);

	    xmlFree (value);
	}
    }
    v.set (action);
}

static void
initMatchValue (CompOption::Value &v,
		bool		  helper,
		xmlDocPtr         doc,
		xmlNodePtr        node)
{
    xmlChar *value;

    v.set (CompMatch ());

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	v.match () = (char *) value;
	xmlFree (value);
    }

    if (!helper)
	v.match ().update ();
}

static void
initListValue (CompOption::Value       &v,
	       CompOption::Restriction &r,
	       CompAction::State       state,
	       bool                    helper,
	       xmlDocPtr               doc,
	       xmlNodePtr              node)
{
    xmlNodePtr child;

    v.list ().clear ();

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	CompOption::Value value;

	if (xmlStrcmp (child->name, BAD_CAST "value"))
	    continue;

	switch (v.listType ()) {
	    case CompOption::TypeBool:
		initBoolValue (value, doc, child);
		break;
	    case CompOption::TypeInt:
		initIntValue (value, r, doc, child);
		break;
	    case CompOption::TypeFloat:
		initFloatValue (value, r, doc, child);
		break;
	    case CompOption::TypeString:
		initStringValue (value, r, doc, child);
		break;
	    case CompOption::TypeColor:
		initColorValue (value, doc, child);
		break;
	    case CompOption::TypeAction:
		initActionValue (value, state, doc, child);
		break;
	    case CompOption::TypeKey:
		initKeyValue (value, state, doc, child);
		break;
	    case CompOption::TypeButton:
		initButtonValue (value, state, doc, child);
		break;
	    case CompOption::TypeEdge:
		initEdgeValue (value, state, doc, child);
		break;
	    case CompOption::TypeBell:
		initBellValue (value, state, doc, child);
		break;
	    case CompOption::TypeMatch:
		initMatchValue (value, helper, doc, child);
	    default:
		break;
	}

	v.list ().push_back (value);
    }
}

static CompString
stringFromMetadataPathElement (CompMetadata *metadata,
			       const char   *path,
			       const char   *element)
{
    return metadata->getStringFromPath (compPrintf ("%s/%s", path, element));
}

static Bool
boolFromMetadataPathElement (CompMetadata *metadata,
			     const char   *path,
			     const char   *element,
			     Bool	  defaultValue)
{
    Bool value = FALSE;
    CompString str;

    str = stringFromMetadataPathElement (metadata, path, element);

    if (strcasecmp (str.c_str (), "true") == 0)
	value = TRUE;

    return value;
}

static void
initIntRestriction (CompMetadata            *metadata,
		    CompOption::Restriction &r,
		    const char              *path)
{
    CompString value;
    int  min = MINSHORT, max = MAXSHORT;

    value = stringFromMetadataPathElement (metadata, path, "min");
    if (!value.empty ())
	min = strtol (value.c_str (), NULL, 0);

    value = stringFromMetadataPathElement (metadata, path, "max");
    if (!value.empty ())
	max = strtol (value.c_str (), NULL, 0);

    r.set (min, max);
}

static void
initFloatRestriction (CompMetadata            *metadata,
		      CompOption::Restriction &r,
		      const char              *path)
{
    CompString value;
    char *loc;

    float min       = MINSHORT;
    float max       = MAXSHORT;
    float precision = 0.1f;

    loc = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "C");
    value = stringFromMetadataPathElement (metadata, path, "min");
    if (!value.empty ())
	min = strtod (value.c_str (), NULL);

    value = stringFromMetadataPathElement (metadata, path, "max");
    if (!value.empty ())
	max = strtod (value.c_str (), NULL);

    value = stringFromMetadataPathElement (metadata, path, "precision");
    if (!value.empty ())
	precision = strtod (value.c_str (), NULL);

    r.set (min, max, precision);

    setlocale (LC_NUMERIC, loc);
}

static void
initActionState (CompMetadata      *metadata,
		 CompOption::Type  type,
		 CompAction::State *state,
		 const char        *path)
{
    static struct _StateMap {
	const char        *name;
	CompAction::State state;
    } map[] = {
	{ "key",     CompAction::StateInitKey     },
	{ "button",  CompAction::StateInitButton  },
	{ "bell",    CompAction::StateInitBell    },
	{ "edge",    CompAction::StateInitEdge    },
	{ "edgednd", CompAction::StateInitEdgeDnd }
    };

    CompXPath xPath;
    CompString value;

    *state = CompAction::StateAutoGrab;

    value = stringFromMetadataPathElement (metadata, path, "passive_grab");
    if (!value.empty ())
	if (value == "false")
	    *state = 0;

    if (type == CompOption::TypeEdge)
    {
	value = stringFromMetadataPathElement (metadata, path, "nodelay");
	if (!value.empty ())
	    if (value == "true")
		*state |= CompAction::StateNoEdgeDelay;
    }

    if (!initXPathFromMetadataPathElement (&xPath, metadata, BAD_CAST path,
					   BAD_CAST "allowed"))
	return;

    for (unsigned int i = 0; i < sizeof (map) / sizeof (map[0]); i++)
    {
	xmlChar *value;

	value = xmlGetProp (*xPath.obj->nodesetval->nodeTab,
			    BAD_CAST map[i].name);
	if (value)
	{
	    if (xmlStrcmp (value, BAD_CAST "true") == 0)
		*state |= map[i].state;
	    xmlFree (value);
	}
    }

    finiXPath (&xPath);
}

static bool
initOptionFromMetadataPath (CompMetadata  *metadata,
			    CompOption	  *option,
			    const xmlChar *path)
{
    CompXPath	      xPath, xDefaultPath;
    xmlNodePtr	      node, defaultNode;
    xmlDocPtr	      defaultDoc;
    xmlChar	      *name, *type;
    CompString	      value;
    CompAction::State state = 0;
    bool	      helper = false;
    CompOption::Type  oType = CompOption::TypeBool;

    CompOption::Value::Vector emptyList (0);

    if (!initXPathFromMetadataPath (&xPath, metadata, path))
	return FALSE;

    node = *xPath.obj->nodesetval->nodeTab;

    type = xmlGetProp (node, BAD_CAST "type");
    if (type)
    {
	oType = getOptionType ((char *) type);
	xmlFree (type);
    }

    name = xmlGetProp (node, BAD_CAST "name");
    option->setName ((char *) name, oType);
    xmlFree (name);

    if (initXPathFromMetadataPathElement (&xDefaultPath, metadata, path,
					  BAD_CAST "default"))
    {
	defaultDoc  = xDefaultPath.doc;
	defaultNode = *xDefaultPath.obj->nodesetval->nodeTab;
    }
    else
    {
	defaultDoc  = NULL;
	defaultNode = NULL;
    }

    switch (option->type ()) {
	case CompOption::TypeBool:
	    initBoolValue (option->value (), defaultDoc, defaultNode);
	    break;
	case CompOption::TypeInt:
	    initIntRestriction (metadata, option->rest (), (char *) path);
	    initIntValue (option->value (), option->rest (),
			  defaultDoc, defaultNode);
	    break;
	case CompOption::TypeFloat:
	    initFloatRestriction (metadata, option->rest (), (char *) path);
	    initFloatValue (option->value (), option->rest (),
			    defaultDoc, defaultNode);
	    break;
	case CompOption::TypeString:
	    initStringValue (option->value (), option->rest (),
			     defaultDoc, defaultNode);
	    break;
	case CompOption::TypeColor:
	    initColorValue (option->value (), defaultDoc, defaultNode);
	    break;
	case CompOption::TypeAction:
	    initActionState (metadata, option->type (), &state, (char *) path);
	    initActionValue (option->value (), state,
			     defaultDoc, defaultNode);
	    break;
	case CompOption::TypeKey:
	    initActionState (metadata, option->type (), &state, (char *) path);
	    initKeyValue (option->value (), state,
			  defaultDoc, defaultNode);
	    break;
	case CompOption::TypeButton:
	    initActionState (metadata, option->type (), &state, (char *) path);
	    initButtonValue (option->value (), state,
			     defaultDoc, defaultNode);
	    break;
	case CompOption::TypeEdge:
	    initActionState (metadata, option->type (), &state, (char *) path);
	    initEdgeValue (option->value (), state,
			   defaultDoc, defaultNode);
	    break;
	case CompOption::TypeBell:
	    initActionState (metadata, option->type (), &state, (char *) path);
	    initBellValue (option->value (), state,
			   defaultDoc, defaultNode);
	    break;
	case CompOption::TypeMatch:
	    helper = boolFromMetadataPathElement (metadata, (char *) path,
						  "helper", false);
	    initMatchValue (option->value (), helper,
			    defaultDoc, defaultNode);
	    break;
	case CompOption::TypeList:
	    value = stringFromMetadataPathElement (metadata, (char *) path,
						   "type");
	    if (!value.empty ())
		option->value ().set (getOptionType (value.c_str ()), emptyList);
	    else
		option->value ().set (CompOption::TypeBool, emptyList);

	    switch (option->value ().listType ()) {
		case CompOption::TypeInt:
		    initIntRestriction (metadata, option->rest (),
					(char *) path);
		    break;
		case CompOption::TypeFloat:
		    initFloatRestriction (metadata, option->rest (),
					  (char *) path);
		    break;
		case CompOption::TypeAction:
		case CompOption::TypeKey:
		case CompOption::TypeButton:
		case CompOption::TypeEdge:
		case CompOption::TypeBell:
		    initActionState (metadata, option->value ().listType (),
				     &state, (char *) path);
		    break;
		case CompOption::TypeMatch:
		    helper = boolFromMetadataPathElement (metadata,
							  (char *) path,
							  "helper", false);
		default:
		    break;
	    }

	    initListValue (option->value (), option->rest (), state, helper,
			   defaultDoc, defaultNode);
	    break;
    }

    if (defaultDoc)
	finiXPath (&xDefaultPath);

    finiXPath (&xPath);

    return TRUE;
}

CompMetadata::CompMetadata (CompString       plugin,
			    const OptionInfo *optionInfo,
			    unsigned int     nOptionInfo)
{
    priv = new PrivateMetadata (plugin, compPrintf ("plugin[@name=\"%s\"]",
						    plugin.c_str ()));
    if (nOptionInfo)
    {
	CompIOCtx ctx;

	ctx.offset	  = 0;
	ctx.name	  = plugin.c_str ();
	ctx.oInfo  = optionInfo;
	ctx.nOInfo = nOptionInfo;

	addFromIO (readPluginXmlCallback, NULL, (void *) &ctx);
    }
}

CompMetadata::~CompMetadata ()
{
    delete priv;
}

std::vector<xmlDoc *> &
CompMetadata::doc ()
{
    return priv->mDoc;
}


bool
CompMetadata::addFromFile (CompString file, bool prepend)
{
    xmlDoc     *doc;
    CompString home (getenv ("HOME"));
    bool       status = false;

    home = getenv ("HOME");
    if (home.size ())
    {
	CompString path = compPrintf ("%s/%s", home.c_str (), HOME_METADATADIR);
	doc = readXmlFile (file, path);
	if (doc)
	{
	    if (prepend)
		priv->mDoc.insert (priv->mDoc.begin (), doc);
	    else
		priv->mDoc.push_back (doc);

	    status = true;
	}
    }

    doc = readXmlFile (file, CompString (METADATADIR));
    if (doc)
    {
	if (prepend)
	    priv->mDoc.insert (priv->mDoc.begin (), doc);
	else
	    priv->mDoc.push_back (doc);

	status |= true;
    }

    if (!status)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Unable to parse XML metadata from file \"%s%s\"",
			file.c_str (), EXTENSION);

	return false;
    }

    return true;
}

bool
CompMetadata::addFromString (CompString string, bool prepend)
{
    xmlDoc *doc;

    doc = xmlReadMemory (string.c_str (), string.size (), NULL, NULL, 0);
    if (!doc)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Unable to parse XML metadata");

	return false;
    }

    if (prepend)
	priv->mDoc.insert (priv->mDoc.begin (), doc);
    else
	priv->mDoc.push_back (doc);

    return true;
}

bool
CompMetadata::addFromIO (xmlInputReadCallback  ioread,
			 xmlInputCloseCallback ioclose,
			 void                  *ioctx,
			 bool                  prepend)
{
    xmlDoc *doc;

    doc = xmlReadIO (ioread, ioclose, ioctx, NULL, NULL, 0);
    if (!doc)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Unable to parse XML metadata");

	return false;
    }

    if (prepend)
	priv->mDoc.insert (priv->mDoc.begin (), doc);
    else
	priv->mDoc.push_back (doc);

    return true;
}

bool
CompMetadata::addFromOptionInfo (const OptionInfo *optionInfo,
				 unsigned int     nOptionInfo,
				 bool             prepend)
{
    if (nOptionInfo)
    {
	CompIOCtx ctx;

	ctx.offset	  = 0;
	ctx.name	  = priv->mPlugin.c_str ();
	ctx.oInfo  = optionInfo;
	ctx.nOInfo = nOptionInfo;

	return addFromIO (readPluginXmlCallback, NULL, (void *) &ctx, prepend);
    }
    else
	return false;
}

bool
CompMetadata::initOption (CompOption  *option,
			  CompString  name)
{
    char str[1024];

    sprintf (str, "/compiz/%s/options//option[@name=\"%s\"]",
	     priv->mPath.c_str (), name.c_str ());

    return initOptionFromMetadataPath (this, option, BAD_CAST str);
}

bool
CompMetadata::initOptions (const OptionInfo   *info,
			   unsigned int       nOptions,
			   CompOption::Vector &opt)
{
    if (opt.size () < nOptions)
	opt.resize (nOptions);

    for (unsigned int i = 0; i < nOptions; i++)
    {
	if (!initOption (&opt[i], info[i].name))
	{
	    return false;
	}
	
	if (info[i].initiate)
	    opt[i].value ().action ().setInitiate  (info[i].initiate);

	if (info[i].terminate)
	    opt[i].value ().action ().setTerminate (info[i].terminate);
    }

    return true;
}

CompString
CompMetadata::getShortPluginDescription ()
{
    return getStringFromPath (
	compPrintf ("/compiz/%s/short/child::text()", priv->mPath.c_str ()));
}

CompString
CompMetadata::getLongPluginDescription ()
{
    return getStringFromPath (
	compPrintf ("/compiz/%s/long/child::text()", priv->mPath.c_str ()));
}

CompString
CompMetadata::getShortOptionDescription (CompOption *option)
{
    return getStringFromPath (
	compPrintf (
	    "/compiz/%s/options//option[@name=\"%s\"]/short/child::text()",
	    priv->mPath.c_str (), option->name ().c_str ()));
}

CompString
CompMetadata::getLongOptionDescription (CompOption *option)
{
    return getStringFromPath (
	compPrintf (
	    "/compiz/%s/options//option[@name=\"%s\"]/long/child::text()",
	    priv->mPath.c_str (), option->name ().c_str ()));
}



CompString
CompMetadata::getStringFromPath (CompString path)
{
    CompXPath  xPath;
    CompString v = "";

    if (!initXPathFromMetadataPath (&xPath, this, BAD_CAST path.c_str ()))
	return "";

    xPath.obj = xmlXPathConvertString (xPath.obj);

    if (xPath.obj->type == XPATH_STRING && xPath.obj->stringval)
	v = (char *) xPath.obj->stringval;

    finiXPath (&xPath);

    return v;
}

unsigned int
CompMetadata::readXmlChunk (const char   *src,
			    unsigned int *offset,
			    char         *buffer,
			    unsigned int length)
{
    unsigned int srcLength = strlen (src);
    unsigned int srcOffset = *offset;

    if (srcOffset > srcLength)
	srcOffset = srcLength;

    *offset -= srcOffset;

    src += srcOffset;
    srcLength -= srcOffset;

    if (srcLength > 0 && length > 0)
    {
	if (srcLength < length)
	    length = srcLength;

	memcpy (buffer, src, length);

	return length;
    }

    return 0;
}

unsigned int
CompMetadata::readXmlChunkFromOptionInfo (const CompMetadata::OptionInfo *info,
					  unsigned int                   *offset,
					  char                           *buffer,
					  unsigned int                   length)
{
    unsigned int i;

    i = readXmlChunk ("<option name=\"", offset, buffer, length);
    i += readXmlChunk (info->name, offset, buffer + i, length - i);

    if (info->type)
    {
	i += readXmlChunk ("\" type=\"", offset, buffer + i, length - i);
	i += readXmlChunk (info->type, offset, buffer + i, length - i);
    }

    if (info->data)
    {
	i += readXmlChunk ("\">", offset, buffer + i, length - i);
	i += readXmlChunk (info->data, offset, buffer + i, length - i);
	i += readXmlChunk ("</option>", offset, buffer + i, length - i);
    }
    else
    {
	i += readXmlChunk ("\"/>", offset, buffer + i, length - i);
    }

    return i;
}

PrivateMetadata::PrivateMetadata (CompString plugin, CompString path) :
    mPlugin (plugin),
    mPath (path),
    mDoc (0)
{
}

PrivateMetadata::~PrivateMetadata ()
{
    foreach (xmlDoc *d, mDoc)
	xmlFreeDoc (d);
}

