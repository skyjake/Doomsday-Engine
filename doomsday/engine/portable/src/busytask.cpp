/**
 * @file busytask.cpp
 * Busy task. @ingroup console
 *
 * @todo Complete refactoring con_busy.c into a BusyTask class.
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

#include "con_busy.h"

#include <QEventLoop>

static QEventLoop* eventLoop;

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

    // Run a local event loop since the primary event loop is blocked while we're busy.
    eventLoop = new QEventLoop;
    int result = eventLoop->exec();

    // Busy mode has ended.
    if(name) free(name);
    free(task);

    return result;
}

void BusyTask_ReturnValue(int result)
{
    Q_ASSERT(eventLoop != 0);
    eventLoop->exit(result);
}
