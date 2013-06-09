/** @file taskpool.h  Pool of tasks.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_TASKPOOL_H
#define LIBDENG2_TASKPOOL_H

#include <QObject>

#include "../libdeng2.h"

namespace de {

class Task;

/**
 * Pool of concurrent tasks.
 *
 * While TaskPool allows the user to monitor whether all tasks are done and
 * block until that time arrives (TaskPool::waitForDone()), no facilities are
 * provided for interrupting any of the started tasks. If that is required, the
 * Task instances in question should periodically check for an abort condition
 * and shut themselves down nicely when requested.
 */
class DENG2_PUBLIC TaskPool : public QObject
{
    Q_OBJECT

public:
    enum Priority
    {
        LowPriority    = 0,
        MediumPriority = 1,
        HighPriority   = 2
    };

public:
    TaskPool();

    virtual ~TaskPool();

    /**
     * Starts a new concurrent task. Ownership of the task is given to the
     * pool.
     *
     * @param task      Task instance. Ownership given.
     * @param priority  Priority of the task.
     */
    void start(Task *task, Priority priority = LowPriority);

    /**
     * Blocks execution until all running tasks have finished.
     */
    void waitForDone();

    /**
     * Determines if all started tasks have finished.
     */
    bool isDone() const;

signals:
    void allTasksDone();

private:
    friend class Task;
    void taskFinished(Task &task);

    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_TASKPOOL_H
