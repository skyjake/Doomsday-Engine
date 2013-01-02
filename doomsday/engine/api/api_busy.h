/** @file api_busy.h Public API for the Busy Mode.
 * @ingroup core
 *
 * @authors Copyright &copy; 2007-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DOOMSDAY_API_BUSY_H
#define DOOMSDAY_API_BUSY_H

#include "api_base.h"

/// Busy mode worker function.
typedef int (*busyworkerfunc_t) (void* parm);

/// POD structure for defining a task processable in busy mode.
typedef struct {
    busyworkerfunc_t worker; ///< Worker thread that does processing while in busy mode.
    void* workerData; ///< Data context for the worker thread.

    int mode; ///< Busy mode flags @ref busyModeFlags
    const char* name; ///< Optional task name (drawn with the progress bar).

    /// Used with task lists:
    int maxProgress;
    float progressStart;
    float progressEnd;

    // Internal state:
    timespan_t _startTime;
} BusyTask;

DENG_API_TYPEDEF(Busy)
{
    de_api_t api;

    /// @return  @c true if we are currently busy.
    boolean (*Active)(void);

    /// @return  Amount of time we have been busy (if not busy, @c 0).
    timespan_t (*ElapsedTime)(void);

    int (*RunTask)(BusyTask* task);

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
    int (*RunTasks)(BusyTask* tasks, int numTasks);

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
    int (*RunNewTask)(int flags, busyworkerfunc_t worker, void* workerData);

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
    int (*RunNewTaskWithName)(int flags, busyworkerfunc_t worker, void* workerData, const char* taskName);

    /**
     * To be called by the busy worker when it has finished processing, to signal
     * the end of the task.
     */
    void (*WorkerEnd)(void);

    /**
     * To be called by the busy worker to shutdown the engine immediately.
     *
     * @param message  Message to show. Expected to exist until the engine closes.
     */
    void (*WorkerError)(const char* message);
}
DENG_API_T(Busy);

#ifndef DENG_NO_API_MACROS_BUSY
#define BusyMode_Active             _api_Busy.Active
#define BusyMode_ElapsedTime        _api_Busy.ElapsedTime
#define BusyMode_RunTask            _api_Busy.RunTask
#define BusyMode_RunTasks           _api_Busy.RunTasks
#define BusyMode_RunNewTask         _api_Busy.RunNewTask
#define BusyMode_RunNewTaskWithName _api_Busy.RunNewTaskWithName
#define BusyMode_WorkerEnd          _api_Busy.WorkerEnd
#define BusyMode_WorkerError        _api_Busy.WorkerError
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Busy);
#endif

#endif /* DOOMSDAY_API_BUSY_H */
