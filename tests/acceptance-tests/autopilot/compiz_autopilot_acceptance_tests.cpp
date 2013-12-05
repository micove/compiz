/*
 * Compiz Autopilot GTest Acceptance Tests
 *
 * Copyright (C) 2013 Canonical Ltd.
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
 *
 * Authored By:
 * Sam Spilsbury <smspillaz@gmail.com>
 */

#include <stdexcept>
#include <boost/noncopyable.hpp>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <errno.h>

#include <gtest/gtest.h>

using ::testing::ValuesIn;
using ::testing::WithParamInterface;

namespace
{
    class Pipe :
	boost::noncopyable
    {
	public:

	    Pipe ()
	    {
		if (pipe2 (mPipe, O_CLOEXEC) == -1)
		    throw std::runtime_error (strerror (errno));
	    }

	    ~Pipe ()
	    {
		if (mPipe[0] &&
		    close (mPipe[0]) == -1)
		    std::cerr << "mPipe[0] " << strerror (errno) << std::endl;

		if (mPipe[1] &&
		    close (mPipe[1]) == -1)
		    std::cerr << "mPipe[0] " << strerror (errno) << std::endl;
	    }

	    /* Read end descriptor is read-only */
	    int ReadEnd ()
	    {
		return mPipe[0];
	    }

	    /* Write end descriptor is writable, we need to close it
	     * from other objects */
	    int & WriteEnd ()
	    {
		return mPipe[1];
	    }

	private:

	    int mPipe[2];
    };

    class FileDescriptorBackup :
	boost::noncopyable
    {
	public:

	    FileDescriptorBackup (int fd) :
		mOriginalFd (fd),
		mBackupFd (0)
	    {
		mBackupFd = dup (mOriginalFd);

		/* Save original */
		if (mBackupFd == -1)
		    throw std::runtime_error (strerror (errno));
	    }

	    ~FileDescriptorBackup ()
	    {
		/* Redirect backed up fd to old fd location */
		if (mBackupFd &&
		    dup2 (mBackupFd, mOriginalFd) == -1)
		    std::cerr << "Failed to restore file descriptor "
			      << strerror (errno) << std::endl;
	    }

	private:

	    int mOriginalFd;
	    int mBackupFd;
    };

    class RedirectedFileDescriptor :
	boost::noncopyable
    {
	public:

	    RedirectedFileDescriptor (int from,
				      int &to) :
		mFromFd (from),
		mToFd (to)
	    {
		/* Make 'to' take the old file descriptor's place */
		if (dup2 (to, from) == -1)
		    throw std::runtime_error (strerror (errno));
	    }

	    ~RedirectedFileDescriptor ()
	    {
		if (mToFd &&
		    close (mToFd) == -1)
		    std::cerr << "Failed to close redirect-to file descriptor "
			      << strerror (errno) << std::endl;

		mToFd = 0;
	    }

	private:

	    int mFromFd;
	    int &mToFd;
    };

    pid_t launchBinary (const std::string &executable,
			const char        **argv,
			int               &stderrWriteEnd,
			int               &stdoutWriteEnd)
    {
	FileDescriptorBackup stderr (STDERR_FILENO);
	FileDescriptorBackup stdout (STDOUT_FILENO);

	/* Close the originals once they have been backed up
	 * We have to do this here and not in the FileDescriptorBackup
	 * constructors because of an order-of-operations issue -
	 * namely if we close an original file descriptor name
	 * before duplicating another one, then there's a possibility
	 * that the duplicated other one will get the same name as
	 * the one we just closed, making us unable to restore
	 * the closed one properly */
	if (close (STDERR_FILENO) == -1)
	    throw std::runtime_error (strerror (errno));

	if (close (STDOUT_FILENO) == -1)
	    throw std::runtime_error (strerror (errno));

	/* Replace the current process stderr and stdout with the write end
	 * of the pipes. Now when someone tries to write to stderr or stdout
	 * they'll write to our pipes instead */
	RedirectedFileDescriptor pipedStderr (STDERR_FILENO, stderrWriteEnd);
	RedirectedFileDescriptor pipedStdout (STDOUT_FILENO, stdoutWriteEnd);

	/* Fork process, child gets a copy of the pipe write ends
	 * - the original pipe write ends will be closed on exec
	 * but the duplicated write ends now taking the place of
	 * stderr and stdout will not be */
	pid_t child = fork ();

	/* Child process */
	if (child == 0)
	{
	    if (execvpe (executable.c_str (),
			 const_cast <char * const *> (argv),
			 environ) == -1)
	    {
		std::cerr << "execvpe failed with error "
			  << errno
			  << std::endl
			  << " - binary "
			  << executable
			  << std::endl;
		abort ();
	    }
	}
	/* Parent process - error */
	else if (child == -1)
	    throw std::runtime_error (strerror (errno));

	/* The old file descriptors for the stderr and stdout
	 * are put back in place, and pipe write ends closed
	 * as the child is using them at return */

	return child;
    }

    int launchBinaryAndWaitForReturn (const std::string &executable,
				      const char        **argv,
				      int               &stderrWriteEnd,
				      int               &stdoutWriteEnd)
    {
	int status = 0;
	pid_t child = launchBinary (executable,
				    argv,
				    stderrWriteEnd,
				    stdoutWriteEnd);

	do
	{
	    /* Wait around for the child to get a signal */
	    pid_t waitChild = waitpid (child, &status, 0);
	    if (waitChild == child)
	    {
		/* If it died unexpectedly, say so */
		if (WIFSIGNALED (status))
		{
		    std::stringstream ss;
		    ss << "child killed by signal "
		       << WTERMSIG (status);
		    throw std::runtime_error (ss.str ());
		}
	    }
	    else
	    {
		/* waitpid () failed */
		throw std::runtime_error (strerror (errno));
	    }

	    /* Keep going until it exited */
	} while (!WIFEXITED (status) && !WIFSIGNALED (status));

	/* Return the exit code */
	return WEXITSTATUS (status);
    }

    const char *autopilot = "/usr/bin/autopilot";
    const char *runOpt = "run";
    const char *dashV = "-v";
}

class CompizAutopilotAcceptanceTest :
    public ::testing::Test,
    public ::testing::WithParamInterface <const char *>
{
    public:

	CompizAutopilotAcceptanceTest ();
	const char ** GetAutopilotArgv ();
	void PrintChildStderr ();
	void PrintChildStdout ();

    protected:

	std::vector <const char *> autopilotArgv;
	Pipe                       childStdoutPipe;
	Pipe			   childStderrPipe;
};

CompizAutopilotAcceptanceTest::CompizAutopilotAcceptanceTest ()
{
    autopilotArgv.push_back (autopilot);
    autopilotArgv.push_back (runOpt);
    autopilotArgv.push_back (dashV);
    autopilotArgv.push_back (GetParam ());
    autopilotArgv.push_back (NULL);
}

const char **
CompizAutopilotAcceptanceTest::GetAutopilotArgv ()
{
    return &autopilotArgv[0];
}

namespace
{
    std::string FdToString (int fd)
    {
	std::string output;

	int bufferSize = 4096;
	char buffer[bufferSize];

	ssize_t count = 0;

	do
	{
	    struct pollfd pfd;
	    pfd.events = POLLIN | POLLERR | POLLHUP;
	    pfd.revents = 0;
	    pfd.fd = fd;

	    /* Check for 10ms if there's anything waiting to be read */
	    int nfds = poll (&pfd, 1, 10);

	    if (nfds == -1)
		throw std::runtime_error (strerror (errno));

	    if (nfds)
	    {
		/* Read as much as we have allocated for */
		count = read (fd, (void *) buffer, bufferSize - 1);

		/* Something failed, bail */
		if (count == -1)
		    throw std::runtime_error (strerror (errno));

		/* Always null-terminate */
		buffer[count] = '\0';

		/* Add it to the output */
		output += buffer;
	    }
	    else
	    {
		/* There's nothing on the pipe, assume EOF */
		count = 0;
	    }

	    /* Keep going until there's nothing left */
	} while (count != 0);

	return output;
    }
}

void
CompizAutopilotAcceptanceTest::PrintChildStderr ()
{
    std::string output = FdToString (childStderrPipe.ReadEnd ());

    std::cout << "[== TEST ERRORS ==]" << std::endl
	      << output
	      << std::endl;
}

void
CompizAutopilotAcceptanceTest::PrintChildStdout ()
{
    std::string output = FdToString (childStdoutPipe.ReadEnd ());

    std::cout << "[== TEST MESSAGES ==]" << std::endl
	      << output
	      << std::endl;
}

TEST_P (CompizAutopilotAcceptanceTest, AutopilotTest)
{
    std::string scopedTraceMsg ("Running Autopilot Test");
    scopedTraceMsg += GetParam ();

    int status = launchBinaryAndWaitForReturn (std::string (autopilot),
					       GetAutopilotArgv (),
					       childStderrPipe.WriteEnd (),
					       childStdoutPipe.WriteEnd ());

    EXPECT_EQ (status, 0) << "expected exit status of 0";

    if (status)
    {
	PrintChildStdout ();
	PrintChildStderr ();
    }
    else
    {
	/* Extra space here to align with gtest output */
	std::cout << "[AUTOPILOT ] Pass test " << GetParam () << std::endl;
    }
}

namespace
{
const char *AutopilotTests[] =
{
    "unity.tests.launcher.test_icon_behavior.LauncherIconsTests.test_clicking_icon_twice_initiates_spread",
    "unity.tests.launcher.test_icon_behavior.LauncherIconsTests.test_expo_launcher_icon_initiates_expo",
    "unity.tests.launcher.test_icon_behavior.LauncherIconsTests.test_expo_launcher_icon_terminates_expo",
    "unity.tests.launcher.test_icon_behavior.LauncherIconsTests.test_launcher_activate_last_focused_window",
    "unity.tests.launcher.test_icon_behavior.LauncherIconsTests.test_unminimize_initially_minimized_windows",
    "unity.tests.launcher.test_icon_behavior.LauncherIconsTests.test_unminimize_minimized_immediately_after_show_windows",
    "unity.tests.launcher.test_icon_behavior.LauncherIconsTests.test_while_in_scale_mode_the_dash_will_still_open",
    "unity.tests.test_dash.DashRevealWithSpreadTests.test_command_lens_opens_when_in_spread",
    "unity.tests.test_dash.DashRevealWithSpreadTests.test_dash_closes_on_spread",
    "unity.tests.test_dash.DashRevealWithSpreadTests.test_dash_opens_when_in_spread",
    "unity.tests.test_dash.DashRevealWithSpreadTests.test_lens_opens_when_in_spread",
    "unity.tests.test_hud.HudBehaviorTests.test_alt_arrow_keys_not_eaten",
    "unity.tests.test_panel.PanelCrossMonitorsTests.test_panel_title_updates_moving_window",
    "unity.tests.test_panel.PanelCrossMonitorsTests.test_window_buttons_close_inactive_when_clicked_in_another_monitor",
    "unity.tests.test_panel.PanelCrossMonitorsTests.test_window_buttons_dont_show_for_maximized_window_on_mouse_in",
    "unity.tests.test_panel.PanelCrossMonitorsTests.test_window_buttons_dont_show_in_other_monitors_when_dash_is_open",
    "unity.tests.test_panel.PanelCrossMonitorsTests.test_window_buttons_dont_show_in_other_monitors_when_hud_is_open",
    "unity.tests.test_panel.PanelCrossMonitorsTests.test_window_buttons_minimize_inactive_when_clicked_in_another_monitor",
    "unity.tests.test_panel.PanelCrossMonitorsTests.test_window_buttons_unmaximize_inactive_when_clicked_in_another_monitor",
    "unity.tests.test_panel.PanelGrabAreaTests.test_focus_the_maximized_window_works",
    "unity.tests.test_panel.PanelGrabAreaTests.test_lower_the_maximized_window_works",
    "unity.tests.test_panel.PanelGrabAreaTests.test_panels_dont_steal_keynav_foucs_from_hud",
    "unity.tests.test_panel.PanelGrabAreaTests.test_unmaximize_from_grab_area_works",
    "unity.tests.test_panel.PanelHoverTests.test_menus_show_for_maximized_window_on_mouse_in_btn_area",
    "unity.tests.test_panel.PanelHoverTests.test_menus_show_for_maximized_window_on_mouse_in_grab_area",
    "unity.tests.test_panel.PanelHoverTests.test_menus_show_for_maximized_window_on_mouse_in_menu_area",
    "unity.tests.test_panel.PanelHoverTests.test_only_menus_show_for_restored_window_on_mouse_in_grab_area",
    "unity.tests.test_panel.PanelHoverTests.test_only_menus_show_for_restored_window_on_mouse_in_menu_area",
    "unity.tests.test_panel.PanelHoverTests.test_only_menus_show_for_restored_window_on_mouse_in_window_btn_area",
    "unity.tests.test_panel.PanelMenuTests.test_menus_dont_show_for_maximized_window_on_mouse_out",
    "unity.tests.test_panel.PanelMenuTests.test_menus_dont_show_for_restored_window_on_mouse_out",
    "unity.tests.test_panel.PanelMenuTests.test_menus_dont_show_if_a_new_application_window_is_opened",
    "unity.tests.test_panel.PanelMenuTests.test_menus_show_for_maximized_window_on_mouse_in",
    "unity.tests.test_panel.PanelMenuTests.test_menus_show_for_restored_window_on_mouse_in",
    "unity.tests.test_panel.PanelMenuTests.test_menus_shows_when_new_application_is_opened",
    "unity.tests.test_panel.PanelTitleTests.test_panel_shows_app_title_with_maximised_app",
    "unity.tests.test_panel.PanelTitleTests.test_panel_title_doesnt_change_with_switcher",
    "unity.tests.test_panel.PanelTitleTests.test_panel_title_on_empty_desktop",
    "unity.tests.test_panel.PanelTitleTests.test_panel_title_updates_on_maximized_window_title_changes",
    "unity.tests.test_panel.PanelTitleTests.test_panel_title_updates_when_switching_to_maximized_app",
    "unity.tests.test_panel.PanelTitleTests.test_panel_title_with_maximized_application",
    "unity.tests.test_panel.PanelTitleTests.test_panel_title_with_maximized_window_restored_child",
    "unity.tests.test_panel.PanelTitleTests.test_panel_title_with_restored_application",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_double_click_unmaximize_window",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_minimize_button_disabled_for_non_minimizable_windows",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_dont_show_for_maximized_window_on_mouse_out",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_dont_show_for_restored_window",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_dont_show_for_restored_window_with_mouse_in_panel",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_dont_show_on_empty_desktop",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_minimize_button_works_for_window",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_show_for_maximized_window_on_mouse_in",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_unmaximize_button_works_for_window",
    "unity.tests.test_panel.PanelWindowButtonsTests.test_window_buttons_unmaximize_follows_fitts_law",
    "unity.tests.test_showdesktop.ShowDesktopTests.test_showdesktop_hides_apps",
    "unity.tests.test_showdesktop.ShowDesktopTests.test_showdesktop_switcher",
    "unity.tests.test_showdesktop.ShowDesktopTests.test_showdesktop_unhides_apps",
    "unity.tests.test_showdesktop.ShowDesktopTests.test_unhide_single_app",
    "unity.tests.test_spread.SpreadTests.test_scale_application_windows",
    "unity.tests.test_spread.SpreadTests.test_scaled_window_closes_on_close_button_click",
    "unity.tests.test_spread.SpreadTests.test_scaled_window_closes_on_middle_click",
    "unity.tests.test_spread.SpreadTests.test_scaled_window_is_focused_on_click",
    "unity.tests.test_switcher.SwitcherDetailsModeTests.test_detail_mode_selects_last_active_window",
    "unity.tests.test_switcher.SwitcherDetailsModeTests.test_detail_mode_selects_third_window",
    "unity.tests.test_switcher.SwitcherDetailsTests.test_no_details_for_apps_on_different_workspace",
    "unity.tests.test_switcher.SwitcherTests.test_application_window_is_fake_decorated",
    "unity.tests.test_switcher.SwitcherTests.test_application_window_is_fake_decorated_in_detail_mode",
    "unity.tests.test_switcher.SwitcherWindowsManagementTests.test_switcher_raises_only_last_focused_window",
    "unity.tests.test_switcher.SwitcherWindowsManagementTests.test_switcher_rises_next_window_of_same_application",
    "unity.tests.test_switcher.SwitcherWindowsManagementTests.test_switcher_rises_other_application",
    "unity.tests.test_switcher.SwitcherWorkspaceTests.test_switcher_all_mode_shows_all_apps",
    "unity.tests.test_switcher.SwitcherWorkspaceTests.test_switcher_can_switch_to_minimised_window",
    "unity.tests.test_switcher.SwitcherWorkspaceTests.test_switcher_is_disabled_when_wall_plugin_active",
    "unity.tests.test_switcher.SwitcherWorkspaceTests.test_switcher_shows_current_workspace_only"
};
}


INSTANTIATE_TEST_CASE_P (UnityIntegrationAutopilotTests, CompizAutopilotAcceptanceTest,
			 ValuesIn (AutopilotTests));
