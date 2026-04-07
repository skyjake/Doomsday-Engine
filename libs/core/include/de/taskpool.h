/** @file taskpool.h  Pool of tasks.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_TASKPOOL_H
#define LIBCORE_TASKPOOL_H

#include "de/observers.h"
#include "de/time.h"
#include "de/variant.h"

#include <functional>

namespace de {

class Task;

/**
 * Pool of concurrent tasks. @ingroup concurrency
 *
 * The application uses a single, shared pool of background threads regardless
 * of how many instances of TaskPool are created. One should use a separate
 * TaskPool instance for each group of concurrent tasks whose state needs to be
 * observed as a whole.
 *
 * While TaskPool allows the user to monitor whether all tasks are done and
 * block until that time arrives (TaskPool::waitForDone()), no facilities are
 * provided for interrupting any of the started tasks. If that is required, the
 * Task instances in question should periodically check for an abort condition
 * and shut themselves down nicely when requested.
 *
 * A Task is considered done/finished when it has exited its Task::runTask() method.
 */
class DE_PUBLIC TaskPool
{
public:
    enum Priority
    {
        LowPriority    = 0,
        MediumPriority = 1,
        HighPriority   = 2
    };

    class IPool : public Lockable
    {
    public:
        virtual void taskFinishedRunning(Task &) = 0;
        virtual ~IPool() = default;
    };

    typedef std::function<void ()> TaskFunction;

    DE_AUDIENCE(Done, void taskPoolDone(TaskPool &))

public:
    TaskPool();

    /**
     * Destroys the task pool when all running tasks have finished. This method will
     * always return immediately and the public-facing TaskPool object will be deleted,
     * but the private instance will exist until all the tasks have finished running.
     * Completion callbacks will not be called for any pending async tasks.
     */
    virtual ~TaskPool();

    /**
     * Starts a new concurrent task. Ownership of the task is given to the
     * pool.
     *
     * @param task      Task instance. Ownership given.
     * @param priority  Priority of the task.
     */
    void start(Task *task, Priority priority = LowPriority);

    void start(TaskFunction taskFunction, Priority priority = LowPriority);

    /**
     * Starts an asynchronous operation in a background thread and calls a completion
     * callback in the main thread once the operation is complete.
     *
     * @param asyncWork  Work to do.
     * @param completionInMainThread  Completion callback. Receives the Value returned from the
     *                   the async work function. Always runs in the main thread.
     */
    void async(const std::function<Variant()> &asyncWork,
               const std::function<void(const Variant &)> &completionInMainThread);

    /**
     * Blocks execution until all running tasks have finished. A Task is considered
     * finished when it has exited its Task::runTask() method.
     */
    void waitForDone();

    /**
     * Determines if all started tasks have finished.
     */
    bool isDone() const;

    /**
     * Use the calling thread to perform queued tasks in any task pool.
     *
     * You should call this instead of sleeping in tasks, so that global thread pool doesn't get
     * blocked by sleeping workers.
     *
     * @param timeout  How long to wait until queued tasks are available. Zero means to
     *                 wait indefinitely.
     */
    static void yield(const TimeSpan timeout);

    /**
     * Called by de::App at shutdown.
     */
    static void deleteThreadPool();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_TASKPOOL_H
