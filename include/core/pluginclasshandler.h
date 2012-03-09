/*
 * Copyright © 2008 Dennis Kasprzyk
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
 */

#ifndef _COMPPLUGINCLASSHANDLER_H
#define _COMPPLUGINCLASSHANDLER_H

#include <typeinfo>
#include <boost/preprocessor/cat.hpp>

#include <compiz.h>
#include <core/screen.h>
#include <core/pluginclasses.h>

extern unsigned int pluginClassHandlerIndex;

template<class Tp, class Tb, int ABI = 0>
class PluginClassHandler {
    public:
	PluginClassHandler (Tb *);
	~PluginClassHandler ();

	void setFailed () { mFailed = true; };
	bool loadFailed () { return mFailed; };

	Tb * get () { return mBase; };
	static Tp * get (Tb *);

    private:
	static CompString keyName ()
	{
	    return compPrintf ("%s_index_%lu", typeid (Tp).name (), ABI);
	}

    private:
	bool mFailed;
	Tb   *mBase;

	static PluginClassIndex mIndex;
};

template<class Tp, class Tb, int ABI>
PluginClassIndex PluginClassHandler<Tp,Tb,ABI>::mIndex;

template<class Tp, class Tb, int ABI>
PluginClassHandler<Tp,Tb,ABI>::PluginClassHandler (Tb *base) :
    mFailed (false),
    mBase (base)
{
    if (mIndex.pcFailed)
    {
	mFailed = true;
    }
    else
    {
	if (!mIndex.initiated)
	{
	    mIndex.index = Tb::allocPluginClassIndex ();
	    if (mIndex.index != (unsigned)~0)
	    {
		mIndex.initiated = true;
		mIndex.failed    = false;
		mIndex.pcIndex = pluginClassHandlerIndex;

		CompPrivate p;
		p.uval = mIndex.index;

		if (!screen->hasValue (keyName ()))
		{
		    screen->storeValue (keyName (), p);
		    pluginClassHandlerIndex++;
		}
		else
		{
		    compLogMessage ("core", CompLogLevelFatal,
			"Private index value \"%s\" already stored in screen.",
			keyName ().c_str ());
		}
	    }
	    else
	    {
		mIndex.failed = true;
		mIndex.initiated = false;
		mIndex.pcFailed = true;
		mIndex.pcIndex = pluginClassHandlerIndex;
		mFailed = true;
	    }
	}

	if (!mIndex.failed)
	{
	    mIndex.refCount++;
	    mBase->pluginClasses[mIndex.index] = static_cast<Tp *> (this);
	}
    }
}

template<class Tp, class Tb, int ABI>
PluginClassHandler<Tp,Tb,ABI>::~PluginClassHandler ()
{
    if (!mIndex.pcFailed)
    {
	mIndex.refCount--;

	if (mIndex.refCount == 0)
	{
	    Tb::freePluginClassIndex (mIndex.index);
	    mIndex.initiated = false;
	    mIndex.failed = false;
	    mIndex.pcIndex = pluginClassHandlerIndex;
	    screen->eraseValue (keyName ());
	    pluginClassHandlerIndex++;
	}
    }
}

template<class Tp, class Tb, int ABI>
Tp *
PluginClassHandler<Tp,Tb,ABI>::get (Tb *base)
{
    if (mIndex.initiated && pluginClassHandlerIndex == mIndex.pcIndex)
    {
	return static_cast<Tp *>
	    (base->pluginClasses[mIndex.index]);
    }
    if (mIndex.failed && pluginClassHandlerIndex == mIndex.pcIndex)
	return NULL;

    if (screen->hasValue (keyName ()))
    {
	mIndex.index     = screen->getValue (keyName ()).uval;
	mIndex.initiated = true;
	mIndex.failed    = false;
	mIndex.pcIndex = pluginClassHandlerIndex;
	return static_cast<Tp *> (base->pluginClasses[mIndex.index]);
    }
    else
    {
	mIndex.initiated = false;
	mIndex.failed    = true;
	mIndex.pcIndex = pluginClassHandlerIndex;
	return NULL;
    }
}

#endif
