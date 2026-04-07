/** @file taskpool.cpp
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

#include "de/taskpool.h"
#include "de/task.h"
#include "de/guard.h"
#include "de/set.h"
#include "de/app.h"
#include "de/garbage.h"
#include "de/lockable.h"
#include "de/loop.h"
#include "de/waitable.h"

#include <the_Foundation/threadpool.h>

namespace de {
namespace internal {

static iThreadPool *s_pool = nullptr;

static iThreadPool *globalThreadPool()
{
    if (!s_pool)
    {
        /*
         * The application is assumed to need a few CPU cores for:
         *  - rendering/input/UI (main thread; most time-consuming)
         *  - audio: continuous buffer mixing, streaming
         *  - timer: triggering timer events
         *  - networking: receiving and sending data via sockets
         *
         * Always create at least two threads so the pool is useful for running background tasks.
         */
        s_pool = newLimits_ThreadPool(2, 3);
    }
    return s_pool;
}

static void deleteThreadPool()
{
    if (s_pool)
    {
        iRelease(s_pool);
    }
}

class CallbackTask : public Task
{
    TaskPool::TaskFunction _func;

public:
    CallbackTask(TaskPool::TaskFunction func)
        : _func(func)
    {}

    void runTask() override { _func(); }
};

} // namespace internal

DE_PIMPL(TaskPool), public Waitable, public TaskPool::IPool
{
    /// Runs async worker + main-thread-completion tasks.
    class CompletionTask : public Task
    {
        std::function<Variant()> _task;
        struct Epilogue
        {
            std::function<void(const Variant &)> completion;
            Variant result;
        };
        Epilogue *_epilogue;

    public:
        CompletionTask(const std::function<Variant()> &task,
                       const std::function<void(const Variant &)> &completion)
            : _task(task)
            , _epilogue(new Epilogue{completion, {}})
        {}

        ~CompletionTask() override
        {
            // Check that the pool is still valid.
            {
                TaskPool::Impl *d = static_cast<TaskPool::Impl *>(_pool);
                DE_GUARD(d);
                if (d->poolDestroyed)
                {
                    // Completion cannot be called now.
                    delete _epilogue;
                    return;
                }
            }
            Epilogue *e = _epilogue; // deleted after the callback is done
            Loop::mainCall([e]() {
                e->completion(e->result);
                delete e;
            });
        }

        void runTask() override
        {
            _epilogue->result = _task();
        }
    };

    bool        poolDestroyed = false; // Impl will be deleted when pool is empty.
    Set<Task *> tasks;                 // Set of running tasks.

    Impl(Public *i) : Base(i)
    {
        // When empty, the semaphore is available.
        post();
    }

    ~Impl()
    {
        // The pool is always empty at this point because the destructor is not
        // called until all the tasks have been finished and removed.
        DE_ASSERT(tasks.isEmpty());
    }

    void add(Task *t)
    {
        DE_GUARD(this);
        t->_pool = this;
        if (tasks.isEmpty())
        {
            wait(); // Semaphore now unavailable.
        }
        tasks.insert(t);
    }

    /// Returns @c true, if the pool became empty as result of the remove.
    bool remove(Task *t)
    {
        DE_GUARD(this);
        tasks.remove(t);
        if (tasks.isEmpty())
        {
            post();
            return true;
        }
        return false;
    }

    void waitForEmpty() const
    {
        wait();
        DE_GUARD(this);
        DE_ASSERT(tasks.isEmpty());
        post(); // When empty, the semaphore is available.
    }

    bool isEmpty() const
    {
        DE_GUARD(this);
        return tasks.isEmpty();
    }

    void taskFinishedRunning(Task &finishedTask)
    {
        std::unique_ptr<Task> task(&finishedTask); // We have ownership of this.

        lock();
        if (remove(&finishedTask))
        {
            if (poolDestroyed)
            {
                // All done, clean up!
                unlock();

                // NOTE: Guard isn't used because the object doesn't exist past this point.
                delete this;
                return;
            }
            else
            {
                try
                {
//                    emit self().allTasksDone();
                    DE_NOTIFY_VAR(Done, i) i->taskPoolDone(self());
                }
                catch (const Error &er)
                {
                    unlock();
                    throw er;
                }
            }
        }
        unlock();
    }

    DE_PIMPL_AUDIENCE(Done)
};

DE_AUDIENCE_METHOD(TaskPool, Done)

TaskPool::TaskPool() : d(new Impl(this))
{}

TaskPool::~TaskPool()
{
    DE_GUARD(d);
    if (!d->isEmpty())
    {
        // Detach the private instance and make it auto-delete itself when done.
        // The ongoing tasks will report themselves finished to the private instance.
        d.release()->poolDestroyed = true;
    }
}

static iThreadResult runTask(iThread *thd)
{
    Task *task = static_cast<Task *>(userData_Thread(thd));
    task->run();
    iRelease(thd);
    return 0;
}

void TaskPool::start(Task *task, Priority priority)
{
    d->add(task);

    iThread *thd = new_Thread(runTask);
    setUserData_Thread(thd, task);
    run_ThreadPool(internal::globalThreadPool(), thd);

    DE_UNUSED(priority);
}

void TaskPool::start(TaskFunction taskFunction, Priority priority)
{
    start(new internal::CallbackTask(taskFunction), priority);
}

void TaskPool::waitForDone()
{
    bool allowSleep = true;

    if (const iThread *cur = current_Thread())
    {
        if (!cmp_String(name_Thread(cur), "PooledThread"))
        {
            // Pooled threads cannot sleep or otherwise the thread pool would likely
            // block, if too many / all workers are sleeping.
            allowSleep = false;
        }
    }

    if (allowSleep)
    {
        // This thread will block here until tasks are complete.
        d->waitForEmpty();
    }
    else
    {
        // Allow the thread pool to execute other tasks.
        while (!isDone())
        {
            yield(100_ms);
        }
    }
}

bool TaskPool::isDone() const
{
    return d->isEmpty();
}

void TaskPool::deleteThreadPool() // static
{
    internal::deleteThreadPool();
}

void TaskPool::yield(const TimeSpan timeout) // static
{
    yield_ThreadPool(internal::globalThreadPool(), timeout);
}

void TaskPool::async(const std::function<Variant()> &work,
                     const std::function<void (const Variant &)> &completion)
{
    start(new Impl::CompletionTask(work, completion));
}

} // namespace de
