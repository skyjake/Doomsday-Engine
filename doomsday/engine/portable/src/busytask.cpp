/**
 * @file busytask.cpp
 * Busy task. @ingroup console
 *
 * @todo Complete refactoring con_busy.c into a BusyTask class. Add a dd_init.h
 * for accessing the legacy core.
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "con_busy.h"
#include "de/c_wrapper.h"

#include <QEventLoop>

extern "C" LegacyCore* de2LegacyCore; // from dd_init.cpp

static QEventLoop* eventLoop;

/**
 * Runs the busy mode event loop. Execution blocks here until the worker thread
 * exits.
 *
 * @param mode          Busy mode flags @ref busyModeFlags
 * @param taskName      Optional task name (drawn with the progress bar).
 * @param worker        Worker thread that does processing while in busy mode.
 * @param workerData    Data context for the worker thread.
 */
int BusyTask_Run(int mode, const char* taskName, busyworkerfunc_t worker, void* workerData)
{
    char* name = NULL;
    BusyTask* task = (BusyTask*) calloc(1, sizeof(*task));

    // Take a copy of the task name.
    if(taskName && taskName[0])
    {
        name = strdup(taskName);
    }

    // Initialize the task.
    task->worker = worker;
    task->workerData = workerData;
    task->name = name;
    task->mode = mode;

    // Let's get busy!
    BusyTask_Begin(task);

    Q_ASSERT(eventLoop == 0);

    // Run a local event loop since the primary event loop is blocked while
    // we're busy. This event loop is able to handle window and input events
    // just like the primary loop.
    eventLoop = new QEventLoop;
    int result = eventLoop->exec();
    delete eventLoop;
    eventLoop = 0;

    // Teardown.
    BusyTask_End(task);
    if(name) free(name);
    free(task);

    return result;
}

/**
 * Ends the busy event loop and sets its return value. The loop callback, which
 * during busy mode points to the busy loop callback, is reset to NULL.
 */
void BusyTask_StopEventLoopWithValue(int result)
{
    // After the event loop is gone, we don't want any loop callbacks until the
    // busy state has been properly torn down.
    LegacyCore_SetLoopFunc(de2LegacyCore, 0);

    Q_ASSERT(eventLoop != 0);
    eventLoop->exit(result);
}
