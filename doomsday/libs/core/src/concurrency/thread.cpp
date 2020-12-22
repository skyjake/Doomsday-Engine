/** @file thread.cpp  Thread.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/thread.h"
#include "de/hash.h"
#include "de/garbage.h"

#include <the_Foundation/thread.h>

#include <condition_variable>

namespace de {

using KnownThreads = LockableT<Hash<iThread *, Thread *>>;
KnownThreads &knownThreads()
{
    static KnownThreads kt;
    return kt;
}

DE_PIMPL(Thread)
{
    tF::ref<iThread> thread;
    std::condition_variable cv;
    volatile bool isFinished = false;

    Impl(Public *i) : Base(i)
    {
        thread = tF::make_ref(new_Thread(runFunc));
        setUserData_Thread(thread, this);
        {
            auto &kt = knownThreads();
            DE_GUARD(kt);
            kt.value.insert(thread, thisPublic);
        }
    }

    ~Impl()
    {
        auto &kt = knownThreads();
        DE_GUARD(kt);
        kt.value.remove(thread);
    }

    static iThreadResult runFunc(iThread *thd)
    {
        Thread::Impl *d = static_cast<Thread::Impl *>(userData_Thread(thd));
        auto &self = d->self();
        self.run();
        d->isFinished = true;
        self.post();
        DE_FOR_OBSERVERS(i, self.audienceForFinished())
        {
            i->threadFinished(self);
        }
        Garbage_ClearForThread();
        return 0;
    }

    DE_PIMPL_AUDIENCE(Finished)
};

DE_AUDIENCE_METHOD(Thread, Finished)

Thread::Thread()
    : d(new Impl(this))
{}

Thread::~Thread()
{
    join();
}

void Thread::setName(const String &name)
{
    setName_Thread(d->thread, name);
}

void Thread::setTerminationEnabled(bool enable)
{
    setTerminationEnabled_Thread(d->thread, enable);
}

void Thread::start()
{
    start_Thread(d->thread);
}

void Thread::join()
{
    join_Thread(d->thread);
}

void Thread::terminate()
{
    // Only possible if termination has been enabled before the thread was started.
    terminate_Thread(d->thread);
}

bool Thread::isRunning() const
{
    return isRunning_Thread(d->thread);
}

bool Thread::isFinished() const
{
    return d->isFinished;
}

bool Thread::isCurrentThread() const
{
    if (d->thread)
    {
        return thrd_current() == id_Thread(d->thread);
    }
    return false;
}
    
void Thread::sleep(TimeSpan span) // static
{
    sleep_Thread(span);
}

Thread *Thread::currentThread() // static
{
    auto &kt = knownThreads();
    DE_GUARD(kt);
    auto found = kt.value.find(current_Thread());
    if (found != kt.value.end()) return found->second;
    return nullptr;
}

} // namespace de
