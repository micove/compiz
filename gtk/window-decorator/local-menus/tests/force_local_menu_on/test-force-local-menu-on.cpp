#include "test-local-menu.h"
#include <cstring>
#include <gdk/gdkx.h>

#define GLOBAL 0
#define LOCAL 1
#ifdef META_HAS_LOCAL_MENUS

namespace
{
    void initializeMetaButtonLayout (MetaButtonLayout *layout)
    {
	memset (layout, 0, sizeof (MetaButtonLayout));

	unsigned int i;

	for (i = 0; i < MAX_BUTTONS_PER_CORNER; i++)
	{
	    layout->right_buttons[i] = META_BUTTON_FUNCTION_LAST;
	    layout->left_buttons[i] = META_BUTTON_FUNCTION_LAST;
	}
    }
}

class GtkWindowDecoratorTestLocalMenuLayout :
    public GtkWindowDecoratorTestLocalMenu
{
    public:

	MetaButtonLayout * getLayout () { return &mLayout; }

	virtual void SetUp ()
	{
	    GtkWindowDecoratorTestLocalMenu::SetUp ();
	    ::initializeMetaButtonLayout (&mLayout);
	    g_settings_set_enum (getSettings (), "menu-mode", LOCAL);

	    Window xid = getWindow ();
	    Atom   ubuntu_appmenu_unique_name = XInternAtom (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), "_UBUNTU_APPMENU_UNIQUE_NAME", FALSE);
	    Atom   utf8_string = XInternAtom (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), "UTF8_STRING", FALSE);
	    const char   data[] = ":abcd1234";

	    XChangeProperty (gdk_x11_display_get_xdisplay (gdk_display_get_default ()), xid, ubuntu_appmenu_unique_name, utf8_string, 8, PropModeReplace, (const unsigned char *) data, strlen (data));

	    gdk_display_sync (gdk_display_get_default ());

	    ASSERT_TRUE (gwd_window_should_have_local_menu (getWindow ()));
	}

    private:

	MetaButtonLayout mLayout;
};

TEST_F (GtkWindowDecoratorTestLocalMenuLayout, TestForceNoButtonsSet)
{
    force_local_menus_on (getWindow (), getLayout ());

    EXPECT_EQ (getLayout ()->right_buttons[0], META_BUTTON_FUNCTION_LAST);
    EXPECT_EQ (getLayout ()->left_buttons[0], META_BUTTON_FUNCTION_LAST);
}

TEST_F (GtkWindowDecoratorTestLocalMenuLayout, TestForceNoCloseButtonSet)
{
    getLayout ()->right_buttons[0] = META_BUTTON_FUNCTION_CLOSE;

    force_local_menus_on (getWindow (), getLayout ());

    EXPECT_EQ (getLayout ()->right_buttons[1], META_BUTTON_FUNCTION_WINDOW_MENU);
    EXPECT_EQ (getLayout ()->left_buttons[0], META_BUTTON_FUNCTION_LAST);
}

TEST_F (GtkWindowDecoratorTestLocalMenuLayout, TestForceNoCloseMinimizeMaximizeButtonSet)
{
    getLayout ()->left_buttons[0] = META_BUTTON_FUNCTION_CLOSE;
    getLayout ()->left_buttons[1] = META_BUTTON_FUNCTION_CLOSE;
    getLayout ()->left_buttons[2] = META_BUTTON_FUNCTION_CLOSE;

    force_local_menus_on (getWindow (), getLayout ());

    EXPECT_EQ (getLayout ()->right_buttons[0], META_BUTTON_FUNCTION_LAST);
    EXPECT_EQ (getLayout ()->left_buttons[3], META_BUTTON_FUNCTION_WINDOW_MENU);
}
#else
TEST_F (GtkWindowDecoratorTestLocalMenu, NoMenus)
{
    ASSERT_TRUE (true) << "Local menus tests not enabled because META_HAS_LOCAL_MENUS is off";
}
#endif
