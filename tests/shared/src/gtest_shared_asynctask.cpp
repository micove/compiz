/*
 * Copyright (C) 2012 Canonical Ltd.
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
 * Sam Spilsbury <sam.spilsbury@canonical.com>
 */
#include <stdexcept>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include "gtest_shared_asynctask.h"
#include "gtest_shared_characterwrapper.h"

namespace
{
void
runtimeErrorWithErrno ()
{
    CharacterWrapper allocatedError (strerror (errno));
    throw std::runtime_error (std::string (allocatedError));
}

bool
checkForMessageOnFd (int fd, int timeout, char expected)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    /* Wait 1ms */
    poll (&pfd, 1, timeout);

    if (pfd.revents == POLLIN)
    {
	char msg[1];
	size_t sz = read (pfd.fd, reinterpret_cast <void *> (msg), 1);
	if (sz == 1 &&
	    msg[0] == expected)
	    return true;
    }

    return false;
}
}

class PrivateAsyncTask
{
    public:

	int testToTaskFd[2];
	int taskToTestFd[2];
	pthread_t mThread;
};

AsyncTask::AsyncTask () :
    priv (new PrivateAsyncTask)
{
    if (pipe2 (priv->testToTaskFd, O_NONBLOCK | O_CLOEXEC) == -1)
	runtimeErrorWithErrno ();

    if (pipe2 (priv->taskToTestFd, O_NONBLOCK | O_CLOEXEC) == -1)
	runtimeErrorWithErrno ();

    if (pthread_create (&priv->mThread,
			NULL,
			AsyncTask::ThreadEntry,
			reinterpret_cast<void *> (this)) != 0)
	runtimeErrorWithErrno ();
}

AsyncTask::~AsyncTask ()
{
    void *ret;
    if (pthread_join (priv->mThread, &ret) != 0)
    {
	runtimeErrorWithErrno ();
    }

    close (priv->testToTaskFd[0]);
    close (priv->testToTaskFd[1]);
    close (priv->taskToTestFd[0]);
    close (priv->taskToTestFd[0]);
}

bool
AsyncTask::ReadMsgFromTest (char msg, int maximumWait)
{
    return checkForMessageOnFd (priv->testToTaskFd[0], maximumWait, msg);
}

bool
AsyncTask::ReadMsgFromTask (char msg, int maximumWait)
{
    return checkForMessageOnFd (priv->taskToTestFd[0], maximumWait, msg);
}

void
AsyncTask::SendMsgToTest (char msg)
{
    char buf[1] = { msg };
    if (write (priv->taskToTestFd[1], reinterpret_cast <void *> (buf), 1) == -1)
	runtimeErrorWithErrno ();
}

void
AsyncTask::SendMsgToTask (char msg)
{
    char buf[1] = { msg };
    if (write (priv->testToTaskFd[1], reinterpret_cast <void *> (buf), 1) == -1)
	runtimeErrorWithErrno ();
}

void *
AsyncTask::ThreadEntry (void *data)
{
    AsyncTask *task = reinterpret_cast <AsyncTask *> (data);
    task->Task ();
    return NULL;
}
