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

#include "de/TaskPool"
#include "de/Task"
#include "de/Guard"
#include "de/Set"

#include <de/Lockable>
#include <de/Loop>
#include <de/Waitable>

#include <the_Foundation/threadpool.h>

namespace de {
namespace internal {

static void deleteThreadPool();

static iThreadPool *globalThreadPool()
{
    static iThreadPool *pool = nullptr;
    if (!pool)
    {
        pool = new_ThreadPool();
        atexit(deleteThreadPool);
    }
    return pool;
}

static void deleteThreadPool()
{
    iRelease(globalThreadPool());
}

class CallbackTask : public Task
{
public:
    CallbackTask(TaskPool::TaskFunction func) : _func(func) {}
    void runTask() override { _func(); }
private:
    TaskPool::TaskFunction _func;
};

} // namespace internal

DE_PIMPL(TaskPool), public Lockable, public Waitable, public TaskPool::IPool
{
    /// Private instance will be deleted when pool is empty.
    bool deleteWhenDone = false;

    /// Set of running tasks.
    Set<Task *> tasks;

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

    void taskFinishedRunning(Task &task)
    {
        lock();
        if (remove(&task))
        {
            if (deleteWhenDone)
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
                    DE_FOR_AUDIENCE(Done, i) i->taskPoolDone(self());
                }
                catch (Error const &er)
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
        d.release()->deleteWhenDone = true;
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
    d->waitForEmpty();
}

bool TaskPool::isDone() const
{
    return d->isEmpty();
}

} // namespace de
