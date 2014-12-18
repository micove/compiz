/*
 * Compiz configuration system library
 *
 * Copyright (C) 2012 Canonical Ltd
 * Copyright (C) 2012 Sam Spilsbury <smspillaz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <tr1/tuple>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <gtest_shared_autodestroy.h>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/function.hpp>

#include <X11/Xlib.h>

#include <ccs.h>

#include "compizconfig_ccs_setting_mock.h"
#include "compizconfig_ccs_plugin_mock.h"
#include "compizconfig_ccs_list_wrapper.h"

namespace cci = compiz::config::impl;
namespace cc = compiz::config;

using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::WithArgs;
using ::testing::WithParamInterface;
using ::testing::Invoke;
using ::testing::ReturnNull;
using ::testing::Values;
using ::testing::Combine;

TEST(CCSSettingTest, TestMock)
{
    CCSSetting *setting = ccsMockSettingNew ();
    CCSSettingGMock *mock = (CCSSettingGMock *) ccsObjectGetPrivate (setting);

    EXPECT_CALL (*mock, getName ());
    EXPECT_CALL (*mock, getShortDesc ());
    EXPECT_CALL (*mock, getLongDesc ());
    EXPECT_CALL (*mock, getType ());
    EXPECT_CALL (*mock, getInfo ());
    EXPECT_CALL (*mock, getGroup ());
    EXPECT_CALL (*mock, getSubGroup ());
    EXPECT_CALL (*mock, getHints ());
    EXPECT_CALL (*mock, getDefaultValue ());
    EXPECT_CALL (*mock, getValue ());
    EXPECT_CALL (*mock, getIsDefault ());
    EXPECT_CALL (*mock, getParent ());
    EXPECT_CALL (*mock, getPrivatePtr ());
    EXPECT_CALL (*mock, setPrivatePtr (_));
    EXPECT_CALL (*mock, setInt (_, _));
    EXPECT_CALL (*mock, setFloat (_, _));
    EXPECT_CALL (*mock, setBool (_, _));
    EXPECT_CALL (*mock, setString (_, _));
    EXPECT_CALL (*mock, setColor (_, _));
    EXPECT_CALL (*mock, setMatch (_, _));
    EXPECT_CALL (*mock, setKey (_, _));
    EXPECT_CALL (*mock, setButton (_, _));
    EXPECT_CALL (*mock, setEdge (_, _));
    EXPECT_CALL (*mock, setBell (_, _));
    EXPECT_CALL (*mock, setList (_, _));
    EXPECT_CALL (*mock, setValue (_, _));
    EXPECT_CALL (*mock, getInt (_));
    EXPECT_CALL (*mock, getFloat (_));
    EXPECT_CALL (*mock, getBool (_));
    EXPECT_CALL (*mock, getString (_));
    EXPECT_CALL (*mock, getColor (_));
    EXPECT_CALL (*mock, getMatch (_));
    EXPECT_CALL (*mock, getKey (_));
    EXPECT_CALL (*mock, getButton (_));
    EXPECT_CALL (*mock, getEdge (_));
    EXPECT_CALL (*mock, getBell (_));
    EXPECT_CALL (*mock, getList (_));
    EXPECT_CALL (*mock, resetToDefault (_));
    EXPECT_CALL (*mock, isIntegrated ());
    EXPECT_CALL (*mock, isReadOnly ());
    EXPECT_CALL (*mock, isReadableByBackend ());

    CCSSettingColorValue cv;
    CCSSettingKeyValue kv;
    CCSSettingButtonValue bv;
    CCSSettingValueList lv = NULL;

    ccsSettingGetName (setting);
    ccsSettingGetShortDesc (setting);
    ccsSettingGetLongDesc (setting);
    ccsSettingGetType (setting);
    ccsSettingGetInfo (setting);
    ccsSettingGetGroup (setting);
    ccsSettingGetSubGroup (setting);
    ccsSettingGetHints (setting);
    ccsSettingGetDefaultValue (setting);
    ccsSettingGetValue (setting);
    ccsSettingGetIsDefault (setting);
    ccsSettingGetParent (setting);
    ccsSettingGetPrivatePtr (setting);
    ccsSettingSetPrivatePtr (setting, NULL);
    ccsSetInt (setting, 1, FALSE);
    ccsSetFloat (setting, 1.0, FALSE);
    ccsSetBool (setting, TRUE, FALSE);
    ccsSetString (setting, "foo", FALSE);
    ccsSetColor (setting, cv, FALSE);
    ccsSetMatch (setting, "bar", FALSE);
    ccsSetKey (setting, kv, FALSE);
    ccsSetButton (setting, bv, FALSE);
    ccsSetEdge (setting, 1, FALSE);
    ccsSetBell (setting, TRUE, FALSE);
    ccsSetList (setting, lv, FALSE);
    ccsSetValue (setting, NULL, FALSE);
    ccsGetInt (setting, NULL);
    ccsGetFloat (setting, NULL);
    ccsGetBool (setting, NULL);
    ccsGetString (setting, NULL);
    ccsGetColor (setting, NULL);
    ccsGetMatch (setting, NULL);
    ccsGetKey (setting, NULL);
    ccsGetButton (setting, NULL);
    ccsGetEdge (setting, NULL);
    ccsGetBell (setting, NULL);
    ccsGetList (setting, NULL);
    ccsResetToDefault (setting, FALSE);
    ccsSettingIsIntegrated (setting);
    ccsSettingIsReadOnly (setting);
    ccsSettingIsReadableByBackend (setting);

    ccsSettingUnref (setting);
}

namespace
{

typedef boost::shared_ptr <CCSSettingInfo> CCSSettingInfoPtr;
typedef boost::shared_ptr <CCSSettingValue> CCSSettingValuePtr;
typedef boost::shared_ptr <CCSSetting> CCSSettingPtr;

class MockInitFuncs
{
    public:

	MOCK_METHOD3 (initInfo, void (CCSSettingType, CCSSettingInfo *, void *));
	MOCK_METHOD4 (initDefaultValue, void (CCSSettingType, CCSSettingInfo *, CCSSettingValue *, void *));

	static void wrapInitInfo (CCSSettingType type,
				  CCSSettingInfo *info,
				  void           *data)
	{
	    MockInitFuncs &funcs (*(reinterpret_cast <MockInitFuncs *> (data)));
	    funcs.initInfo (type, info, data);
	}

	static void wrapInitValue (CCSSettingType  type,
				   CCSSettingInfo  *info,
				   CCSSettingValue *value,
				   void            *data)
	{
	    MockInitFuncs &funcs (*(reinterpret_cast <MockInitFuncs *> (data)));
	    funcs.initDefaultValue (type, info, value, data);
	}
};

const std::string SETTING_NAME = "setting_name";
const std::string SETTING_SHORT_DESC = "Short Description";
const std::string SETTING_LONG_DESC = "Long Description";
const std::string SETTING_GROUP = "Group";
const std::string SETTING_SUBGROUP = "Sub Group";
const std::string SETTING_HINTS = "Hints";
const CCSSettingType SETTING_TYPE = TypeInt;

class CCSSettingDefaultImplTest :
    public ::testing::Test
{
    public:

	CCSSettingDefaultImplTest () :
	    plugin (AutoDestroy (ccsMockPluginNew (),
				 ccsPluginUnref)),
	    gmockPlugin (reinterpret_cast <CCSPluginGMock *> (ccsObjectGetPrivate (plugin.get ())))
	{
	}

	virtual void SetUpSetting (MockInitFuncs &funcs)
	{
	    void                 *vFuncs = reinterpret_cast <void *> (&funcs);

	    /* Info must be initialized before the value */
	    InSequence s;

	    EXPECT_CALL (funcs, initInfo (GetSettingType (), _, vFuncs));
	    EXPECT_CALL (funcs, initDefaultValue (GetSettingType (), _, _, vFuncs));

	    setting = AutoDestroy (ccsSettingDefaultImplNew (plugin.get (),
							     SETTING_NAME.c_str (),
							     GetSettingType (),
							     SETTING_SHORT_DESC.c_str (),
							     SETTING_LONG_DESC.c_str (),
							     SETTING_HINTS.c_str (),
							     SETTING_GROUP.c_str (),
							     SETTING_SUBGROUP.c_str (),
							     MockInitFuncs::wrapInitValue,
							     vFuncs,
							     MockInitFuncs::wrapInitInfo,
							     vFuncs,
							     &ccsDefaultObjectAllocator,
							     &ccsDefaultInterfaceTable),
				   ccsSettingUnref);
	}

	virtual void SetUp ()
	{
	    MockInitFuncs funcs;

	    SetUpSetting (funcs);

	}

	virtual CCSSettingType GetSettingType ()
	{
	    return SETTING_TYPE;
	}

	boost::shared_ptr <CCSPlugin>  plugin;
	CCSPluginGMock                 *gmockPlugin;
	boost::shared_ptr <CCSSetting> setting;
};

}

TEST_F (CCSSettingDefaultImplTest, Construction)
{
    EXPECT_EQ (SETTING_TYPE, ccsSettingGetType (setting.get ()));
    EXPECT_EQ (SETTING_NAME, ccsSettingGetName (setting.get ()));
    EXPECT_EQ (SETTING_SHORT_DESC, ccsSettingGetShortDesc (setting.get ()));
    EXPECT_EQ (SETTING_LONG_DESC, ccsSettingGetLongDesc (setting.get ()));
    EXPECT_EQ (SETTING_GROUP, ccsSettingGetGroup (setting.get ()));
    EXPECT_EQ (SETTING_SUBGROUP, ccsSettingGetSubGroup (setting.get ()));
    EXPECT_EQ (SETTING_HINTS, ccsSettingGetHints (setting.get ()));

    /* Pointer Equality */
    EXPECT_EQ (ccsSettingGetDefaultValue (setting.get ()),
	       ccsSettingGetValue (setting.get ()));

    /* Actual Equality */
    EXPECT_TRUE (ccsCheckValueEq (ccsSettingGetDefaultValue (setting.get ()),
				  ccsSettingGetType (setting.get ()),
				  ccsSettingGetInfo (setting.get ()),
				  ccsSettingGetValue (setting.get ()),
				  ccsSettingGetType (setting.get ()),
				  ccsSettingGetInfo (setting.get ())));

    EXPECT_EQ (ccsSettingGetParent (setting.get ()),
	       plugin.get ());
}

namespace
{

/* Testing values */
const int INTEGER_MIN = std::numeric_limits <int>::min () + 1;
const int INTEGER_MAX = std::numeric_limits <int>::max () - 1;
const int INTEGER_VALUE = 5;
const int INTEGER_DEFAULT_VALUE = 2;

const float FLOAT_MIN = std::numeric_limits <float>::min () + 1.0f;
const float FLOAT_MAX = std::numeric_limits <float>::max () - 1.0f;
const float FLOAT_VALUE = 5.0;
const float FLOAT_DEFAULT_VALUE = 2.0;
const char                  *STRING_VALUE = "string_nondefault_value";
const char                  *STRING_DEFAULT_VALUE = "string";
const char                  *MATCH_VALUE = "match_nondefault_value";
const char                  *MATCH_DEFAULT_VALUE = "match";
const Bool                  BOOL_VALUE = TRUE;
const Bool                  BOOL_DEFAULT_VALUE = FALSE;

CCSSettingColorValue
getColorValue (unsigned short r,
	       unsigned short g,
	       unsigned short b,
	       unsigned short a)
{
    CCSSettingColorValue value;

    value.color.red = r;
    value.color.green = g;
    value.color.blue = b;
    value.color.alpha = a;

    return value;
}

CCSSettingKeyValue
getKeyValue (int          keysym,
	     unsigned int modMask)
{
    CCSSettingKeyValue value;

    value.keysym = keysym;
    value.keyModMask = modMask;

    return value;
}

CCSSettingButtonValue
getButtonValue (int          button,
		unsigned int modMask,
		unsigned int edgeMask)
{
    CCSSettingButtonValue value;

    value.button = button;
    value.buttonModMask = modMask;
    value.edgeMask = edgeMask;

    return value;
}

const int          VALUE_KEYSYM = XStringToKeysym ("a");
const int          VALUE_DEFAULT_KEYSYM = XStringToKeysym ("1");
const unsigned int VALUE_MODMASK = ShiftMask | ControlMask;
const unsigned int VALUE_DEFAULT_MODMASK = ControlMask;
const int          VALUE_BUTTON = 1;
const int          VALUE_DEFAULT_BUTTON = 3;
const unsigned int VALUE_EDGEMASK = 1 | 2;
const unsigned int VALUE_DEFAULT_EDGEMASK = 2 | 3;

const CCSSettingColorValue  COLOR_VALUE = getColorValue (255, 255, 255, 255);
const CCSSettingColorValue  COLOR_DEFAULT_VALUE = getColorValue (0, 0, 0, 0);
const CCSSettingKeyValue    KEY_VALUE = getKeyValue (VALUE_KEYSYM, VALUE_MODMASK);
const CCSSettingKeyValue    KEY_DEFAULT_VALUE = getKeyValue (VALUE_DEFAULT_KEYSYM,
							     VALUE_DEFAULT_MODMASK);
const CCSSettingButtonValue BUTTON_VALUE = getButtonValue (VALUE_BUTTON,
							   VALUE_MODMASK,
							   VALUE_EDGEMASK);
const CCSSettingButtonValue BUTTON_DEFAULT_VALUE = getButtonValue (VALUE_DEFAULT_BUTTON,
								   VALUE_DEFAULT_MODMASK,
								   VALUE_DEFAULT_EDGEMASK);
const unsigned int          EDGE_VALUE = 1;
const unsigned int          EDGE_DEFAULT_VALUE = 2;
const Bool                  BELL_VALUE = BOOL_VALUE;
const Bool                  BELL_DEFAULT_VALUE = BOOL_DEFAULT_VALUE;

/* Test CCSSettingInfo */

CCSSettingInfo *
NewCCSSettingInfo ()
{
    return reinterpret_cast <CCSSettingInfo *> (calloc (1, sizeof (CCSSettingInfo)));
}

void
FreeAndCleanupInfo (CCSSettingInfo *info,
		    CCSSettingType type)

{
    ccsCleanupSettingInfo (info, type);
    free (info);
}

CCSSettingInfoPtr
AutoDestroyInfo (CCSSettingInfo *info,
		 CCSSettingType type)
{
    return CCSSettingInfoPtr (info,
			      boost::bind (FreeAndCleanupInfo, info, type));
}

CCSSettingInfo * getGenericInfo (CCSSettingType         type)
{
    return NewCCSSettingInfo ();
}

CCSSettingInfo * getIntInfo ()
{
    CCSSettingInfo *info (getGenericInfo (TypeInt));

    info->forInt.max = INTEGER_MAX;
    info->forInt.min = INTEGER_MIN;

    return info;
}

CCSSettingInfo * getFloatInfo ()
{
    CCSSettingInfo *info (getGenericInfo (TypeFloat));

    info->forFloat.max = FLOAT_MAX;
    info->forFloat.min = FLOAT_MIN;

    return info;
}

CCSSettingInfo * getStringInfo ()
{
    CCSSettingInfo *info (getGenericInfo (TypeString));

    info->forString.restriction = NULL;
    info->forString.sortStartsAt = 1;
    info->forString.extensible = FALSE;

    return info;
}

CCSSettingInfo *
getActionInfo (CCSSettingType actionType)
{
    EXPECT_TRUE ((actionType == TypeAction ||
		  actionType == TypeKey ||
		  actionType == TypeButton ||
		  actionType == TypeEdge ||
		  actionType == TypeBell));

    CCSSettingInfo *info (getGenericInfo (actionType));

    info->forAction.internal = FALSE;

    return info;
}

CCSSettingInfoPtr
getListInfo (CCSSettingType type,
	     CCSSettingInfo *childrenInfo)
{
    CCSSettingInfo *info = getGenericInfo (TypeList);

    info->forList.listType = type;
    info->forList.listInfo = childrenInfo;

    return AutoDestroyInfo (info, TypeList);
}

/* Used to copy different raw values */
template <typename SettingValueType>
class CopyRawValueBase
{
    public:

	CopyRawValueBase (const SettingValueType &value) :
	    mValue (value)
	{
	}

    protected:

	const SettingValueType &mValue;
};

template <typename SettingValueType>
class CopyRawValue :
    public CopyRawValueBase <SettingValueType>
{
    public:

	typedef SettingValueType ReturnType;
	typedef CopyRawValueBase <SettingValueType> Parent;

	CopyRawValue (const SettingValueType &value) :
	    CopyRawValueBase <SettingValueType> (value)
	{
	}

	SettingValueType operator () ()
	{
	    return Parent::mValue;
	}
};

template <>
class CopyRawValue <const char *> :
    public CopyRawValueBase <const char *>
{
    public:

	typedef const char * ReturnType;
	typedef CopyRawValueBase <const char *> Parent;

	CopyRawValue (const char * value) :
	    CopyRawValueBase <const char *> (ptr),
	    ptr (value)
	{
	}

	ReturnType operator () ()
	{
	    /* Passing an illegal value is okay */
	    if (Parent::mValue)
		return strdup (Parent::mValue);
	    else
		return NULL;
	}

    private:
	// mValue is a reference so it needs a persistent variable to point at
	const char *ptr;
};

template <>
class CopyRawValue <cci::SettingValueListWrapper::Ptr> :
    public CopyRawValueBase <cci::SettingValueListWrapper::Ptr>
{
    public:

	typedef CCSSettingValueList ReturnType;
	typedef CopyRawValueBase <cci::SettingValueListWrapper::Ptr> Parent;

	CopyRawValue (const cci::SettingValueListWrapper::Ptr &value) :
	    CopyRawValueBase <cci::SettingValueListWrapper::Ptr> (value)
	{
	}

	ReturnType operator () ()
	{
	    if (Parent::mValue)
		return ccsCopyList (*Parent::mValue,
				    Parent::mValue->setting ().get ());
	    else
		return NULL;
	}
};

CCSSettingValue *
NewCCSSettingValue ()
{
    CCSSettingValue *value =
	    reinterpret_cast <CCSSettingValue *> (
		calloc (1, sizeof (CCSSettingValue)));

    value->refCount = 1;

    return value;
}

template <typename SettingValueType>
CCSSettingValue *
RawValueToCCSValue (const SettingValueType &value)
{
    typedef typename CopyRawValue <SettingValueType>::ReturnType UnionType;

    CCSSettingValue  *settingValue = NewCCSSettingValue ();
    UnionType *unionMember =
	    reinterpret_cast <UnionType *> (&settingValue->value);

    *unionMember = (CopyRawValue <SettingValueType> (value)) ();

    return settingValue;
}

class ContainedValueGenerator
{
    private:

	const CCSSettingValuePtr &
	InitValue (const CCSSettingValuePtr &value,
		   CCSSettingType           type,
		   const CCSSettingInfoPtr  &info)
	{
	    const CCSSettingPtr &setting (GetSetting (type, info));
	    value->parent = setting.get ();
	    mValues.push_back (value);

	    return mValues.back ();
	}

    public:

	template <typename SettingValueType>
	const CCSSettingValuePtr &
	SpawnValue (const SettingValueType  &rawValue,
		    CCSSettingType          type,
		    const CCSSettingInfoPtr &info)
	{

	    CCSSettingValuePtr  value (AutoDestroy (RawValueToCCSValue <SettingValueType> (rawValue),
						    ccsSettingValueUnref));

	    return InitValue (value, type, info);
	}

	const CCSSettingPtr &
	GetSetting (CCSSettingType          type,
		    const CCSSettingInfoPtr &info)
	{
	    if (!mSetting)
		SetupMockSetting (type, info);

	    return mSetting;
	}

    private:

	void SetupMockSetting (CCSSettingType          type,
			       const CCSSettingInfoPtr &info)
	{
	    mSetting = AutoDestroy (ccsMockSettingNew (),
				    ccsSettingUnref);

	    CCSSettingGMock *settingMock =
		    reinterpret_cast <CCSSettingGMock *> (
			ccsObjectGetPrivate (mSetting.get ()));

	    EXPECT_CALL (*settingMock, getType ())
		.WillRepeatedly (Return (type));

	    EXPECT_CALL (*settingMock, getInfo ())
		.WillRepeatedly (Return (info.get ()));

	    EXPECT_CALL (*settingMock, getDefaultValue ())
		.WillRepeatedly (ReturnNull ());
	}

	/* This must always be before the value
	 * as the values hold a weak reference to
	 * it */
	CCSSettingPtr                    mSetting;
	std::vector <CCSSettingValuePtr> mValues;

};

template <typename SettingValueType>
class ValueContainer
{
    public:

	virtual ~ValueContainer () {}
	typedef boost::shared_ptr <ValueContainer> Ptr;

	virtual const SettingValueType &
	getRawValue (CCSSettingType          type,
		     const CCSSettingInfoPtr &info) = 0;
	virtual const CCSSettingValuePtr &
	getContainedValue (CCSSettingType          type,
			   const CCSSettingInfoPtr &info) = 0;
};

class NormalValueContainerBase
{
    protected:

	ContainedValueGenerator  mGenerator;
	CCSSettingValuePtr       mValue;
};

template <typename SettingValueType>
class NormalValueContainer :
    public NormalValueContainerBase,
    public ValueContainer <SettingValueType>
{
    public:

	NormalValueContainer (const SettingValueType &value) :
	    mRawValue (value)
	{
	}

	const SettingValueType &
	getRawValue (CCSSettingType          type,
		     const CCSSettingInfoPtr &info)
	{
	    return mRawValue;
	}

	const CCSSettingValuePtr &
	getContainedValue (CCSSettingType          type,
			   const CCSSettingInfoPtr &info)
	{
	    if (!mValue)
		mValue = mGenerator.SpawnValue (mRawValue,
						type,
						info);

	    return mValue;
	}

    private:

	const SettingValueType   &mRawValue;
};

template <typename SettingValueType>
typename NormalValueContainer <SettingValueType>::Ptr
ContainNormal (const SettingValueType &value)
{
    return boost::make_shared <NormalValueContainer <SettingValueType> > (value);
}

class ListValueContainerBase :
    public ValueContainer <CCSSettingValueList>
{
	public:

	virtual ~ListValueContainerBase () {}

    protected:

	const CCSSettingValuePtr &
	getContainedValue (CCSSettingType          type,
			   const CCSSettingInfoPtr &info)
	{
	    if (!mContainedWrapper)
	    {
		const cci::SettingValueListWrapper::Ptr &wrapper (SetupWrapper (type, info));

		mContainedWrapper =
			mContainedValueGenerator.SpawnValue (wrapper,
							     type,
							     info);
	    }

	    return mContainedWrapper;
	}

	const CCSSettingValueList &
	getRawValue (CCSSettingType          type,
		     const CCSSettingInfoPtr &info)
	{
	    const cci::SettingValueListWrapper::Ptr &wrapper (SetupWrapper (type, info));

	    return *wrapper;
	}

	cci::SettingValueListWrapper::Ptr mWrapper;

	/* ccsFreeSettingValue has an implicit
	 * dependency on mWrapper (CCSSettingValue -> CCSSetting ->
	 * CCSSettingInfo -> cci::SettingValueListWrapper), these should
	 * be kept after mWrapper here */
	ContainedValueGenerator           mContainedValueGenerator;
	CCSSettingValuePtr                mContainedWrapper;

    private:

	virtual const cci::SettingValueListWrapper::Ptr &
	SetupWrapper (CCSSettingType          type,
		      const CCSSettingInfoPtr &info) = 0;
};

class ListValueContainerFromChildValueBase :
    public ListValueContainerBase
{
    private:

	virtual const cci::SettingValueListWrapper::Ptr &
	SetupWrapper (CCSSettingType          type,
		      const CCSSettingInfoPtr &info)
	{
	    if (!mWrapper)
	    {
		const CCSSettingPtr &setting (mContainedValueGenerator.GetSetting (type, info));
		CCSSettingValue     *value = GetValueForListWrapper ();

		value->parent = setting.get ();
		value->isListChild = TRUE;
		mWrapper.reset (new cci::SettingValueListWrapper (NULL,
								  cci::Deep,
								  type,
								  setting));
		mWrapper->append (value);
	    }

	    return mWrapper;
	}

	virtual CCSSettingValue * GetValueForListWrapper () = 0;
};

class ListValueContainerFromList :
    public ListValueContainerBase
{
    public:

	ListValueContainerFromList (CCSSettingValueList rawValueList) :
	    mRawValueList (rawValueList)
	{
	}

    private:

	const cci::SettingValueListWrapper::Ptr &
	SetupWrapper (CCSSettingType          type,
		      const CCSSettingInfoPtr &info)
	{
	    if (!mWrapper)
	    {
		const CCSSettingPtr &setting (mContainedValueGenerator.GetSetting (type, info));
		mWrapper.reset (new cci::SettingValueListWrapper (ccsCopyList (mRawValueList, setting.get ()),
								  cci::Deep,
								  type,
								  setting));
	    }

	    return mWrapper;
	}

	CCSSettingValueList mRawValueList;
};

template <typename SettingValueType>
class ChildValueListValueContainer :
    public ListValueContainerFromChildValueBase
{
    public:

	ChildValueListValueContainer (const SettingValueType &value) :
	    mRawChildValue (value)
	{
	}
	virtual ~ChildValueListValueContainer () {}

    private:

	CCSSettingValue * GetValueForListWrapper ()
	{
	    return RawValueToCCSValue (mRawChildValue);
	}

	const SettingValueType  &mRawChildValue;
};

template <typename SettingValueType>
typename ValueContainer <CCSSettingValueList>::Ptr
ContainList (const SettingValueType &value)
{
    return boost::make_shared <ChildValueListValueContainer <SettingValueType> > (value);
}

typename ValueContainer <CCSSettingValueList>::Ptr
ContainPrexistingList (const CCSSettingValueList &value)
{
    return boost::make_shared <ListValueContainerFromList> (value);
}

template <typename SettingValueType>
struct SettingMutators
{
    typedef CCSSetStatus (*SetFunction) (CCSSetting *setting,
					 SettingValueType data,
					 Bool);
    typedef Bool (*GetFunction) (CCSSetting *setting,
				 SettingValueType *);
};

typedef enum _SetMethod
{
    ThroughRaw,
    ThroughValue
} SetMethod;

template <typename SettingValueType>
CCSSetStatus performRawSet (const SettingValueType                                  &rawValue,
			    const CCSSettingPtr                                     &setting,
			    typename SettingMutators<SettingValueType>::SetFunction setFunction)
{
    return (*setFunction) (setting.get (), rawValue, FALSE);
}

template <typename SettingValueType>
class RawValueContainmentPacker
{
    public:

	RawValueContainmentPacker (const SettingValueType &value) :
	    mValue (value)
	{
	}

	typename ValueContainer <SettingValueType>::Ptr
	operator () ()
	{
	    return ContainNormal (mValue);
	}

    private:

	const SettingValueType &mValue;
};

template <>
class RawValueContainmentPacker <CCSSettingValueList>
{
    public:

	RawValueContainmentPacker (const CCSSettingValueList &value) :
	    mValue (value)
	{
	}

	typename ValueContainer <CCSSettingValueList>::Ptr
	operator () ()
	{
	    return ContainPrexistingList (mValue);
	}

    private:

	const CCSSettingValueList &mValue;
};

template <typename SettingValueType>
typename ValueContainer <SettingValueType>::Ptr
ContainRawValue (const SettingValueType &value)
{
    return RawValueContainmentPacker <SettingValueType> (value) ();
}

template <typename SettingValueType>
CCSSetStatus performValueSet (const SettingValueType  &rawValue,
			      const CCSSettingInfoPtr &info,
			      CCSSettingType          type,
			      const CCSSettingPtr     &setting)
{
    typename ValueContainer <SettingValueType>::Ptr container (ContainRawValue (rawValue));
    const CCSSettingValuePtr                        &value (container->getContainedValue (type,
											  info));

    return ccsSetValue (setting.get (), value.get (), FALSE);
}

template <typename SettingValueType>
CCSSetStatus performSet (const SettingValueType                                  &rawValue,
			 const CCSSettingPtr                                     &setting,
			 const CCSSettingInfoPtr                                 &info,
			 CCSSettingType                                          type,
			 typename SettingMutators<SettingValueType>::SetFunction setFunction,
			 SetMethod                                               method)
{
    /* XXX:
     * This is really bad design because it effectively involves runtime
     * switching on types. Unfortunately, there doesn't seem to be a better
     * way to do this that's not hugely verbose - injecting the method
     * as a class or a function would mean that we'd have to expose
     * template parameters to areas where we can't do that because we
     * want the tests to run once for ccsSetValue and once for
     * ccsSet* . If we did that, we'd have to either write the tests
     * twice, or write the INSTANTIATE_TEST_CASE_P sections twice and
     * both are 200+ lines of copy-and-paste as opposed to this type-switch
     * here
     */
    switch (method)
    {
	case ThroughRaw:
	    return performRawSet (rawValue, setting, setFunction);
	    break;
	case ThroughValue:
	    return performValueSet (rawValue, info, type, setting);
	    break;
	default:
	    throw std::runtime_error ("called perfomSet with unknown SetMethod");
    }

    throw std::runtime_error ("Unreachable");
}


class SetParam
{
    public:

	typedef boost::shared_ptr <SetParam> Ptr;
	typedef boost::function <void (MockInitFuncs &funcs)> SetUpSettingFunc;

	virtual ~SetParam () {};

	virtual void SetUpSetting (const SetUpSettingFunc &func) = 0;
	virtual void TearDownSetting () = 0;
	virtual CCSSettingType GetSettingType () = 0;
	virtual void         SetUpParam (const CCSSettingPtr &) = 0;
	virtual CCSSetStatus setWithInvalidType (SetMethod) = 0;
	virtual CCSSetStatus setToFailValue (SetMethod) = 0;
	virtual CCSSetStatus setToNonDefaultValue (SetMethod) = 0;
	virtual CCSSetStatus setToDefaultValue (SetMethod) = 0;
};

void stubInitInfo (CCSSettingType type,
		   CCSSettingInfo *dst,
		   void           *data)
{
    CCSSettingInfo *src = reinterpret_cast <CCSSettingInfo *> (data);

    ccsCopyInfo (src, dst, type);
}

void stubInitDefaultValue (CCSSettingType  type,
			   CCSSettingInfo  *info,
			   CCSSettingValue *dest,
			   void           *data)
{
    CCSSettingValue *src = reinterpret_cast <CCSSettingValue *> (data);
    CCSSetting      *oldDestParent = src->parent;

    /* Change the parent to this setting that's being initialized
     * as that needs to go into the setting's default value as
     * the parent entry */
    src->parent = dest->parent;
    ccsCopyValueInto (src, dest, type, info);

    /* Restore the old parent */
    src->parent = oldDestParent;
}

class StubInitFuncs :
    public MockInitFuncs
{
    public:

	StubInitFuncs (CCSSettingInfo  *info,
		       CCSSettingValue *value) :
	    MockInitFuncs (),
	    mInfo (info),
	    mValue (value)
	{
	    ON_CALL (*this, initInfo (_, _, _))
		    .WillByDefault (WithArgs <0, 1> (
					Invoke (this,
						&StubInitFuncs::initInfo)));

	    ON_CALL (*this, initDefaultValue (_, _, _, _))
		    .WillByDefault (WithArgs <0, 1, 2> (
					Invoke (this,
						&StubInitFuncs::initializeValue)));
	}

	void initInfo (CCSSettingType type,
		       CCSSettingInfo *info)
	{
	    stubInitInfo (type, info, reinterpret_cast <void *> (mInfo));
	}

	void initializeValue (CCSSettingType  type,
			      CCSSettingInfo  *info,
			      CCSSettingValue *value)
	{
	    stubInitDefaultValue (type, info, value,
				  reinterpret_cast <void *> (mValue));
	}

	CCSSettingInfo  *mInfo;
	CCSSettingValue *mValue;
};

class InternalSetParam :
    public SetParam
{
    protected:

	InternalSetParam (const CCSSettingInfoPtr &info,
				 CCSSettingType          type) :
	    mInfo (info),
	    mType (type)
	{
	}

	virtual void TearDownSetting ()
	{
	}

	void InitDefaultsForSetting (const SetUpSettingFunc &func)
	{
	    StubInitFuncs stubInitializers (mInfo.get (), mValue.get ());

	    func (stubInitializers);
	}

	void TakeReferenceToCreatedSetting (const CCSSettingPtr &setting)
	{
	    mSetting = setting;
	}

	const CCSSettingInterface * RedirectSettingInterface ()
	{
	    const CCSSettingInterface *settingInterface =
		GET_INTERFACE (CCSSettingInterface, mSetting.get ());
	    CCSSettingInterface *tmpSettingInterface =
		    new CCSSettingInterface;
	    *tmpSettingInterface = *settingInterface;

	    tmpSettingInterface->settingGetType =
		InternalSetParam::returnIncorrectSettingType;

	    ccsObjectRemoveInterface (mSetting.get (),
				      GET_INTERFACE_TYPE (CCSSettingInterface));
	    ccsObjectAddInterface (mSetting.get (),
				   (const CCSInterface *) tmpSettingInterface,
				   GET_INTERFACE_TYPE (CCSSettingInterface));

	    return settingInterface;
	}

	void RestoreSettingInterface (const CCSSettingInterface *settingInterface)
	{
	    /* Restore the old interface */
	    const CCSSettingInterface *oldSettingInterface =
		GET_INTERFACE (CCSSettingInterface, mSetting.get ());
	    ccsObjectRemoveInterface (mSetting.get (),
				      GET_INTERFACE_TYPE (CCSSettingInterface));
	    delete oldSettingInterface;
	    ccsObjectAddInterface (mSetting.get (),
				   (const CCSInterface *) settingInterface,
				   GET_INTERFACE_TYPE (CCSSettingInterface));
	}

	virtual CCSSetStatus setToFailValue (SetMethod method)
	{
	    return SetFailed;
	}

	virtual CCSSettingType GetSettingType ()
	{
	    return mType;
	}

    protected:

	CCSSettingInfoPtr  mInfo;
	CCSSettingValuePtr mValue;
	CCSSettingType     mType;
	CCSSettingPtr      mSetting;

	CCSSettingInterface tmpSettingInterface;

    private:

	static const CCSSettingType incorrectSettingType = TypeNum;
	static CCSSettingType returnIncorrectSettingType (CCSSetting *setting)
	{
	    return incorrectSettingType;
	}
};

template <typename SettingValueType>
class SetParamContainerStorage
{
    protected:

	typedef typename ValueContainer <SettingValueType>::Ptr ValueContainerPtr;

	SetParamContainerStorage (const ValueContainerPtr &defaultValue,
				  const ValueContainerPtr &nonDefaultValue) :
	    mDefault (defaultValue),
	    mNonDefault (nonDefaultValue)
	{
	}

	ValueContainerPtr  mDefault;
	ValueContainerPtr  mNonDefault;
};

template <typename SettingValueType>
class TypedSetParam :
    /* Do not change the order of inheritance here, DefaultImplSetParamTemplatedBase
     * must be destroyed after DefaultImplSetParamBase as DefaultImplSetParamBase
     * has indirect weak references to variables in DefaultImplSetParamTemplatedBase
     */
   private SetParamContainerStorage <SettingValueType>,
   public  InternalSetParam
{
    public:

	typedef typename SettingMutators <SettingValueType>::SetFunction SetFunction;
	typedef typename SettingMutators <SettingValueType>::GetFunction GetFunction;
	typedef typename ValueContainer <SettingValueType>::Ptr ValueContainerPtr;
	typedef SetParamContainerStorage <SettingValueType> TemplateParent;

	TypedSetParam (const ValueContainerPtr &defaultValue,
		       CCSSettingType          type,
		       SetFunction             setFunction,
		       GetFunction             getFunction,
		       const CCSSettingInfoPtr &info,
		       const ValueContainerPtr &nonDefaultValue) :
	    SetParamContainerStorage <SettingValueType> (defaultValue,
								 nonDefaultValue),
	    InternalSetParam (info, type),
	    mSetFunction (setFunction),
	    mGetFunction (getFunction)
	{
	}

	virtual void SetUpSetting (const SetUpSettingFunc &func)
	{
	    /* Do delayed setup here */
	    mValue = TemplateParent::mDefault->getContainedValue (mType, mInfo);
	    mNonDefaultValue = TemplateParent::mNonDefault->getRawValue (mType, mInfo);

	    InitDefaultsForSetting (func);
	}

	virtual void SetUpParam (const CCSSettingPtr &setting)
	{
	    ASSERT_TRUE ((*mGetFunction) (setting.get (), &mDefaultValue));

	    TakeReferenceToCreatedSetting (setting);
	}

	virtual CCSSetStatus setWithInvalidType (SetMethod method)
	{
	    /* Temporarily redirect the setting interface to
	     * our own with an overloaded settingGetType function */
	    const CCSSettingInterface *iface = RedirectSettingInterface ();
	    CCSSetStatus ret = performSet (mNonDefaultValue, mSetting, mInfo, mType, mSetFunction, method);
	    RestoreSettingInterface (iface);

	    return ret;
	}

	virtual CCSSetStatus setToNonDefaultValue (SetMethod method)
	{
	    return performSet (mNonDefaultValue, mSetting, mInfo, mType, mSetFunction, method);
	}

	virtual CCSSetStatus setToDefaultValue (SetMethod method)
	{
	    return performSet (mDefaultValue, mSetting, mInfo, mType, mSetFunction, method);
	}

    private:

	SettingValueType   mDefaultValue;
	SettingValueType   mNonDefaultValue;

    protected:

	SetFunction        mSetFunction;
	GetFunction        mGetFunction;

};

class SetWithDisallowedValueBase
{
    protected:

	SetWithDisallowedValueBase (const CCSSettingPtr     &setting,
				    const CCSSettingInfoPtr &info,
				    CCSSettingType          type) :
	    mSetting (setting),
	    mInfo (info),
	    mType (type)
	{
	}

	CCSSettingPtr     mSetting;
	CCSSettingInfoPtr mInfo;
	CCSSettingType    mType;
};

template <typename SettingValueType>
class SetWithDisallowedValueTemplatedBase :
    public SetWithDisallowedValueBase
{
    protected:

	typedef typename SettingMutators <SettingValueType>::SetFunction SetFunction;

	SetWithDisallowedValueTemplatedBase (SetFunction             setFunction,
					     const CCSSettingPtr     &setting,
					     const CCSSettingInfoPtr &info,
					     CCSSettingType          type) :
	    SetWithDisallowedValueBase (setting, info, type),
	    mSetFunction (setFunction)
	{
	}

	SetFunction       mSetFunction;
};

template <typename SettingValueType>
class SetWithDisallowedValue :
    public SetWithDisallowedValueTemplatedBase <SettingValueType>
{
    public:

	typedef typename SettingMutators <SettingValueType>::SetFunction SetFunction;

	SetWithDisallowedValue (SetFunction             setFunction,
				const CCSSettingPtr     &setting,
				const CCSSettingInfoPtr &info,
				CCSSettingType          type) :
	    SetWithDisallowedValueTemplatedBase <SettingValueType> (setFunction, setting, info, type)
	{
	}

	CCSSetStatus operator () (SetMethod method)
	{
	    return SetFailed;
	}
};

template <>
class SetWithDisallowedValue <int> :
    public SetWithDisallowedValueTemplatedBase <int>
{
    public:

	typedef typename SettingMutators <int>::SetFunction SetFunction;
	typedef SetWithDisallowedValueTemplatedBase <int> Parent;

	SetWithDisallowedValue (SetFunction             setFunction,
				const CCSSettingPtr     &setting,
				const CCSSettingInfoPtr &info,
				CCSSettingType          type) :
	    SetWithDisallowedValueTemplatedBase <int> (setFunction, setting, info, type)
	{
	}

	CCSSetStatus operator () (SetMethod method)
	{
	    return performSet <int> (Parent::mInfo->forInt.min - 1,
				     Parent::mSetting,
				     Parent::mInfo,
				     Parent::mType,
				     Parent::mSetFunction,
				     method);
	}
};

template <>
class SetWithDisallowedValue <float> :
    public SetWithDisallowedValueTemplatedBase <float>
{
    public:

	typedef typename SettingMutators <float>::SetFunction SetFunction;
	typedef SetWithDisallowedValueTemplatedBase <float> Parent;

	SetWithDisallowedValue (SetFunction             setFunction,
				const CCSSettingPtr     &setting,
				const CCSSettingInfoPtr &info,
				CCSSettingType          type) :
	    SetWithDisallowedValueTemplatedBase <float> (setFunction, setting, info, type)
	{
	}

	CCSSetStatus operator () (SetMethod method)
	{
	    return performSet <float> (Parent::mInfo->forFloat.min - 1.0f,
				       Parent::mSetting,
				       Parent::mInfo,
				       Parent::mType,
				       Parent::mSetFunction,
				       method);
	}
};

template <>
class SetWithDisallowedValue <const char *> :
    public SetWithDisallowedValueTemplatedBase <const char *>
{
    public:

	typedef typename SettingMutators <const char *>::SetFunction SetFunction;
	typedef SetWithDisallowedValueTemplatedBase <const char *> Parent;

	SetWithDisallowedValue (SetFunction             setFunction,
				const CCSSettingPtr     &setting,
				const CCSSettingInfoPtr &info,
				CCSSettingType          type) :
	    SetWithDisallowedValueTemplatedBase <const char *> (setFunction, setting, info, type)
	{
	}

	CCSSetStatus operator () (SetMethod method)
	{
	    return performSet <const char *> (NULL,
					      Parent::mSetting,
					      Parent::mInfo,
					      Parent::mType,
					      Parent::mSetFunction,
					      method);
	}
};

template <typename SettingValueType>
class SetFailureParam :
    public TypedSetParam <SettingValueType>
{
    public:

	typedef TypedSetParam <SettingValueType> Parent;
	typedef typename TypedSetParam <SettingValueType>::SetFunction SetFunction;
	typedef typename TypedSetParam <SettingValueType>::GetFunction GetFunction;
	typedef typename TypedSetParam <SettingValueType>::ValueContainerPtr ValueContainerPtr;

	SetFailureParam (const ValueContainerPtr &defaultValue,
			 CCSSettingType          type,
			 SetFunction             setFunction,
			 GetFunction             getFunction,
			 const CCSSettingInfoPtr &info,
			 const ValueContainerPtr &nonDefault) :
	    TypedSetParam <SettingValueType> (defaultValue,
					      type,
					      setFunction,
					      getFunction,
					      info,
					      nonDefault)
	{
	}
	virtual ~SetFailureParam () {}

	virtual CCSSetStatus setToFailValue (SetMethod method)
	{
	    typedef TypedSetParam <SettingValueType> Parent;
	    return SetWithDisallowedValue <SettingValueType> (Parent::mSetFunction,
							      Parent::mSetting,
							      Parent::mInfo,
							      Parent::mType) (method);
	}
};

template <typename T>
SetParam::Ptr
SParam (const typename ValueContainer <T>::Ptr   &defaultValue,
	CCSSettingType                           type,
	typename SettingMutators<T>::SetFunction setFunc,
	typename SettingMutators<T>::GetFunction getFunc,
	const CCSSettingInfoPtr                  &settingInfo,
	const typename ValueContainer <T>::Ptr   &changeTo)
{
    return boost::make_shared <TypedSetParam <T> > (defaultValue,
						    type,
						    setFunc,
						    getFunc,
						    settingInfo,
						    changeTo);
}

template <typename T>
SetParam::Ptr
FailSParam (const typename ValueContainer <T>::Ptr   &defaultValue,
	    CCSSettingType			   type,
	    typename SettingMutators<T>::SetFunction setFunc,
	    typename SettingMutators<T>::GetFunction getFunc,
	    const CCSSettingInfoPtr                  &settingInfo,
	    const typename ValueContainer <T>::Ptr   &changeTo)
{
    return boost::make_shared <SetFailureParam <T> > (defaultValue,
						      type,
						      setFunc,
						      getFunc,
						      settingInfo,
						      changeTo);
}

typedef std::tr1::tuple <SetParam::Ptr,
			 SetMethod> SettingDefaultImplSetParamType;

class SettingDefaultImplSet :
    public CCSSettingDefaultImplTest,
    public WithParamInterface <SettingDefaultImplSetParamType>
{
    public:

	SettingDefaultImplSet () :
	    setHarness (std::tr1::get <0> (GetParam ())),
	    setMethod (std::tr1::get <1> (GetParam ()))
	{
	}

	virtual void SetUp ()
	{
	    setHarness->SetUpSetting (boost::bind (&CCSSettingDefaultImplTest::SetUpSetting, this, _1));
	    setHarness->SetUpParam (setting);
	}

	virtual void TearDown ()
	{
	    setHarness->TearDownSetting ();
	}

	CCSSettingType GetSettingType ()
	{
	    return setHarness->GetSettingType ();
	}

    protected:

	SetParam::Ptr setHarness;
	SetMethod     setMethod;
};

class SettingDefaulImplSetFailure :
    public SettingDefaultImplSet
{
};

}

/* Tests */

TEST_P (SettingDefaultImplSet, Construction)
{
}

TEST_P (SettingDefaultImplSet, WithInvalidType)
{
    EXPECT_EQ (SetFailed, setHarness->setWithInvalidType (setMethod));
}

TEST_P (SettingDefaultImplSet, ToNewValue)
{
    EXPECT_EQ (SetToNewValue, setHarness->setToNonDefaultValue (setMethod));
}

TEST_P (SettingDefaultImplSet, ToSameValue)
{
    EXPECT_EQ (SetToNewValue, setHarness->setToNonDefaultValue (setMethod));
    EXPECT_EQ (SetToSameValue, setHarness->setToNonDefaultValue (setMethod));
}

TEST_P (SettingDefaultImplSet, ToDefaultValue)
{
    EXPECT_EQ (SetToNewValue, setHarness->setToNonDefaultValue (setMethod));
    EXPECT_EQ (SetToDefault, setHarness->setToDefaultValue (setMethod));
}

TEST_P (SettingDefaultImplSet, IsDefaultValue)
{
    EXPECT_EQ (SetToNewValue, setHarness->setToNonDefaultValue (setMethod));
    EXPECT_EQ (SetToDefault, setHarness->setToDefaultValue (setMethod));
    EXPECT_EQ (SetIsDefault, setHarness->setToDefaultValue (setMethod));
}

TEST_P (SettingDefaulImplSetFailure, ToFailValue)
{
    EXPECT_EQ (SetFailed, setHarness->setToFailValue (setMethod));
}

#define VALUE_TEST INSTANTIATE_TEST_CASE_P

VALUE_TEST (SetSemantics, SettingDefaulImplSetFailure, Combine (
	    Values (FailSParam <int> (ContainNormal (INTEGER_DEFAULT_VALUE),
						     TypeInt,
						     ccsSetInt,
						     ccsGetInt,
						     AutoDestroyInfo (getIntInfo (),
								      TypeInt),
						     ContainNormal (INTEGER_VALUE)),
		    FailSParam <float> (ContainNormal (FLOAT_DEFAULT_VALUE),
						       TypeFloat,
						       ccsSetFloat,
						       ccsGetFloat,
						       AutoDestroyInfo (getFloatInfo (),
									TypeFloat),
						       ContainNormal (FLOAT_VALUE)),
		    FailSParam <const char *> (ContainNormal (STRING_DEFAULT_VALUE),
							      TypeString,
							      ccsSetString,
							      ccsGetString,
							      AutoDestroyInfo (getGenericInfo (TypeString),
									       TypeMatch),
							      ContainNormal (STRING_VALUE)),
		    FailSParam <const char *> (ContainNormal (MATCH_DEFAULT_VALUE),
							      TypeMatch,
							      ccsSetMatch,
							      ccsGetMatch,
							      AutoDestroyInfo (getGenericInfo (TypeMatch),
									       TypeMatch),
							      ContainNormal (MATCH_VALUE))),
		Values (ThroughRaw, ThroughValue)));

VALUE_TEST (SetSemantics, SettingDefaultImplSet, Combine (
	    Values (SParam <int> (ContainNormal (INTEGER_DEFAULT_VALUE),
				  TypeInt,
				  ccsSetInt,
				  ccsGetInt,
				  AutoDestroyInfo (getIntInfo (),
						   TypeInt),
				  ContainNormal (INTEGER_VALUE)),
		     SParam <float> (ContainNormal (FLOAT_DEFAULT_VALUE),
				     TypeFloat,
				     ccsSetFloat,
				     ccsGetFloat,
				     AutoDestroyInfo (getFloatInfo (),
						      TypeFloat),
				     ContainNormal  (FLOAT_VALUE)),
		     SParam <Bool> (ContainNormal  (BOOL_DEFAULT_VALUE),
				    TypeBool,
				    ccsSetBool,
				    ccsGetBool,
				    AutoDestroyInfo (getGenericInfo (TypeBool),
						     TypeBool),
				    ContainNormal (BOOL_VALUE)),
		     SParam <const char *> (ContainNormal (STRING_DEFAULT_VALUE),
					    TypeString,
					    ccsSetString,
					    ccsGetString,
					    AutoDestroyInfo (getStringInfo (),
							     TypeBool),
					   ContainNormal (STRING_VALUE)),
		     SParam <const char *> (ContainNormal (MATCH_DEFAULT_VALUE),
					    TypeMatch,
					    ccsSetMatch,
					    ccsGetMatch,
					    AutoDestroyInfo (getGenericInfo (TypeMatch),
							     TypeMatch),
					    ContainNormal (MATCH_VALUE)),
		     SParam <CCSSettingColorValue> (ContainNormal (COLOR_DEFAULT_VALUE),
						    TypeColor,
						    ccsSetColor,
						    ccsGetColor,
						    AutoDestroyInfo (getGenericInfo (TypeColor),
								     TypeColor),
						    ContainNormal (COLOR_VALUE)),
		     SParam <CCSSettingKeyValue> (ContainNormal (KEY_DEFAULT_VALUE),
						  TypeKey,
						  ccsSetKey,
						  ccsGetKey,
						  AutoDestroyInfo (getActionInfo (TypeKey),
								   TypeKey),
						  ContainNormal (KEY_VALUE)),
		     SParam <CCSSettingButtonValue> (ContainNormal (BUTTON_DEFAULT_VALUE),
						     TypeButton,
						     ccsSetButton,
						     ccsGetButton,
						     AutoDestroyInfo (getActionInfo (TypeButton),
								      TypeButton),
						     ContainNormal (BUTTON_VALUE)),
		     SParam <unsigned int> (ContainNormal (EDGE_DEFAULT_VALUE),
					    TypeEdge,
					    ccsSetEdge,
					    ccsGetEdge,
					    AutoDestroyInfo (getActionInfo (TypeEdge),
							     TypeEdge),
					    ContainNormal (EDGE_VALUE)),
		     SParam <Bool> (ContainNormal  (BELL_DEFAULT_VALUE),
						    TypeBell,
						    ccsSetBell,
						    ccsGetBell,
						    AutoDestroyInfo (getGenericInfo (TypeBell),
								     TypeBell),
						    ContainNormal (BELL_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (INTEGER_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeInt,
								getIntInfo ()),
						   ContainList (INTEGER_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (FLOAT_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeFloat,
								getFloatInfo ()),
						   ContainList (FLOAT_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (BOOL_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeBool,
								getGenericInfo (TypeBool)),
						   ContainList (BOOL_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (STRING_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeString,
								getGenericInfo (TypeMatch)),
						   ContainList (STRING_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (MATCH_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeMatch,
								getGenericInfo (TypeMatch)),
						   ContainList (MATCH_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (COLOR_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeColor,
								getGenericInfo (TypeColor)),
						   ContainList (COLOR_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (KEY_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeKey,
								getActionInfo (TypeKey)),
						   ContainList (KEY_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (BUTTON_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeButton,
								getActionInfo (TypeButton)),
						   ContainList (BUTTON_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (EDGE_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeEdge,
								getIntInfo ()),
						   ContainList (EDGE_VALUE)),
		     SParam <CCSSettingValueList> (ContainList (BELL_DEFAULT_VALUE),
						   TypeList,
						   ccsSetList,
						   ccsGetList,
						   getListInfo (TypeBell,
								getActionInfo (TypeBell)),
						   ContainList (BELL_VALUE))),
		Values (ThroughRaw, ThroughValue)));
