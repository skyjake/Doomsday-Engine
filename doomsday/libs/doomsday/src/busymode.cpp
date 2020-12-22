/** @file busymode.cpp  Background task runner.
 *
 * @authors Copyright © 2007-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/busymode.h"
#include "doomsday/doomsdayapp.h"

#include <de/legacy/types.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/memory.h>
#include <de/legacy/timer.h>
#include <de/lockable.h>
#include <de/log.h>

using namespace de;

DE_PIMPL(BusyMode)
, public Lockable
{
    ITaskRunner *runner = nullptr;

    bool busyInited = false;

    BusyTask *busyTask = nullptr; // Current task.
    bool busyTaskEndedWithError = false;
    String busyError;
    Time taskStartedAt;

    Impl(Public *i) : Base(i) {}

    int performTask(BusyTask *task)
    {
        DE_ASSERT(task);
        if (!task) return 0;

        DE_ASSERT(!busyInited);

        busyTask = task;
        busyTaskEndedWithError = false;
        taskStartedAt = Time();
        busyInited = true;

        ITaskRunner::Result result;
        if (runner)
        {
            result = runner->runTask(task);
        }
        if (!result.wasRun)
        {
            // As a fallback, just run the callback.
            result.returnValue = task->worker(task->workerData);
            result.wasRun = true;
        }

        // Clean up.
        busyInited = false;
        if (task->name)
        {
            LOG_VERBOSE("Busy task \"%s\" performed in %.2f seconds")
                    << task->name
                    << taskStartedAt.since();
        }
        if (busyTaskEndedWithError)
        {
            throw Error("BusyMode::performTask", "Task failed: " + busyError);
        }
        busyTask = nullptr;

        return result.returnValue;
    }

    DE_PIMPL_AUDIENCES(Beginning, End, Abort, TaskWillStart, TaskComplete)
};

DE_AUDIENCE_METHODS(BusyMode, Beginning, End, Abort, TaskWillStart, TaskComplete)

BusyMode::BusyMode() : d(new Impl(this))
{}

void BusyMode::setTaskRunner(ITaskRunner *runner)
{
    d->runner = runner;
}

BusyMode::ITaskRunner *BusyMode::taskRunner() const
{
    return d->runner;
}

bool BusyMode::isActive() const
{
    return d->busyInited;
}

bool BusyMode::endedWithError() const
{
    return d->busyTaskEndedWithError;
}

BusyTask *BusyMode::currentTask() const
{
    DE_GUARD(d)

    if (!isActive()) return nullptr;
    return d->busyTask;
}

void BusyMode::abort(const String &message)
{
    DE_GUARD(d)

    d->busyTaskEndedWithError = true;
    d->busyError = message;

    DE_NOTIFY(Abort, i)
    {
        i->busyModeAborted(message);
    }
}

/**
 * @param mode          Busy mode flags @ref busyModeFlags
 * @param worker        Worker thread that does processing while in busy mode.
 * @param workerData    Data context for the worker thread.
 * @param taskName      Optional task name (drawn with the progress bar).
 */
static BusyTask *newTask(int                               mode,
                         const std::function<int(void *)> &worker,
                         void *                            workerData,
                         const String &                    taskName)
{
    BusyTask *task = new BusyTask;
    zapPtr(task);

    // Initialize the task.
    task->mode = mode;
    task->worker = worker;
    task->workerData = workerData;

    // Take a copy of the task name.
    if (!taskName.isEmpty())
    {
        task->name = M_StrDup(taskName.c_str());
    }

    return task;
}

static void deleteTask(BusyTask *task)
{
    DE_ASSERT(task);
    if (task->name) M_Free((void *) task->name);
    delete task;
}

int BusyMode::runNewTaskWithName(int mode, const String &taskName, const std::function<int (void *)>& worker)
{
    BusyTask *task = newTask(mode, worker, nullptr, taskName);
    int result = runTask(task);
    deleteTask(task);
    return result;
}

int BusyMode::runNewTaskWithName(int mode, busyworkerfunc_t worker, void *workerData,
                                 const String &taskName)
{
    BusyTask *task = newTask(mode, worker, workerData, taskName);
    int result = runTask(task);
    deleteTask(task);
    return result;
}

int BusyMode::runNewTask(int mode, busyworkerfunc_t worker, void *workerData)
{
    return runNewTaskWithName(mode, worker, workerData, "" /*no name*/);
}

int BusyMode::runTask(BusyTask *task)
{
    return runTasks(task, 1);
}

int BusyMode::runTasks(BusyTask *tasks, int numTasks)
{
    String currentTaskName;
    BusyTask *task;
    int i, mode;
    int result = 0;
    Time startedAt;

    DE_ASSERT(!isActive());

    if (!tasks || numTasks <= 0) return result; // Hmm, no work?

    // Pick the first task.
    task = tasks;

    DE_NOTIFY(Beginning, i)
    {
        i->busyModeWillBegin(*task);
    }

    // Process tasks.
    for (i = 0; i < numTasks; ++i, ++task)
    {
        // If no new task name is specified, continue using the name of the previous task.
        if (task->name)
        {
            currentTaskName = task->name;
        }

        mode = task->mode;

        /*
        /// @todo Kludge: Force BUSYF_STARTUP here so that the animation of one task
        ///       is not drawn on top of the last frame of the previous.
        if (numTasks > 1)
        {
            mode |= BUSYF_STARTUP;
        }
        // kludge end
        */

        // Null tasks are not processed (implicit success).
        if (!task->worker) continue;

        /**
         * Process the work.
         */
        DE_NOTIFY(TaskWillStart, i)
        {
            i->busyTaskWillStart(*task);
        }

        // Invoke the worker in a new thread.
        /// @todo Kludge: Presently a temporary local task is needed so that we can modify
        ///       the task name and mode flags.
        BusyTask *tmp = newTask(mode, task->worker, task->workerData, currentTaskName);
        result = d->performTask(tmp);
        deleteTask(tmp);
        tmp = nullptr;

        DE_NOTIFY(TaskComplete, i)
        {
            i->busyTaskCompleted(*task);
        }

        if (result) break;
    }

    DE_NOTIFY(End, i)
    {
        i->busyModeEnded();
    }

    LOG_VERBOSE("Busy mode lasted %.2f seconds") << startedAt.since();

    return result;
}

bool BusyMode_Active()
{
    return DoomsdayApp::app().busyMode().isActive();
}

int BusyMode_RunTask(BusyTask *task)
{
    return DoomsdayApp::app().busyMode().runTask(task);
}

int BusyMode_RunTasks(BusyTask *task, int numTasks)
{
    return DoomsdayApp::app().busyMode().runTasks(task, numTasks);
}

int BusyMode_RunNewTask(int flags, busyworkerfunc_t worker, void *workerData)
{
    return DoomsdayApp::app().busyMode().runNewTask(flags, worker, workerData);
}

int BusyMode_RunNewTaskWithName(int flags, busyworkerfunc_t worker, void *workerData, const char *taskName)
{
    return DoomsdayApp::app().busyMode()
            .runNewTaskWithName(flags, worker, workerData, taskName? taskName : "");
}
