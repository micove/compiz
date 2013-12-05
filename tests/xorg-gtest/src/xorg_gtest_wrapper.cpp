/*
 * Compiz XOrg GTest Wrapper
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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int usage ()
{
    printf ("usage: xorg_gtest_wrapper EXECUTABLE ARGUMENTS\n");
    return 1;
}

int main (int argc, char **argv)
{
    if (argc < 2)
	return usage ();

    /* We need to force the child stdout to remain open here.
     *
     * This is ugly, but the X Server and clients seem to
     * have some real trouble when you close their stdin/stdout.
     * It isn't really clear why, but at having a noisy test is
     * better than having a test that fails randomly. Without it, 
     * the tests will occasionally hang in _XReply (), or the servers
     * will hang in other areas, probably in an IPC area or syscall
     */
    setenv ("XORG_GTEST_CHILD_STDOUT", "1", 1);

    char * const *exec_args = const_cast <char * const *> (&argv[1]);

    /* Fork and spawn the new process */
    pid_t test = fork ();

    if (test == -1)
    {
	perror ("fork");
	return 1;
    }

    if (test == 0)
    {
	if (execvp (argv[1], exec_args) == -1)
	{
	    perror ("execvp");
	    return 1;
	}
    }

    int   status = 0;

    /* Make sure it exited normally */
    do
    {
	pid_t child = waitpid (test, &status, 0);
	if (test == child)
	{
	    if (WIFSIGNALED (status))
		return 1;
	}
	else
	{
	    perror ("waitpid");
	    return 1;
	}
    } while (!WIFEXITED (status) && !WIFSIGNALED (status));

   return WEXITSTATUS (status);
}
