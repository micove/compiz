/*
 * Copyright © 2010 Sam Spilsbury
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
 * Authors: Sam Spilsbury <smspillaz@gmail.com>
 */

#ifndef _COMPSERIALIZATION_H
#define _COMPSERIALIZATION_H

#include <core/core.h>
#include <core/timer.h>
#include <core/propertywriter.h>

#include <typeinfo>
#include <boost/preprocessor/cat.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/export.hpp>

#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>

#include <sstream>
#include <fstream>


template <class T>
class PluginStateWriter
{
    private:
	PropertyWriter mPw;
	Window	       mResource;
	T	       *mClassPtr;
	CompTimer      mTimeout;

	friend class boost::serialization::access;

	bool
	checkTimeout ()
	{
	    if (!screen->shouldSerializePlugins ())
		return false;

	    CompOption::Vector atomTemplate = mPw.readProperty (mResource);

	    if (atomTemplate.empty ())
		return false;

	    if (!(atomTemplate.at (0).value ().type () == CompOption::TypeString))
		return false;
	    
	    std::istringstream iss (atomTemplate.at (0).value ().s ());
	    boost::archive::text_iarchive ia (iss);
	    
	    ia >> *this;

	    postLoad ();
	    
	    /* No need to store this data in XServer anymore, get rid of it */
	    
	    mPw.deleteProperty (mResource);
	    
	    return false;
	};	    
	    
    public:

	template <class Archive>
	void serialize (Archive &ar, const unsigned int version)
	{
	     ar & *mClassPtr;;
	};    

	virtual void postLoad () {};
	
	/* Classes get destroyed in the order of:
	 * derived -> this. Because variables might
	 * have thier destructors called, we provide
	 * a method to intercept this process
	 * and immediately serialize data such that it
	 * won't be unintentionally destroyed before the
	 * base CompPluginStateWriter destructor gets called
	 */
	
	void writeSerializedData ()
	{
	    if (!screen->shouldSerializePlugins ())
		return;

	    CompOption::Vector atomTemplate = mPw.getReadTemplate ();	  
	    std::string str;  
	    std::ostringstream oss (str);
	    boost::archive::text_oarchive oa (oss);

	    /* Nothing was initially read from the property, which probably means that
	     * shouldSerializePlugins was turned on in between plugin load and unload
	     * so don't attempt to do anything here */
	    if (!atomTemplate.size ())
		return;

	    oa << *this;
	    
	    CompOption::Value v (oss.str ().c_str ());
	    atomTemplate.at (0).set (v);

	    mPw.updateProperty (mResource, atomTemplate, XA_STRING);
	}	
	
	PluginStateWriter (T *instance,
			   Window xid) :
	    mResource (xid),
	    mClassPtr (instance)
	{
	    if (screen->shouldSerializePlugins ())

	    {
		CompString atomName = compPrintf ("_COMPIZ_%s_STATE",
						  typeid (T).name ());
		CompOption::Vector o;

		o.resize (1);
		o.at (0).setName ("data", CompOption::TypeString);

		mPw = PropertyWriter (atomName, o);

		mTimeout.setCallback (boost::bind (&PluginStateWriter::checkTimeout,
						 this));
		mTimeout.setTimes (0, 0);
		mTimeout.start ();
	    }
	}
	
	virtual ~PluginStateWriter () {};

};

#endif
