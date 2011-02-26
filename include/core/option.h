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

#ifndef _COMPOPTION_H
#define _COMPOPTION_H

#include <compiz.h>

#include <vector>

class PrivateOption;
class PrivateValue;
class PrivateRestriction;
class CompAction;
class CompMatch;
class CompScreen;

/**
 * A configuration option with boolean, int, float, String, Color, Key, Button,
 * Edge, Bell, or List.
 */
class CompOption {
	/**
	 * Option data types
	 */
    public:
	typedef enum {
	    TypeBool,
	    TypeInt,
	    TypeFloat,
	    TypeString,
	    TypeColor,
	    TypeAction,
	    TypeKey,
	    TypeButton,
	    TypeEdge,
	    TypeBell,
	    TypeMatch,
	    TypeList,
	    /* internal use only */
	    TypeUnset
	} Type;

	/**
	 * A value of an Option
	 */
	class Value {
	    public:
		typedef std::vector<Value> Vector;

	    public:
		Value ();
		Value (const Value &);
		Value (const bool b);
		Value (const int i);
		Value (const float f);
		Value (const unsigned short *color);
		Value (const CompString& s);
		Value (const char *s);
		Value (const CompMatch& m);
		Value (const CompAction& a);
		Value (Type type, const Vector& l);
		~Value ();

		Type type () const;

		void set (const bool b);
		void set (const int i);
		void set (const float f);
		void set (const unsigned short *color);
		void set (const CompString& s);
		void set (const char *s);
		void set (const CompMatch& m);
		void set (const CompAction& a);
		void set (Type type, const Vector& l);

		bool               b ();
		int                i ();
		float              f ();
		unsigned short*    c ();
		CompString         s ();
		CompMatch &        match ();
		CompAction &       action ();
		Type               listType ();
		Vector &           list ();

		bool operator== (const Value& val);
		bool operator!= (const Value& val);
		Value & operator= (const Value &val);

		operator bool ();
		operator int ();
		operator float ();
		operator unsigned short * ();
		operator CompString ();
		operator CompMatch & ();
		operator CompAction & ();
		operator CompAction * ();
		operator Type ();
		operator Vector & ();

	    private:
		PrivateValue *priv;
	};

	/**
	 * TODO
	 */
	class Restriction {
	    public:
		Restriction ();
		Restriction (const Restriction &);
		~Restriction ();

		int iMin ();
		int iMax ();
		float fMin ();
		float fMax ();
		float fPrecision ();

		void set (int, int);
		void set (float, float, float);

		bool inRange (int);
		bool inRange (float);

		Restriction & operator= (const Restriction &rest);
	    private:
		PrivateRestriction *priv;
	};

	typedef std::vector<CompOption> Vector;

	/**
	 * TODO
	 */
	class Class {
	    public:
		virtual Vector & getOptions () = 0;

		virtual CompOption * getOption (const CompString &name);

		virtual bool setOption (const CompString &name,
					Value            &value) = 0;
	};

    public:
	CompOption ();
	CompOption (const CompOption &);
	CompOption (CompString name, Type type);
	~CompOption ();

	void setName (CompString name, Type type);

	void reset ();

	CompString name ();

	Type type ();
	Value & value ();
	Restriction & rest ();

	bool set (Value &val);
	bool isAction ();

	CompOption & operator= (const CompOption &option);

    public:
	static CompOption * findOption (Vector &options, CompString name,
					unsigned int *index = NULL);

	static bool
	getBoolOptionNamed (const Vector& options,
			    const CompString& name,
			    bool defaultValue = false);

	static int
	getIntOptionNamed (const Vector& options,
			   const CompString& name,
			   int defaultValue = 0);

	static float
	getFloatOptionNamed (const Vector& options,
			     const CompString& name,
			     const float& defaultValue = 0.0);

	static CompString
	getStringOptionNamed (const Vector& options,
			      const CompString& name,
			      const CompString& defaultValue = "");

	static unsigned short *
	getColorOptionNamed (const Vector& options,
			     const CompString& name,
			     unsigned short *defaultValue);

	static CompMatch
	getMatchOptionNamed (const Vector& options,
			     const CompString& name,
			     const CompMatch& defaultValue);

	static CompString typeToString (Type type);

	static bool stringToColor (CompString     color,
				   unsigned short *rgba);

	static CompString colorToString (unsigned short *rgba);


	static bool setOption (CompOption  &o, Value &value);

    private:
	PrivateOption *priv;
};

extern CompOption::Vector noOptions;

#endif
