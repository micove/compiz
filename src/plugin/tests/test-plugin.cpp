#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#include "core/plugin.h"

// Get rid of stupid macro from X.h
// Why, oh why, are we including X.h?
#undef None

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <gtest_shared_tmpenv.h>
#include <gtest_shared_autodestroy.h>

class CompMatch
{
};

namespace
{

class PluginFilesystem
{
    public:

	virtual bool
	LoadPlugin(CompPlugin *p, const char *path, const char *name) const = 0;

	virtual void
	UnloadPlugin(CompPlugin *p) const = 0;

	static PluginFilesystem const* instance;

    protected:

	PluginFilesystem();
	virtual ~PluginFilesystem() {}
};

class MockPluginFilesystem : public PluginFilesystem
{
    public:

	MOCK_CONST_METHOD3(LoadPlugin, bool (CompPlugin *, const char *, const char *));

	MOCK_CONST_METHOD1(UnloadPlugin, void (CompPlugin *p));
};

class MockVTable :
    public CompPlugin::VTable
{
    public:
	MOCK_METHOD0(init, bool ());
	MOCK_METHOD0(markReadyToInstantiate, void ());
	MOCK_METHOD0(markNoFurtherInstantiation, void ());
};


bool
ThunkLoadPluginProc(CompPlugin *p, const char *path_, const char *name)
{
    return PluginFilesystem::instance->LoadPlugin(p, path_, name);
}

void
ThunkUnloadPluginProc(CompPlugin *p)
{
    PluginFilesystem::instance->UnloadPlugin(p);
}


PluginFilesystem::PluginFilesystem()
{
    ::loaderLoadPlugin = ::ThunkLoadPluginProc;
    ::loaderUnloadPlugin = ::ThunkUnloadPluginProc;

    instance = this;
}

PluginFilesystem const* PluginFilesystem::instance = 0;

} // (abstract) namespace

class PluginTest :
    public ::testing::Test
{
    public:

	MockPluginFilesystem mockfs;
};

TEST_F (PluginTest, load_non_existant_plugin_must_fail)
{
    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(AtMost(0));

    ASSERT_EQ(0, CompPlugin::load("dummy"));
}

TEST_F (PluginTest, load_plugin_from_HOME_PLUGINDIR_succeeds)
{
    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}

TEST_F (PluginTest, load_plugin_from_PLUGINDIR_succeeds)
{
    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	    Times(AtMost(0));;

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}

TEST_F (PluginTest, load_plugin_from_COMPIZ_PLUGIN_DIR_env_succeeds)
{
    const char *COMPIZ_PLUGIN_DIR_VALUE = "/path/to/plugin/dir";
    TmpEnv env ("COMPIZ_PLUGIN_DIR", COMPIZ_PLUGIN_DIR_VALUE);

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(COMPIZ_PLUGIN_DIR_VALUE), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	    Times(AtMost(0));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}

TEST_F (PluginTest, load_plugin_from_multi_COMPIZ_PLUGIN_DIR_env_succeeds)
{
    std::string COMPIZ_PLUGIN_DIR_PART1 = "/path/to/plugin/dir";
    std::string COMPIZ_PLUGIN_DIR_PART2 = "/dir/plugin/to/path";
    std::string COMPIZ_PLUGIN_DIR_VALUE = COMPIZ_PLUGIN_DIR_PART1 + ":" +
					  COMPIZ_PLUGIN_DIR_PART2;
    TmpEnv env ("COMPIZ_PLUGIN_DIR", COMPIZ_PLUGIN_DIR_VALUE.c_str ());

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(_,
				   EndsWith(COMPIZ_PLUGIN_DIR_PART1),
				   StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0),
				   StrEq(COMPIZ_PLUGIN_DIR_PART2),
				   StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0),
				   EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0),
				   EndsWith(PLUGINDIR), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0),
				   Eq((void*)0), StrEq("dummy"))).
	    Times(AtMost(0));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}

TEST_F (PluginTest, load_plugin_from_void_succeeds)
{
    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}

TEST_F (PluginTest, when_we_load_plugin_init_load_is_called_without_null_pointer)
{
    using namespace testing;

    EXPECT_CALL (mockfs, LoadPlugin (NotNull (),
				     EndsWith(HOME_PLUGINDIR),
				     StrEq("dummy")))
	.Times (1)
	.WillOnce (Return (true));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(AtLeast (0));

    CompPlugin* cp = CompPlugin::load("dummy");
    CompPlugin::unload(cp);
}

TEST_F (PluginTest, when_we_push_plugin_init_is_called)
{
    using namespace testing;

    EXPECT_CALL (mockfs, LoadPlugin (_, _, _)).Times (AtLeast (0));
    EXPECT_CALL (mockfs, UnloadPlugin(_)).Times (AtLeast (0));

    ON_CALL(mockfs, LoadPlugin (_, _, _)).
	WillByDefault (Return (true));

    MockVTable mockVtable;
    EXPECT_CALL (mockVtable, markReadyToInstantiate ()).Times (AtLeast (0));
    EXPECT_CALL (mockVtable, markNoFurtherInstantiation ())
	.Times (AtLeast (0));
    EXPECT_CALL (mockVtable, init ()).WillOnce (Return (true));

    CompPlugin* cp = CompPlugin::load("dummy");

    cp->vTable = &mockVtable;

    CompPlugin::push(cp);
    CompPlugin::pop ();
    CompPlugin::unload(cp);
}

TEST_F (PluginTest, when_we_push_plugin_mark_ready_to_instantiate_is_called)
{
    using namespace testing;

    EXPECT_CALL (mockfs, LoadPlugin (_, _, _)).Times (AtLeast (0));
    EXPECT_CALL (mockfs, UnloadPlugin(_)).Times (AtLeast (0));

    ON_CALL(mockfs, LoadPlugin (_, _, _)).
	WillByDefault (Return (true));

    MockVTable mockVtable;
    EXPECT_CALL (mockVtable, init ())
	.Times (AtLeast (0))
	.WillOnce (Return (true));
    EXPECT_CALL (mockVtable, markReadyToInstantiate ()).Times (1);
    EXPECT_CALL (mockVtable, markNoFurtherInstantiation ())
	.Times (AtLeast (0));

    CompPlugin* cp = CompPlugin::load("dummy");

    cp->vTable = &mockVtable;

    CompPlugin::push(cp);
    CompPlugin::pop ();
    CompPlugin::unload(cp);
}

TEST_F (PluginTest, when_we_pop_plugin_mark_no_further_instantiation_is_called)
{
    using namespace testing;

    EXPECT_CALL (mockfs, LoadPlugin (_, _, _)).Times (AtLeast (0));
    EXPECT_CALL (mockfs, UnloadPlugin(_)).Times (AtLeast (0));

    ON_CALL(mockfs, LoadPlugin (_, _, _)).
	WillByDefault (Return (true));

    MockVTable mockVtable;
    EXPECT_CALL (mockVtable, init ())
	.Times (AtLeast (0))
	.WillOnce (Return (true));
    EXPECT_CALL (mockVtable, markReadyToInstantiate ()).Times (AtLeast (0));

    CompPlugin* cp = CompPlugin::load("dummy");

    cp->vTable = &mockVtable;

    CompPlugin::push(cp);

    EXPECT_CALL (mockVtable, markNoFurtherInstantiation ())
	.Times (1);

    CompPlugin::pop ();
    CompPlugin::unload(cp);
}

TEST_F (PluginTest, pop_returns_plugin_pointer)
{
    using namespace testing;

    EXPECT_CALL (mockfs, LoadPlugin (_, _, _)).Times (AtLeast (0));
    EXPECT_CALL (mockfs, UnloadPlugin(_)).Times (AtLeast (0));

    ON_CALL(mockfs, LoadPlugin (_, _, _)).
	WillByDefault (Return (true));

    MockVTable mockVtable;
    EXPECT_CALL (mockVtable, init ())
	.Times (AtLeast (0))
	.WillOnce (Return (true));
    EXPECT_CALL (mockVtable, markReadyToInstantiate ())
	.Times (AtLeast (0));
    EXPECT_CALL (mockVtable, markNoFurtherInstantiation ())
	.Times (AtLeast (0));

    CompPlugin* cp = CompPlugin::load("dummy");

    cp->vTable = &mockVtable;

    CompPlugin::push(cp);
    EXPECT_EQ (cp, CompPlugin::pop ());
    CompPlugin::unload(cp);
}

TEST_F (PluginTest, unload_calls_unload_on_fs)
{
    using namespace testing;

    EXPECT_CALL (mockfs, LoadPlugin (_, _, _)).Times (AtLeast (0));

    ON_CALL(mockfs, LoadPlugin (_, _, _)).
	WillByDefault (Return (true));

    CompPlugin* cp = CompPlugin::load("dummy");

    EXPECT_CALL (mockfs, UnloadPlugin (cp)).Times (1);

    CompPlugin::unload(cp);
}

namespace
{

/* TODO (cleanup): Extract this into a separate library because it
 * duplicates what's in test-pluginclasshandler */
template <typename BaseType>
class Base :
    public PluginClassStorage
{
    public:

	typedef BaseType Type;

	Base ();
	~Base ();

	static unsigned int allocPluginClassIndex ();
	static void freePluginClassIndex (unsigned int index);

    private:

	static PluginClassStorage::Indices indices;
	static std::list <Base *>        bases;
};

template <typename BaseType>
PluginClassStorage::Indices Base <BaseType>::indices;

template <typename BaseType>
std::list <Base <BaseType> * > Base <BaseType>::bases;

template <typename BaseType>
Base <BaseType>::Base () :
    PluginClassStorage (indices)
{
    bases.push_back (this);
}

template <typename BaseType>
Base <BaseType>::~Base ()
{
    bases.remove (this);
}

template <typename BaseType>
unsigned int
Base <BaseType>::allocPluginClassIndex ()
{
    unsigned int i = PluginClassStorage::allocatePluginClassIndex (indices);

    foreach (Base *b, bases)
    {
	if (indices.size () != b->pluginClasses.size ())
	    b->pluginClasses.resize (indices.size ());
    }

    return i;
}

template <typename BaseType>
void
Base <BaseType>::freePluginClassIndex (unsigned int index)
{
    PluginClassStorage::freePluginClassIndex (indices, index);

    foreach (Base *b, bases)
    {
	if (indices.size () != b->pluginClasses.size ())
	    b->pluginClasses.resize (indices.size ());
    }
}

class BaseScreen :
    public Base <BaseScreen>
{
};

class BaseWindow :
    public Base <BaseWindow>
{
};

class DestructionVerifier
{
    public:

	DestructionVerifier ();

	virtual ~DestructionVerifier ();
	MOCK_METHOD0 (destroy, void ());
};

DestructionVerifier::DestructionVerifier ()
{
    using namespace testing;

    /* By default we don't care */
    EXPECT_CALL (*this, destroy ()).Times (AtLeast (0));
}

DestructionVerifier::~DestructionVerifier ()
{
    destroy ();
}

class PluginScreen :
    public PluginClassHandler <PluginScreen, BaseScreen>,
    public DestructionVerifier
{
    public:

	PluginScreen (BaseScreen *s);
};

class PluginWindow :
    public PluginClassHandler <PluginWindow, BaseWindow>,
    public DestructionVerifier
{
    public:

	virtual ~PluginWindow () {}
	PluginWindow (BaseWindow *s);
};

PluginScreen::PluginScreen (BaseScreen *s) :
    PluginClassHandler (s)
{
}

PluginWindow::PluginWindow (BaseWindow *w) :
    PluginClassHandler (w)
{
}

class MockScreenAndWindowVTable :
    public CompPlugin::VTableForScreenAndWindow <PluginScreen, PluginWindow>
{
    public:

	MOCK_METHOD0 (init, bool ());
};

template <class BasePluginType>
class PluginClassIntegrationTest :
    public PluginTest
{
    public:

	PluginClassIntegrationTest ();
	~PluginClassIntegrationTest ();

	boost::shared_ptr <CompPlugin>
	LoadPlugin (MockScreenAndWindowVTable &);

	MockPluginFilesystem      mockfs;
	ValueHolder               *valueHolder;
};

void
PopAndUnloadPlugin (CompPlugin *p)
{
    ASSERT_EQ (p, CompPlugin::pop ());
    CompPlugin::unload (p);
}

template <class BasePluginType>
boost::shared_ptr <CompPlugin>
PluginClassIntegrationTest <BasePluginType>::LoadPlugin (MockScreenAndWindowVTable &v)
{
    typedef PluginClassIntegrationTest <BasePluginType> TestParam;

    using namespace testing;

    EXPECT_CALL (TestParam::mockfs, LoadPlugin (_, _, _))
	.Times (AtLeast (0))
	.WillOnce (Return (true));
    EXPECT_CALL (TestParam::mockfs, UnloadPlugin(_))
	.Times (AtLeast (0));

    /* Load a plugin, we'll assign the vtable later */
    CompPlugin *cp = CompPlugin::load("dummy");
    cp->vTable = &v;

    EXPECT_CALL (v, init ())
	.Times (AtLeast (0))
	.WillOnce (Return (true));

    CompPlugin::push(cp);

    return AutoDestroy (cp,
			PopAndUnloadPlugin);
}

template <class BasePluginType>
PluginClassIntegrationTest <BasePluginType>::PluginClassIntegrationTest ()
{
    valueHolder = new ValueHolder ();
    ValueHolder::SetDefault (valueHolder);
}

template <class BasePluginType>
PluginClassIntegrationTest <BasePluginType>::~PluginClassIntegrationTest ()
{
    delete valueHolder;
    ValueHolder::SetDefault (NULL);
}

template <typename BaseType, typename PluginType>
class BasePluginTemplate
{
    public:

	typedef BaseType Base;
	typedef PluginType Plugin;
};

typedef BasePluginTemplate <BaseScreen, PluginScreen> BasePluginScreen;
typedef BasePluginTemplate <BaseWindow, PluginWindow> BasePluginWindow;

}

/* TODO: Extract actual interfaces out of CompScreen
 * and CompWindow that can be passed to the vTables without
 * using a link-seam like this one */
class CompScreen :
    public BaseScreen
{
};

class CompWindow :
    public BaseWindow
{
};

typedef ::testing::Types <BasePluginScreen, BasePluginWindow> PluginTypes;
TYPED_TEST_CASE (PluginClassIntegrationTest, PluginTypes);

TYPED_TEST (PluginClassIntegrationTest, get_plugin_structure_null_on_not_loaded)
{
    using namespace testing;

    /* Can't figure out how to get this out of TestParam::base at the moment */
    typename TypeParam::Base base;
    typename TypeParam::Plugin *p = TypeParam::Plugin::get (&base);

    EXPECT_THAT (p, IsNull ());
}

TYPED_TEST (PluginClassIntegrationTest, get_plugin_structure_nonnull_on_loaded)
{
    using namespace testing;

    typedef PluginClassIntegrationTest <TypeParam> TestParam;

    MockScreenAndWindowVTable vTable;
    boost::shared_ptr <CompPlugin> cp (TestParam::LoadPlugin (vTable));

    typename TypeParam::Base base;
    typedef boost::shared_ptr <typename TypeParam::Plugin> PluginPtr;

    /* Because CompScreen is not available, we just need to delete
     * the plugin structure ourselves */
    PluginPtr p (TypeParam::Plugin::get (&base));

    EXPECT_THAT (p.get (), NotNull ());
}

TYPED_TEST (PluginClassIntegrationTest, plugin_class_destroyed_when_vtable_is)
{
    using namespace testing;

    typedef PluginClassIntegrationTest <TypeParam> TestParam;

    MockScreenAndWindowVTable vTable;
    boost::shared_ptr <CompPlugin> cp (TestParam::LoadPlugin (vTable));

    typename TypeParam::Base base;
    typedef boost::shared_ptr <typename TypeParam::Plugin> PluginPtr;

    /* Because CompScreen is not available, we just need to delete
     * the plugin structure ourselves */
    PluginPtr p (TypeParam::Plugin::get (&base));

    EXPECT_CALL (*p, destroy ()).Times (1);
}
