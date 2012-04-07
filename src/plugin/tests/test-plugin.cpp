#include "core/plugin.h"

// This prevents an instantiation error - not sure why ATM
#include "core/screen.h"

// Get rid of stupid macro from X.h
// Why, oh why, are we including X.h?
#undef None

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

class PluginFilesystem
{
public:
    virtual bool
    LoadPlugin(CompPlugin *p, const char *path, const char *name) const = 0;

    virtual void
    UnloadPlugin(CompPlugin *p) const = 0;

    virtual CompStringList
    ListPlugins(const char *path) const = 0;

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

    MOCK_CONST_METHOD1(ListPlugins, CompStringList (const char *path));
};

class MockVTable : public CompPlugin::VTable
{
public:
    MOCK_METHOD0(init, bool ());
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


CompStringList
ThunkListPluginsProc(const char *path)
{
    return PluginFilesystem::instance->ListPlugins(path);
}

PluginFilesystem::PluginFilesystem()
{
	::loaderLoadPlugin = ::ThunkLoadPluginProc;
	::loaderUnloadPlugin = ::ThunkUnloadPluginProc;
	::loaderListPlugins = ::ThunkListPluginsProc;

	instance = this;
}

PluginFilesystem const* PluginFilesystem::instance = 0;

} // (abstract) namespace



TEST(PluginTest, load_non_existant_plugin_must_fail)
{
    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(AtMost(0));
    EXPECT_CALL(mockfs, ListPlugins(_)).Times(AtMost(0));

    ASSERT_EQ(0, CompPlugin::load("dummy"));
}

TEST(PluginTest, load_plugin_from_HOME_PLUGINDIR_succeeds)
{
    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);
    EXPECT_CALL(mockfs, ListPlugins(_)).Times(AtMost(0));

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}

TEST(PluginTest, load_plugin_from_PLUGINDIR_succeeds)
{
    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	    Times(AtMost(0));;

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);
    EXPECT_CALL(mockfs, ListPlugins(_)).Times(AtMost(0));

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}

TEST(PluginTest, load_plugin_from_void_succeeds)
{
    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(false));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);
    EXPECT_CALL(mockfs, ListPlugins(_)).Times(AtMost(0));

    CompPlugin* cp = CompPlugin::load("dummy");
    ASSERT_NE((void*)0, cp);

    CompPlugin::unload(cp);
}


TEST(PluginTest, list_plugins_none)
{
    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(_, _, _)).Times(AtMost(0));
    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(AtMost(0));

    EXPECT_CALL(mockfs, ListPlugins(EndsWith(HOME_PLUGINDIR))).
	WillOnce(Return(CompStringList()));

    EXPECT_CALL(mockfs, ListPlugins(EndsWith(PLUGINDIR))).
	WillOnce(Return(CompStringList()));

    EXPECT_CALL(mockfs, ListPlugins(Eq((void*)0))).
	WillOnce(Return(CompStringList()));

    CompStringList cl = CompPlugin::availablePlugins();
    ASSERT_EQ(0, cl.size());
}


TEST(PluginTest, list_plugins_some)
{
    std::string one = "one";
    std::string two = "two";
    std::string three = "three";

    CompStringList home(1, one);
    CompStringList plugin(1, two);
    CompStringList local(1, three);

    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(_, _, _)).Times(AtMost(0));
    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(AtMost(0));

    EXPECT_CALL(mockfs, ListPlugins(EndsWith(HOME_PLUGINDIR))).WillOnce(Return(home));
    EXPECT_CALL(mockfs, ListPlugins(EndsWith(PLUGINDIR))).WillOnce(Return(plugin));
    EXPECT_CALL(mockfs, ListPlugins(Eq((void*)0))).WillOnce(Return(local));

    CompStringList cl = CompPlugin::availablePlugins();
    ASSERT_EQ(3, cl.size());

    ASSERT_THAT(cl, Contains(one));
    ASSERT_THAT(cl, Contains(two));
    ASSERT_THAT(cl, Contains(three));
}

TEST(PluginTest, list_plugins_are_deduped)
{
    std::string one = "one";
    std::string two = "two";
    std::string three = "three";

    CompStringList home(1, one);
    CompStringList plugin(1, two);
    CompStringList local(1, three);

    plugin.push_back(one);
    local.push_back(two);

    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(_, _, _)).Times(AtMost(0));
    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(AtMost(0));

    EXPECT_CALL(mockfs, ListPlugins(EndsWith(HOME_PLUGINDIR))).WillOnce(Return(home));
    EXPECT_CALL(mockfs, ListPlugins(EndsWith(PLUGINDIR))).WillOnce(Return(plugin));
    EXPECT_CALL(mockfs, ListPlugins(Eq((void*)0))).WillOnce(Return(local));

    CompStringList cl = CompPlugin::availablePlugins();
    ASSERT_EQ(3, cl.size());

    ASSERT_THAT(cl, Contains(one));
    ASSERT_THAT(cl, Contains(two));
    ASSERT_THAT(cl, Contains(three));
}


TEST(PluginTest, when_we_push_plugin_init_is_called)
{
    MockPluginFilesystem mockfs;

    using namespace testing;

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(HOME_PLUGINDIR), StrEq("dummy"))).
	WillOnce(Return(true));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), EndsWith(PLUGINDIR), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, LoadPlugin(Ne((void*)0), Eq((void*)0), StrEq("dummy"))).
	Times(AtMost(0));

    EXPECT_CALL(mockfs, UnloadPlugin(_)).Times(1);
    EXPECT_CALL(mockfs, ListPlugins(_)).Times(0);

    MockVTable mockVtable;
    EXPECT_CALL(mockVtable, init()).WillOnce(Return(true));

    CompPlugin* cp = CompPlugin::load("dummy");

    cp->vTable = &mockVtable;

    CompPlugin::push(cp);
    ASSERT_EQ(cp, CompPlugin::pop());
    CompPlugin::unload(cp);
}
