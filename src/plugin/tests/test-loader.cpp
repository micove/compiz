#include "core/plugin.h"

// This prevents an instantiation error - not sure why ATM
#include "core/screen.h"

// Get rid of stupid macro from X.h
// Why, oh why, are we including X.h?
#undef None

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/stat.h>

namespace {

template <int Len>
bool
check(CompStringList const& actual, char const* const (&expect)[Len])
{
    if (actual.size() != Len) return false;

    CompStringList::const_iterator p = actual.begin();
    for (int i = 0; i != Len; ++i, ++p)
	if (*p != expect[i]) return false;

    return true;
}

bool
check(CompStringList const& actual)
{
    if (actual.size() != 0) return false;
    return true;
}
} // (abstract) namespace



TEST(LoaderTest, dllLoaderListPlugins_null)
{
    char const* const expectCore[] = { "core" };

    EXPECT_TRUE(check(loaderListPlugins(0), expectCore));
}

TEST(LoaderTest, dllLoaderListPlugins_non_such)
{
    char const* const non_such = "non-such";
    rmdir(non_such);

    EXPECT_TRUE(check(loaderListPlugins(non_such)));
}

TEST(LoaderTest, dllLoaderListPlugins_empty)
{
    char const* const empty = "empty";

    rmdir(empty);
    EXPECT_EQ(0, mkdir(empty,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));

    EXPECT_TRUE(check(loaderListPlugins(empty)));
    rmdir(empty);
}


TEST(LoaderTest, dllLoaderListPlugins_exists)
{
    char const* const expectComposite[] = { "composite" };

    EXPECT_TRUE(check(loaderListPlugins("./plugins/composite"), expectComposite));
}
