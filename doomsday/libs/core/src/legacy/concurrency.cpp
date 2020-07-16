/**
 * @file concurrency.cpp
 * Concurrency: threads, mutexes, semaphores.
 *
 * @authors Copyright © 2003-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de/legacy/concurrency.h"
#include "de/app.h"
#include "de/time.h"
#include "de/log.h"
#include "de/loop.h"
#include "de/garbage.h"
#include <assert.h>

CallbackThread::CallbackThread(systhreadfunc_t func, void *param)
    : _callback(std::move(func))
    , _parm(param)
    , _returnValue(0)
    , _exitStatus(DE_THREAD_STOPPED_NORMALLY)
    , _terminationFunc(nullptr)
{
    //qDebug() << "CallbackThread:" << this << "created.";

    // Only used if the thread needs to be shut down forcibly.
    setTerminationEnabled(true);

    // Cleanup at app exit time for threads whose exit value hasn't been checked.
//    audienceForFinished() += this;
}

CallbackThread::~CallbackThread()
{
    if (isRunning())
    {
        DE_ASSERT(!isCurrentThread());
        de::debug("CallbackThread %p being forcibly stopped before deletion", this);
        terminate();
//        wait(1000);
    }
    else
    {
        //qDebug() << "CallbackThread:" << this << "deleted.";
    }
}

//void CallbackThread::threadFinished(Thread &)
//{
//    using namespace de;
//    Loop::mainCall([this]() {
//        DE_ASSERT(!isCurrentThread());
//        trash(this);
//    });
//}

void CallbackThread::run()
{
    _exitStatus = DE_THREAD_STOPPED_WITH_FORCE;

    try
    {
        if (_callback)
        {
            _returnValue = _callback(_parm);
        }
        _exitStatus = DE_THREAD_STOPPED_NORMALLY;
    }
    catch (const std::exception &error)
    {
        LOG_AS("CallbackThread");
        LOG_ERROR("Uncaught exception: ") << error.what();
        _returnValue = -1;
        _exitStatus = DE_THREAD_STOPPED_WITH_EXCEPTION;
    }

    if (_terminationFunc)
    {
        _terminationFunc(_exitStatus);
    }

    Garbage_ClearForThread();
}

int CallbackThread::exitValue() const
{
    return _returnValue;
}

systhreadexitstatus_t CallbackThread::exitStatus() const
{
    return _exitStatus;
}

void CallbackThread::setTerminationFunc(void (*func)(systhreadexitstatus_t))
{
    _terminationFunc = func;
}

/*void Sys_MarkAsMainThread(void)
{
    // This is the main thread.
    mainThreadId = Sys_CurrentThreadId();
}*/

dd_bool Sys_InMainThread(void)
{
    return de::App::inMainThread();
}

void Thread_Sleep(int milliseconds)
{
    de::TimeSpan::fromMilliSeconds(milliseconds).sleep();
}

thread_t Sys_StartThread(systhreadfunc_t startpos, void *parm, void (*terminationFunc)(systhreadexitstatus_t))
{
    auto *t = new CallbackThread(std::move(startpos), parm);
    t->setTerminationFunc(terminationFunc);
    t->start();
    return t;
}

thread_t Sys_StartThread(int (*startpos)(void *), void *parm, void (*terminationFunc)(systhreadexitstatus_t))
{
    return Sys_StartThread(systhreadfunc_t(startpos), parm, terminationFunc);
}

void Thread_KillAbnormally(thread_t handle)
{
    auto *t = reinterpret_cast<de::Thread *>(handle);
    if (!handle)
    {
        t = de::Thread::currentThread();
    }
    DE_ASSERT(t);
    t->terminate();
    de::trash(t);
}

void Thread_SetCallback(thread_t thread, void (*terminationFunc)(systhreadexitstatus_t))
{
    auto *t = reinterpret_cast<CallbackThread *>(thread);
    DE_ASSERT(t);
    if (t)
    {
        t->setTerminationFunc(terminationFunc);
    }
}

int Sys_WaitThread(thread_t handle, int timeoutMs, systhreadexitstatus_t *exitStatus)
{
    if (!handle)
    {
        if (exitStatus) *exitStatus = DE_THREAD_STOPPED_NORMALLY;
        return 0;
    }

    auto *t = reinterpret_cast<CallbackThread *>(handle);
    DE_ASSERT(!t->isCurrentThread());
    t->tryWait(de::TimeSpan::fromMilliSeconds(timeoutMs));
    if (t->isFinished())
    {
        if (exitStatus) *exitStatus = t->exitStatus();
    }
    else
    {
        LOG_WARNING("Thread did not stop in time, forcibly killing it.");
        if (exitStatus) *exitStatus = DE_THREAD_STOPPED_WITH_FORCE;
    }
    de::trash(t);
    return t->exitValue();
}

uint32_t Sys_ThreadId(thread_t handle)
{
    auto *t = reinterpret_cast<de::Thread *>(handle);
    if (!t) t = de::Thread::currentThread();
    return uint32_t(PTR2INT(t));
}

uint32_t Sys_CurrentThreadId(void)
{
    return Sys_ThreadId(nullptr /*this thread*/);
}

/// @todo remove the name parameter
mutex_t Sys_CreateMutex(const char *)
{
    return new std::recursive_mutex;
}

void Sys_DestroyMutex(mutex_t handle)
{
    if (handle)
    {
        delete reinterpret_cast<std::recursive_mutex *>(handle);
    }
}

void Sys_Lock(mutex_t handle)
{
    auto *m = reinterpret_cast<std::recursive_mutex *>(handle);
    DE_ASSERT(m != nullptr);
    if (m)
    {
        m->lock();
    }
}

void Sys_Unlock(mutex_t handle)
{
    auto *m = reinterpret_cast<std::recursive_mutex *>(handle);
    DE_ASSERT(m != nullptr);
    if (m)
    {
        m->unlock();
    }
}
