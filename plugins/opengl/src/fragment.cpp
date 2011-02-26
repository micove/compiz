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

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include <core/core.h>
#include <opengl/texture.h>
#include <opengl/fragment.h>
#include "privatefragment.h"
#include "privates.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define COMP_FUNCTION_TYPE_ARB 0
#define COMP_FUNCTION_TYPE_NUM 1

#define COMP_FUNCTION_ARB_MASK (1 << 0)
#define COMP_FUNCTION_MASK     (COMP_FUNCTION_ARB_MASK)

namespace GLFragment {

    class Program {
	public:
	    Program () :
		signature (0),
		blending (false),
		name (0),
		type (GL_FRAGMENT_PROGRAM_ARB)
	    {};
	    ~Program ()
	    {
		if (name)
		    (*GL::deletePrograms) (1, &name);
	    };

	public:
	    std::vector<FunctionId> signature;

	    bool blending;

	    GLuint name;
	    GLenum type;
    };

    typedef enum {
	OpTypeData,
	OpTypeDataStore,
	OpTypeDataOffset,
	OpTypeDataBlend,
	OpTypeHeaderTemp,
	OpTypeHeaderParam,
	OpTypeHeaderAttrib,
	OpTypeColor,
	OpTypeFetch,
	OpTypeLoad
    } OpType;

    class HeaderOp {
	public:
	    HeaderOp () : type (OpTypeHeaderTemp), name ("") {};
	public:
	    OpType     type;
	    CompString name;
    };

    class BodyOp {
	public:
	    BodyOp () :
		type (OpTypeData),
		data (""),
		dst (""),
		src (""),
	        target (0)
	    {
		foreach (CompString &str, noOffset)
		    str = "";
		foreach (CompString &str, offset)
		    str = "";
	    };

	public:
	    OpType       type;
	    CompString   data;
	    CompString   dst;
	    CompString   src;
	    unsigned int target;
	    CompString   noOffset[COMP_FETCH_TARGET_NUM];
	    CompString   offset[COMP_FETCH_TARGET_NUM];

    };

    class PrivateFunctionData {
	public:
	    PrivateFunctionData () : header (0), body (0), status (true) {};
	    PrivateFunctionData (const PrivateFunctionData&, CompString);

	public:
	    std::vector<HeaderOp> header;
	    std::vector<BodyOp>   body;
	    bool                  status;
    };

    class Function {
	public:
	    Function ():
		id (0),
		name (""),
		mask (0)
	    {};

	public:
	    FunctionId          id;
	    CompString          name;
	    PrivateFunctionData data[COMP_FUNCTION_TYPE_NUM];
	    unsigned int        mask;
    };

    class PrivateAttrib {
	public:
	    PrivateAttrib () :
		opacity (0xffff),
		brightness (0xffff),
		saturation (0xffff),
		nTexture (0),
		nFunction (0),
		nParam (0)
	    {}

	    PrivateAttrib (const PrivateAttrib &pa) :
		opacity (pa.opacity),
		brightness (pa.brightness),
		saturation (pa.saturation),
		nTexture (pa.nTexture),
		nFunction (pa.nFunction),
		nParam (pa.nParam)
	    {
		for (int i = 0; i < MAX_FRAGMENT_FUNCTIONS; i++)
		    function[i] = pa.function[i];
	    }

	public:
	    GLushort   opacity;
	    GLushort   brightness;
	    GLushort   saturation;
	    int        nTexture;
	    FunctionId function[MAX_FRAGMENT_FUNCTIONS];
	    int        nFunction;
	    int        nParam;
    };

    typedef boost::function<void (BodyOp *, int)> DataOpCallBack;

    class InitialLoadFunction : public Function {
	public:
	    InitialLoadFunction ()
	    {
		id   = 0;
		name = "__core_load";
		mask = COMP_FUNCTION_MASK;

		BodyOp b;
		b.type = OpTypeLoad;
		b.noOffset[0] = "TEX output, fragment.texcoord[0], texture[0], 2D;";
		b.noOffset[1] = "TEX output, fragment.texcoord[0], texture[0], RECT;";
		b.offset[0] = "TEX output, __tmp_texcoord0, texture[0], 2D;";
		b.offset[1] = "TEX output, __tmp_texcoord0, texture[0], RECT;";
		data[0].body.push_back (b);
	    };
    };

    static InitialLoadFunction initialLoadFunction;

    static Function *
    findFragmentFunction (GLScreen   *s,
			  FunctionId id)
    {
	foreach (Function *f, s->fragmentStorage ()->functions)
	    if (f->id == id)
		return f;
	return NULL;
    }

    static Function *
    findFragmentFunctionWithName (GLScreen   *s,
				  CompString name)
    {
	foreach (Function *f, s->fragmentStorage ()->functions)
	    if (f->name.compare (name) == 0)
		return f;
	return NULL;
    }

    static Program *
    findFragmentProgram (GLScreen     *s,
			 FunctionId   *signature,
			 unsigned int nSignature)
    {
	unsigned int i;

	foreach (Program *p, s->fragmentStorage ()->programs)
	{
	    if (p->signature.size () != nSignature)
		continue;

	    for (i = 0; i < nSignature; i++)
		if (signature[i] != p->signature[i])
		    break;

	    if (i == nSignature)
		return p;
	}
	return NULL;
    }

    static unsigned int
    functionMaskToType (int mask)
    {
	static struct {
	    unsigned int type;
	    unsigned int mask;
	} maskToType[] = {
	    { COMP_FUNCTION_TYPE_ARB, COMP_FUNCTION_ARB_MASK }
	};

	unsigned int i;

	for (i = 0; i < sizeof (maskToType) / sizeof (maskToType[0]); i++)
	    if (mask & maskToType[i].mask)
		return maskToType[i].type;

	return 0;
    }

    static void
    forEachDataOpInFunction (std::vector<Function *> list,
			     int                     index,
			     int                     type,
			     int                     loadTarget,
			     CompString              loadOffset,
			     bool                    *color,
			     bool                    *blend,
			     DataOpCallBack          callBack)
    {
	Function *f = list[index];
	BodyOp   dataOp;
	bool	 colorDone = false;
	bool	 blendDone = false;

	*color = false;
	*blend = false;

	foreach (BodyOp &bodyOp, f->data[type].body)
	{
	    switch (bodyOp.type) {
		case OpTypeFetch: {
		    CompString offset = loadOffset;

		    /* add offset */
		    if (bodyOp.data.size ())
		    {
			if (loadOffset.size ())
			{
			    dataOp.type = OpTypeDataOffset;
			    dataOp.data =
				compPrintf ("ADD __tmp_texcoord%d, %s, %s;",
					    index, loadOffset.c_str (),
					    bodyOp.data.c_str ());

			    callBack (&dataOp, index);

			    offset = compPrintf ("__tmp_texcoord%d", index);
			}
			else
			{
			    offset = bodyOp.data;
			}
		    }

		    forEachDataOpInFunction (list, index - 1, type,
					    bodyOp.target,
					    offset, &colorDone, &blendDone,
					    callBack);

		    if (bodyOp.dst.compare ("output"))
		    {
			dataOp.type = OpTypeDataStore;
			dataOp.data =
			    compPrintf ("MOV %s, output;", bodyOp.dst.c_str ());

			/* move to destination */
			callBack (&dataOp, index);
		    }
		} break;
		case OpTypeLoad:
		    if (loadOffset.size ())
		    {
			dataOp.type = OpTypeDataOffset;
			dataOp.data =
			    compPrintf ("ADD __tmp_texcoord0, fragment.texcoord[0], %s;",
					loadOffset.c_str ());

			callBack (&dataOp, index);

			dataOp.data = bodyOp.offset[loadTarget];
		    }
		    else
		    {
			dataOp.data = bodyOp.noOffset[loadTarget];
		    }

		    dataOp.type = OpTypeData;

		    callBack (&dataOp, index);

		    break;
		case OpTypeColor:
		    if (!colorDone)
		    {
			dataOp.type = OpTypeData;
			dataOp.data =
			    compPrintf ("MUL %s, fragment.color, %s;",
					bodyOp.dst.c_str (),
					bodyOp.src.c_str ());

			callBack (&dataOp, index);
		    }
		    else if (bodyOp.dst.compare (bodyOp.src))
		    {
			dataOp.type = OpTypeData;
			dataOp.data =
			    compPrintf ("MOV %s, %s;",
					bodyOp.dst.c_str (),
					bodyOp.src.c_str ());

			callBack (&dataOp, index);
		    }
		    *color = true;
		    break;
		case OpTypeDataBlend:
		    *blend = true;
		    /* fall-through */
		case OpTypeData:
		    callBack (&bodyOp, index);
		    break;
		case OpTypeDataStore:
		case OpTypeDataOffset:
		case OpTypeHeaderTemp:
		case OpTypeHeaderParam:
		case OpTypeHeaderAttrib:
		    break;
	    }
	}

	if (colorDone)
	    *color = true;

	if (blendDone)
	    *blend = true;
    }

    static int
    forEachHeaderOpWithType (std::vector<HeaderOp> list,
			     int                   index,
			     OpType                type,
			     CompString            prefix,
			     CompString            functionPrefix,
			     int                   count,
			     DataOpCallBack        callBack)
    {
	BodyOp dataOp;

	dataOp.type = OpTypeData;

	foreach (HeaderOp &header, list)
	{
	    if (header.type == type)
	    {
		if (count)
		{
		    dataOp.data = ", ";
		}
		else
		{
		    dataOp.data = prefix;
		}

		dataOp.data += functionPrefix;
		dataOp.data += "_";
		dataOp.data += header.name;

		callBack (&dataOp, index);

		count++;
	    }
	}

	return count;
    }

    static bool
    forEachDataOp (std::vector<Function *> list,
		   int                     type,
		   DataOpCallBack          callBack)
    {
	BodyOp dataOp;
	bool   colorDone;
	bool   blendDone;
	int    count, nList = list.size ();

	dataOp.type = OpTypeData;

	count = 1;

	dataOp.data = "TEMP output";

	callBack (&dataOp, nList);

	foreach (Function *f, list)
	    count = forEachHeaderOpWithType (f->data[type].header,
					     nList, OpTypeHeaderTemp,
					     "", f->name, count, callBack);

	dataOp.data = ";";

	callBack (&dataOp, nList);

	count = 0;

	foreach (Function *f, list)
	    count = forEachHeaderOpWithType (f->data[type].header,
					     nList, OpTypeHeaderParam,
					     "PARAM ", f->name, count,
					     callBack);

	if (count)
	{
	    dataOp.data = ";";

	    callBack (&dataOp, nList);
	}

	count = 0;

	foreach (Function *f, list)
	    count = forEachHeaderOpWithType (f->data[type].header,
					     nList, OpTypeHeaderAttrib,
					     "ATTRIB ", f->name, count,
					     callBack);

	if (count)
	{
	    dataOp.data = ";";

	    callBack (&dataOp, nList);
	}

	forEachDataOpInFunction (list, nList - 1, type, 0, "",
				 &colorDone, &blendDone,
				 callBack);

	if (colorDone)
	    dataOp.data = "MOV result.color, output;END";
	else
	    dataOp.data = "MUL result.color, fragment.color, output;END";

	callBack (&dataOp, nList);

	return blendDone;
    }

    static void
    addFetchOffsetVariables (BodyOp     *op,
			     int	index,
			     bool       *indices,
			     CompString *data)
    {
	if (op->type == OpTypeDataOffset)
	{
	    if (!indices[index])
	    {
		data->append (compPrintf ("TEMP __tmp_texcoord%d;", index));
		indices[index] = true;
	    }
	}
    }

    static void
    addData (BodyOp     *op,
	     CompString *data)
    {
	data->append (op->data);
    }

    static Program *
    buildFragmentProgram (GLScreen      *s,
			  PrivateAttrib *attrib)
    {
	Program	                *program;
	std::vector<Function *> functionList (1);
	int                     mask = COMP_FUNCTION_MASK;
	int                     type;
	GLint                   errorPos;
	GLenum			errorType;
	CompString fetchData;
	bool       indices[MAX_FRAGMENT_FUNCTIONS];
	int        i;

	program = new Program ();
	if (!program)
	    return NULL;

	functionList[0] = &initialLoadFunction;

	for (i = 0; i < attrib->nFunction; i++)
	{
	    Function *f = findFragmentFunction (s, attrib->function[i]);

	    if (f)
	        functionList.push_back (f);
	}

	foreach (Function *f, functionList)
	    mask &= f->mask;

	if (!mask)
	{
	    compLogMessage ("opengl", CompLogLevelWarn,
			    "fragment functions can't be linked together "
			    "because a common type doesn't exist");
	}

	if (!mask || functionList.size () == 1)
	{
	    delete program;
	    return NULL;
	}

	for (i = 0; i < attrib->nFunction; i++)
	    program->signature.push_back (attrib->function[i]);

	type = functionMaskToType (mask);

	fetchData = "!!ARBfp1.0";

	foreach (bool &val, indices)
	    val = false;

	forEachDataOp (functionList, type,
	    boost::bind (addFetchOffsetVariables, _1, _2, indices, &fetchData));

	program->blending = forEachDataOp (functionList, type,
				boost::bind (addData, _1, &fetchData));

	program->type = GL_FRAGMENT_PROGRAM_ARB;

	glGetError ();

	(*GL::genPrograms) (1, &program->name);
	(*GL::bindProgram) (GL_FRAGMENT_PROGRAM_ARB, program->name);
	(*GL::programString) (GL_FRAGMENT_PROGRAM_ARB,
			      GL_PROGRAM_FORMAT_ASCII_ARB,
			      fetchData.size (), fetchData.c_str ());

	glGetIntegerv (GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
	errorType = glGetError ();
	if (errorType != GL_NO_ERROR || errorPos != -1)
	{
	    compLogMessage ("opengl", CompLogLevelError,
			    "failed to load fragment program");

	    (*GL::deletePrograms) (1, &program->name);

	    program->name = 0;
	    program->type = 0;
	}

	return program;
    }

    static GLuint
    getFragmentProgram (GLScreen      *s,
			PrivateAttrib *attrib,
			GLenum	      *type,
			bool	      *blending)
    {
	Program	*program;

	if (!attrib->nFunction)
	    return 0;

	program = findFragmentProgram (s, attrib->function, attrib->nFunction);
	if (!program)
	{
	    program = buildFragmentProgram (s, attrib);
	    if (program)
	    {
		s->fragmentStorage ()->programs.push_back (program);
	    }
	}

	if (program)
	{
	    *type     = program->type;
	    *blending = program->blending;

	    return program->name;
	}

	return 0;
    }


    /* performs simple variable substitution */
    static CompString
    copyData (std::vector<HeaderOp> header,
	      const CompString      prefix,
	      CompString            data)
    {
	CompString inPrefix (prefix);
	inPrefix += "_";

	foreach (HeaderOp &h, header)
	{
	    size_t pos = data.find (h.name);
	    while (pos != std::string::npos)
	    {
		bool prependPrefix = false;
		/* It is possible to match parts of words here, so
		 * make sure that we have found the next chunk in the
		 * string and not just a header which matches
		 * part of another word */
		if (data.size () > pos + h.name.size ())
		{
		    const CompString &token = data.substr (pos + h.name.size (), 1);
		    if (token == "," ||
			token == "." ||
			token == ";")
		    {
			prependPrefix = true;
		    }
		    else
		    {
			/* We matched part of another word as our
			 * token so search for the next whole
			 * header op */
			pos = data.find (h.name, pos + 1);
		    }
		}
		else
		{
		    /* If this is the last word in the string, then it must
		     * have matched exactly our header op, so it is ok
		     * to prepend a prefix here and go straight to
		     * std::string::npos */
		    prependPrefix = true;
		}

		if (prependPrefix)
		{
		    /* prepend the header op prefix to the header op
		     * and seek past this word to the next instance
		     * of the unprepended header op */
		    data.insert (pos, inPrefix);
		    pos += inPrefix.size () + h.name.size ();
		    pos = data.find (h.name, pos);
		}
	    }
	}

	return data;
    }

    PrivateFunctionData::PrivateFunctionData (const PrivateFunctionData& src,
					      CompString dstPrefix) :
	header (src.header),
	body (0),
	status (src.status)
    {

	foreach (BodyOp b, src.body)
	{
	    BodyOp dst;
	    dst.type = b.type;

	    switch (b.type) {
		case OpTypeFetch:
		    dst.dst = copyData (header, dstPrefix, b.dst);
		    if (b.data.size ())
			dst.data = copyData (header, dstPrefix, b.data);
		    else
			dst.data = "";

		    dst.target = b.target;
		    break;
		case OpTypeLoad:
		case OpTypeHeaderTemp:
		case OpTypeHeaderParam:
		case OpTypeHeaderAttrib:
		    break;
		case OpTypeData:
		case OpTypeDataBlend:
		case OpTypeDataStore:
		case OpTypeDataOffset:
		    dst.data = copyData (header, dstPrefix, b.data);
		    break;
		case OpTypeColor:
		    dst.dst = copyData (header, dstPrefix, b.dst);
		    dst.src = copyData (header, dstPrefix, b.src);
		    break;
		}
	    body.push_back (dst);
	}
    }

    static bool
    addHeaderOpToFunctionData (PrivateFunctionData *data,
			       const char          *name,
			       OpType              type)
    {
	static const char *reserved[] = {
	    "output",
	    "__tmp_texcoord",
	    "fragment",
	    "program",
	    "result",
	    "state",
	    "texture"
	};
	HeaderOp   header;
	CompString n (name);

	foreach (const char *word, reserved)
	{
	    if (n.find (word) != std::string::npos)
	    {
		compLogMessage ("opengl", CompLogLevelWarn,
				"%s is a reserved word", word);
		return false;
	    }
	}


	header.type = type;
	header.name = n;
	data->header.push_back (header);

	return true;
    }

    FunctionData::FunctionData () :
	priv (new PrivateFunctionData ())
    {
    }

    FunctionData::~FunctionData ()
    {
	delete priv;
    }

    bool
    FunctionData::status ()
    {
	return priv->status;
    }

    void
    FunctionData::addTempHeaderOp (const char *name)
    {
	priv->status &=
	    addHeaderOpToFunctionData (priv, name, OpTypeHeaderTemp);
    }

    void
    FunctionData::addParamHeaderOp (const char *name)
    {
	priv->status &=
	    addHeaderOpToFunctionData (priv, name, OpTypeHeaderParam);
    }

    void
    FunctionData::addAttribHeaderOp (const char *name)
    {
	priv->status &=
	    addHeaderOpToFunctionData (priv, name, OpTypeHeaderAttrib);
    }


    void
    FunctionData::addFetchOp (const char *dst, const char *offset, int target)
    {
	BodyOp b;

	b.type	 = OpTypeFetch;
	b.dst    = CompString (dst);
	b.target = target;

	if (offset)
	    b.data = CompString (offset);
	else
	    b.data = CompString ("");

	priv->body.push_back (b);
    }

    void
    FunctionData::addColorOp (const char *dst, const char *src)
    {
	BodyOp b;

	b.type = OpTypeColor;
	b.dst  = CompString (dst);
	b.src  = CompString (src);

	priv->body.push_back (b);
    }

    void
    FunctionData::addDataOp (const char *str, ...)
    {
	BodyOp  b;
	va_list ap;

	b.type = OpTypeData;
	va_start (ap, str);
	b.data = compPrintf (str, ap);
	va_end (ap);

	priv->body.push_back (b);
    }

    void
    FunctionData::addBlendOp (const char *str, ...)
    {
	BodyOp  b;
	va_list ap;

	b.type = OpTypeDataBlend;
	va_start (ap, str);
	b.data = compPrintf (str, ap);
	va_end (ap);

	priv->body.push_back (b);
    }

    FunctionId
    FunctionData::createFragmentFunction (const char *name)
    {
	GLScreen     *s = GLScreen::get (screen);
	Function     *function = new Function ();
	CompString   validName = name;
	unsigned int i = 0;

	while (findFragmentFunctionWithName (s, validName))
	{
	    validName = compPrintf ("%s%d", name, i++);
	}

	function->data[COMP_FUNCTION_TYPE_ARB] =
	    PrivateFunctionData (*priv, validName);

	function->name = validName;
	function->mask = COMP_FUNCTION_ARB_MASK;
	function->id   = s->fragmentStorage ()->lastFunctionId++;

	s->fragmentStorage ()->functions.push_back (function);

	return function->id;
    }

    Attrib::Attrib (const GLWindowPaintAttrib &paint) :
	priv (new PrivateAttrib ())
    {
	priv->opacity    = paint.opacity;
	priv->brightness = paint.brightness;
	priv->saturation = paint.saturation;
	priv->nTexture   = 0;
	priv->nFunction  = 0;
	priv->nParam     = 0;

	foreach (FunctionId &f, priv->function)
	    f = 0;
    }

    Attrib::Attrib (const Attrib &fa) :
	priv (new PrivateAttrib (*fa.priv))
    {
    }

    Attrib::~Attrib ()
    {
	delete priv;
    }

    Attrib &
    Attrib::operator= (const Attrib &rhs)
    {
	if (this == &rhs) // Check for self-assignment
	    return *this;

	delete priv;
	priv = new PrivateAttrib (*rhs.priv);

	return *this;
    }

    unsigned int
    Attrib::allocTextureUnits (unsigned int nTexture)
    {
	unsigned int first = priv->nTexture;

	priv->nTexture += nTexture;

	/* 0 is reserved for source texture */
	return 1 + first;
    }

    unsigned int
    Attrib::allocParameters (unsigned int nParam)
    {
	unsigned int first = priv->nParam;

	priv->nParam += nParam;

	return first;
    }

    void
    Attrib::addFunction (FunctionId function)
    {
	if (priv->nFunction < MAX_FRAGMENT_FUNCTIONS)
	    priv->function[priv->nFunction++] = function;
    }

    bool
    Attrib::enable (bool *blending)
    {
	GLuint name;
	GLenum type;
	bool   programBlending;

	if (!GL::fragmentProgram)
	    return false;

	name = getFragmentProgram (GLScreen::get (screen), priv, &type,
				   &programBlending);
	if (!name)
	    return false;

	*blending = !programBlending;

	glEnable (GL_FRAGMENT_PROGRAM_ARB);

	(*GL::bindProgram) (type, name);

	return true;
    }

    void
    Attrib::disable ()
    {
	glDisable (GL_FRAGMENT_PROGRAM_ARB);
    }

    unsigned short
    Attrib::getSaturation ()
    {
	return priv->saturation;
    }

    unsigned short
    Attrib::getBrightness ()
    {
	return priv->brightness;
    }

    unsigned short
    Attrib::getOpacity ()
    {
	return priv->opacity;
    }

    void
    Attrib::setSaturation (unsigned short value)
    {
	priv->saturation = value;
    }

    void
    Attrib::setBrightness (unsigned short value)
        {
	priv->brightness = value;
    }


    void
    Attrib::setOpacity (unsigned short value)
    {
	priv->opacity = value;
    }

    bool
    Attrib::hasFunctions ()
    {
	return priv->nFunction > 0;
    }

    void destroyFragmentFunction (FunctionId id)
    {
	GLScreen *s = GLScreen::get (screen);
	Function *function;
	Program  *program;

	function = findFragmentFunction (s, id);

	if (!function)
	    return;

	std::vector<Program *>::iterator it;

	do {
	    program = NULL;

	    it = s->fragmentStorage ()->programs.begin ();

	    for (; it != s->fragmentStorage ()->programs.end (); it++)
	    {
		foreach (FunctionId i, (*it)->signature)
		    if (i == id)
		    {
			program = (*it);
			break;
		    }

		if (program)
		    break;
	    }

	    if (program)
	    {
		delete program;
		s->fragmentStorage ()->programs.erase (it);
	    }

	} while (program);

	std::vector<Function *>::iterator fi =
	    std::find (s->fragmentStorage ()->functions.begin (),
		       s->fragmentStorage ()->functions.end (),
		       function);
	if (fi != s->fragmentStorage ()->functions.end ())
	    s->fragmentStorage ()->functions.erase (fi);

	delete (function);
    }

    FunctionId
    getSaturateFragmentFunction (GLTexture *texture,
				 int         param)
    {
	int      target;
	GLScreen *s = GLScreen::get (screen);

	if (param >= 64)
	    return 0;

	if (texture->target () == GL_TEXTURE_2D)
	    target = COMP_FETCH_TARGET_2D;
	else
	    target = COMP_FETCH_TARGET_RECT;

	if (!s->fragmentStorage ()->saturateFunction [target][param])
	{
	    static const char *saturateData =
		"MUL temp, output, { 1.0, 1.0, 1.0, 0.0 };"
		"DP3 temp, temp, program.env[%d];"
		"LRP output.xyz, program.env[%d].w, output, temp;";
	    FunctionData  data;

	    data.addTempHeaderOp ("temp");
	    data.addFetchOp ("output", NULL, target);
	    data.addColorOp ("output", "output");

	    data.addDataOp (saturateData, param, param);

	    if (!data.status ())
		return 0;

	    s->fragmentStorage ()->saturateFunction [target][param] =
		data.createFragmentFunction ("__core_saturate");

	}

	return s->fragmentStorage ()->saturateFunction [target][param];
    }

    Storage::Storage () :
	lastFunctionId (1),
	functions (0),
	programs (0)
    {
	for (int i = 0; i < 64; i++)
	{
	    saturateFunction[0][i] = 0;
	    saturateFunction[1][i] = 0;
	}
    }

    Storage::~Storage ()
    {
	foreach (Program *p, programs)
	    delete p;
	programs.clear ();
	foreach (Function *f, functions)
	    delete f;
	functions.clear ();
    }

};
