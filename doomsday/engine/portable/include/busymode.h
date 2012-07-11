/**
 * @file busymode.h
 * Busy Mode @ingroup core
 *
 * The Busy Mode is intended for long-running tasks that would otherwise block
 * the main engine (UI) thread. During busy mode, a progress screen is shown by
 * the main thread while a background thread works on a long operation. The
 * normal Doomsday UI cannot be interacted with while the task is running. The
 * busy mode can be configured to show a progress bar, the console output,
 * and/or a description of the task being carried out.
 *
 * @todo Refactor: Remove the busy mode loop to prevent the app UI from
 * freezing while busy mode is running. Instead, busy mode should run in the
 * regular application event loop. During busy mode, the game loop callback
 * should not be called.
 *
 * @authors Copyright &copy; 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_CORE_BUSYMODE_H
#define LIBDENG_CORE_BUSYMODE_H

#include "dd_share.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @return  @c true if we are currently busy.
boolean BusyMode_Active(void);

/// @return  Amount of time we have been busy (if not busy, @c 0).
timespan_t BusyMode_ElapsedTime(void);

/**
 * Process a list of work tasks in Busy Mode, from left to right sequentially.
 * Tasks are worked on one at a time and execution of a task only begins once
 * all earlier tasks have completed.
 *
 * Caller relinquishes ownership of the task list until busy mode completes,
 * (therefore it should NOT be accessed in the worker).
 *
 * @param tasks  List of tasks.
 * @param numTasks  Number of tasks.
 * @return  Return value for the worker(s).
 */
int BusyMode_RunTask(BusyTask* task);
int BusyMode_RunTasks(BusyTask* tasks, int numTasks);

/**
 * Convenient shortcut method for constructing and then running of a single work
 * task in Busy Mode.
 *
 * @param mode          Busy mode flags @ref busyModeFlags
 * @param taskName      Optional task name (drawn with the progress bar).
 * @param worker        Worker thread that does processing while in busy mode.
 * @param workerData    Data context for the worker thread.
 *
 * @return  Return value of the worker.
 */
int BusyMode_RunNewTask(int mode, busyworkerfunc_t worker, void* workerData);
int BusyMode_RunNewTaskWithName(int mode, busyworkerfunc_t worker, void* workerData, const char* taskName);

/**
 * To be called by the busy worker when it has finished processing, to signal
 * the end of the task.
 */
void BusyMode_WorkerEnd(void);

/**
 * To be called by the busy worker to shutdown the engine immediately.
 *
 * @param message       Message, expected to exist until the engine closes.
 */
void BusyMode_WorkerError(const char* message);

/// Internal to libdeng:

/// @return  @c true if specified thread is the current busy task worker.
boolean BusyMode_IsWorkerThread(uint threadId);

/// @return  @c true if the current thread is the busy task worker.
boolean BusyMode_InWorkerThread(void);

/// @return  Current busy task, else @c NULL.
BusyTask* BusyMode_CurrentTask(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_CORE_BUSYMODE_H
