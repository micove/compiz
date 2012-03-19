#include "test-local-menu.h"
#include <string.h>
#include <gdk/gdkx.h>

#define GLOBAL 0
#define LOCAL 1
#ifdef META_HAS_LOCAL_MENUS

TEST_F (GtkWindowDecoratorTestLocalMenu, TestOnNoProp)
{
    g_settings_set_enum (getSettings (), "menu-mode", LOCAL);
    gboolean result = gwd_window_should_have_local_menu (getWindow ());

    EXPECT_FALSE (result);
}

TEST_F (GtkWindowDecoratorTestLocalMenu, TestOnWithProp)
{
    g_settings_set_enum (getSettings (), "menu-mode", LOCAL);

    Window xid = getWindow ();
    Atom   ubuntu_appmenu_unique_name = XInternAtom (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), "_UBUNTU_APPMENU_UNIQUE_NAME", FALSE);
    Atom   utf8_string = XInternAtom (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), "UTF8_STRING", FALSE);
    const char   data[] = ":abcd1234";

    XChangeProperty (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), xid, ubuntu_appmenu_unique_name, utf8_string, 8, PropModeReplace, (const unsigned char *) data, strlen (data));

    gdk_display_sync (gdk_display_get_default ());

    gboolean result = gwd_window_should_have_local_menu (getWindow ());

    EXPECT_TRUE (result);
}

TEST_F (GtkWindowDecoratorTestLocalMenu, TestOff)
{
    g_settings_set_enum (getSettings (), "menu-mode", GLOBAL);
    gboolean result = gwd_window_should_have_local_menu (getWindow ());

    EXPECT_FALSE (result);
}
#else
TEST_F (GtkWindowDecoratorTestLocalMenu, NoMenus)
{
    ASSERT_TRUE (true) << "Local menus tests not enabled because META_HAS_LOCAL_MENUS is off";
}
#endif
