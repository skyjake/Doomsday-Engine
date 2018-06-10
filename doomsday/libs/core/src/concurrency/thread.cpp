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

#include "de/Thread"

#include <c_plus/thread.h>

namespace de {

DE_PIMPL(Thread)
{
    iThread *thread;
    std::condition_variable cv;

    Impl(Public *i) : Base(i)
    {
        thread = new_Thread(runFunc);
        setUserData_Thread(thread, this);
    }

    ~Impl()
    {
        iRelease(thread);
        thread = nullptr;
    }

    static iThreadResult runFunc(iThread *thd)
    {
        Thread::Impl *d = static_cast<Thread::Impl *>(userData_Thread(thd));
        auto &self = d->self();
        self.run();
        self.post();
        DE_FOR_EACH_OBSERVER(i, self.audienceForFinished())
        {
            i->threadFinished(self);
        }
        return 0;
    }

    DE_PIMPL_AUDIENCE(Finished)
};

DE_AUDIENCE_METHOD(Thread, Finished)

Thread::Thread()
    : d(new Impl(this))
{}

Thread::~Thread()
{}

void Thread::start()
{
    start_Thread(d->thread);
}

void Thread::join()
{
    join_Thread(d->thread);
}

bool Thread::isRunning() const
{
    return isRunning_Thread(d->thread);
}

bool Thread::isFinished() const
{
    return isFinished_Thread(d->thread);
}

void Thread::sleep(const TimeSpan &span) // static
{
    sleep_Thread(span);
}

} // namespace de
