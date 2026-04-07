/** @file doomsday/busymode.h  Background task runner.
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

#ifndef LIBDOOMSDAY_BUSYMODE_H
#define LIBDOOMSDAY_BUSYMODE_H

#include "libdoomsday.h"
#include <de/observers.h>
#include <de/time.h>
#include <de/string.h>
#include <functional>

/// Busy mode worker function.
typedef int (*busyworkerfunc_t) (void *parm);

/// Defines a task processable in busy mode.
struct LIBDOOMSDAY_PUBLIC BusyTask
{
    std::function<int (void *)> worker; ///< Worker thread that does processing while in busy mode.
    void *workerData; ///< Data context for the worker thread.

    int mode; ///< Busy mode flags @ref busyModeFlags
    const char *name; ///< Optional task name (drawn with the progress bar).

    /// Used with task lists:
    int maxProgress;
    float progressStart;
    float progressEnd;
};

/**
 * Runs tasks in the background sequentially.
 */
class LIBDOOMSDAY_PUBLIC BusyMode
{
public:
    /**
     * Interface for an object responsible for running tasks. By default, BusyMode
     * simply calls the worker function synchronously. A task runner could instead
     * start a background thread for the task, for example.
     */
    class LIBDOOMSDAY_PUBLIC ITaskRunner
    {
    public:
        virtual ~ITaskRunner() = default;

        struct LIBDOOMSDAY_PUBLIC Result {
            bool wasRun;
            int returnValue;

            Result(bool taskWasRun = false, int result = 0)
                : wasRun(taskWasRun)
                , returnValue(result) {}
        };

        virtual Result runTask(BusyTask *task) = 0;
    };

public:
    BusyMode();

    void setTaskRunner(ITaskRunner *runner);
    ITaskRunner *taskRunner() const;

    bool isActive() const;
    bool endedWithError() const;
    BusyTask *currentTask() const;

    int runTask(BusyTask *task);

    /**
     * Process a list of work tasks in Busy Mode, from left to right sequentially.
     * Tasks are worked on one at a time and execution of a task only begins once
     * all earlier tasks have completed.
     *
     * Caller relinquishes ownership of the task list until busy mode completes,
     * (therefore it should NOT be accessed in the worker).
     *
     * @param tasks     List of tasks.
     * @param numTasks  Number of tasks.
     *
     * @return  Return value for the worker(s).
     */
    int runTasks(BusyTask *tasks, int numTasks);

    /**
     * Convenient shortcut method for constructing and then running of a single work
     * task in Busy Mode.
     *
     * @param flags         Busy mode flags @ref busyModeFlags
     * @param worker        Worker thread that does processing while in busy mode.
     * @param workerData    Data context for the worker thread.
     *
     * @return  Return value of the worker.
     */
    int runNewTask(int mode, busyworkerfunc_t worker, void *workerData);

    /**
     * Convenient shortcut method for constructing and then running of a single work
     * task in Busy Mode.
     *
     * @param flags         Busy mode flags @ref busyModeFlags
     * @param worker        Worker thread that does processing while in busy mode.
     * @param workerData    Data context for the worker thread.
     * @param taskName      Optional task name (drawn with the progress bar).
     *
     * @return  Return value of the worker.
     */
    int runNewTaskWithName(int mode, busyworkerfunc_t worker, void *workerData, const de::String &taskName);

    /**
     * Run a single task with a std::function callback.
     *
     * @param mode      Busy mode flags.
     * @param worker    Worker callback.
     * @param taskName  Task name (drawn next to the progress bar).
     *
     * @return  Return value from the worker.
     */
    int runNewTaskWithName(int mode, const de::String &taskName, const std::function<int (void *)>& worker);

    /**
     * Abnormally aborts the currently running task. Call this when the task encounters
     * an unrecoverable error. Calling this causes the Abort audience to be notified.
     * Busy mode is stopped as soon as possible, and an exception is raised with the
     * error message.
     *
     * @param message  Error message to be presented to the user.
     */
    void abort(const de::String &message);

public:
    DE_AUDIENCE(Beginning, void busyModeWillBegin(BusyTask &firstTask))
    DE_AUDIENCE(End,       void busyModeEnded())
    DE_AUDIENCE(Abort,     void busyModeAborted(const de::String &message))
    DE_AUDIENCE(TaskWillStart, void busyTaskWillStart(BusyTask &task))
    DE_AUDIENCE(TaskComplete, void busyTaskCompleted(BusyTask &task))

private:
    DE_PRIVATE(d)
};

LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC bool BusyMode_Active();
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC int  BusyMode_RunTask(BusyTask *task);
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC int  BusyMode_RunTasks(BusyTask *task, int numTasks);
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC int  BusyMode_RunNewTask(int flags, busyworkerfunc_t worker, void *workerData);
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC int  BusyMode_RunNewTaskWithName(int flags, busyworkerfunc_t worker, void *workerData, const char *taskName);

#endif // LIBDOOMSDAY_BUSYMODE_H

