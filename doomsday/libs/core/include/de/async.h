/** @file asynctask.h  Asynchoronous task with a completion callback.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ASYNCTASK_H
#define LIBCORE_ASYNCTASK_H

#include "de/app.h"
#include "de/deletable.h"
#include "de/loop.h"
#include "de/string.h"
#include "de/thread.h"
#include "de/garbage.h"

#include <atomic>
#include <utility>

namespace de {

struct DE_PUBLIC AsyncTask : public Deletable, public Thread
{
    virtual ~AsyncTask() {}
    virtual void abort() = 0;
    virtual void invalidate() = 0;
};

namespace internal {

template <typename Task, typename Completion>
class AsyncTaskThread : public AsyncTask
{
    Task task;
    decltype(task()) result {}; // can't be void
    Completion completion;
    bool valid;

    void run() override
    {
        try
        {
            result = task();
        }
        catch (...)
        {}
        notifyCompletion();
    }

    void notifyCompletion()
    {
        Loop::mainCall([this] ()
        {
            if (valid) completion(result);
            de::trash(this);
        });
    }

    void invalidate() override
    {
        valid = false;
    }

public:
    AsyncTaskThread(Task task, Completion completion)
        : task(std::move(task))
        , completion(std::move(completion))
        , valid(true)
    {}

    AsyncTaskThread(const Task &task)
        : task(task)
        , valid(false)
    {}

    void abort() override
    {
//        terminate();
        warning("AsyncTaskThread requested to terminate (ignoring)");
        notifyCompletion();
    }
};

} // namespace internal

/**
 * Executes an asynchronous callback in a background thread.
 *
 * After the background thread finishes, the result from the callback is passed to
 * another callback that is called in the main thread.
 *
 * Must be called from the main thread.
 *
 * If it is possible that the completion becomes invalid (e.g., the object that
 * started the operation is destroyed), you should use AsyncScope to automatically
 * invalidate the completion callbacks of the started tasks.
 *
 * @param task        Task callback. If an exception is thrown here, it will be
 *                    quietly caught, and the completion callback will be called with
 *                    a default-constructed result value. Note that if you return a
 *                    pointer to an object and intend to pass ownership to the
 *                    completion callback, the object will leak if the completion has
 *                    been invalidated. Therefore, you should always pass ownership via
 *                    std::shared_ptr or other reference-counted type.
 *
 * @param completion  Completion callback to be called in the main thread. Takes one
 *                    argument matching the type of the return value from @a task.
 *
 * @return Background thread object. The thread will delete itself after the completion
 * callback has been called. You can pass this to AsyncScope for keeping track of.
 */
template <typename Task, typename Completion>
AsyncTask *async(Task task, Completion completion)
{
    DE_ASSERT_IN_MAIN_THREAD();
    auto *t = new internal::AsyncTaskThread<Task, Completion>(std::move(task), std::move(completion));
    t->start();
    // Note: The thread will delete itself when finished.
    return t;
}

/*template <typename Task>
AsyncTask *async(const Task &task)
{
    auto *t = new internal::AsyncTaskThread<Task, void *>(task);
    t->start();
    // Note: The thread will delete itself when finished.
    return t;
}*/

/**
 * Utility for invalidating the completion callbacks of async tasks whose initiator
 * has gone out of scope.
 */
class DE_PUBLIC AsyncScope : DE_OBSERVES(Thread, Finished), DE_OBSERVES(Deletable, Deletion)
{
public:
    AsyncScope() = default;
    ~AsyncScope() override;

    AsyncScope &operator+=(AsyncTask *task);
    bool isAsyncFinished() const;
    void waitForFinished(TimeSpan timeout = 0.0);
    void threadFinished(Thread &) override;
    void objectWasDeleted(Deletable *) override;

private:
    LockableT<Set<AsyncTask *>> _tasks;
};

} // namespace de

#endif // LIBCORE_ASYNCTASK_H
