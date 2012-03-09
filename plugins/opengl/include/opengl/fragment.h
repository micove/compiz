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

#ifndef _GLFRAGMENT_H
#define _GLFRAGMENT_H

#define MAX_FRAGMENT_FUNCTIONS 16

#define COMP_FETCH_TARGET_2D   0
#define COMP_FETCH_TARGET_RECT 1
#define COMP_FETCH_TARGET_NUM  2

struct GLWindowPaintAttrib;
class GLScreen;
class GLTexture;

/**
 * Describes a texture modification fragment program
 * for a texture
 */
namespace GLFragment {

    class Storage;

    typedef unsigned int FunctionId;

    class PrivateFunctionData;
    class PrivateAttrib;

    class FunctionData {
	public:
	    FunctionData ();
	    ~FunctionData ();

	    /**
	     * Returns the status of this fragment program
	     * (valid or invalid)
	     */
	    bool status ();

	    void addTempHeaderOp (const char *name);

	    void addParamHeaderOp (const char *name);

	    void addAttribHeaderOp (const char *name);


	    void addFetchOp (const char *dst, const char *offset, int target);

	    void addColorOp (const char *dst, const char *src);

	    void addDataOp (const char *str, ...);

	    void addBlendOp (const char *str, ...);

	    FunctionId createFragmentFunction (const char *name);

	private:
	    PrivateFunctionData *priv;
    };

    class Attrib {
	public:
	    Attrib (const GLWindowPaintAttrib &paint);
	    Attrib (const Attrib&);
	    ~Attrib ();

	    Attrib &operator= (const Attrib &rhs);

	    unsigned int allocTextureUnits (unsigned int nTexture);

	    unsigned int allocParameters (unsigned int nParam);

	    void addFunction (FunctionId function);

	    bool enable (bool *blending);
	    void disable ();

	    unsigned short getSaturation ();
	    unsigned short getBrightness ();
	    unsigned short getOpacity ();

	    void setSaturation (unsigned short);
	    void setBrightness (unsigned short);
	    void setOpacity (unsigned short);

	    bool hasFunctions ();

	private:
	    PrivateAttrib *priv;
    };

    void destroyFragmentFunction (FunctionId id);

    FunctionId getSaturateFragmentFunction (GLTexture *texture,
					    int       param);
};



#endif
