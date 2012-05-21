/**
 * @file concurrency.cpp
 * Brief description of the source file. @ingroup system
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "concurrency.h"
#include "garbage.h"
#include "dd_types.h"
#include <QMutex>
#include <QApplication>
#include <QDebug>
#include <de/Time>
#include <de/Log>
#include <assert.h>

static uint mainThreadId = 0; ///< ID of the main thread.

CallbackThread::CallbackThread(systhreadfunc_t func, void* param)
    : _callback(func), _parm(param), _returnValue(0)
{
    //qDebug() << "CallbackThread:" << this << "created.";

    // Only used if the thread needs to be shut down forcibly.
    setTerminationEnabled(true);

    // Cleanup at app exit time for threads whose exit value hasn't been checked.
    connect(qApp, SIGNAL(destroyed()), this, SLOT(deleteNow()));
}

CallbackThread::~CallbackThread()
{
    if(isRunning())
    {
        //qDebug() << "CallbackThread:" << this << "forcibly stopping, deleting.";
        terminate();
        wait(1000);
    }
    else
    {
        //qDebug() << "CallbackThread:" << this << "deleted.";
    }
}

void CallbackThread::deleteNow()
{
    delete this;
}

void CallbackThread::run()
{
    try
    {
        if(_callback)
        {
            _returnValue = _callback(_parm);
        }
    }
    catch(const std::exception& error)
    {
        LOG_AS("CallbackThread");
        LOG_ERROR(QString("Uncaught exception: ") + error.what());
        _returnValue = -1;
    }

    Garbage_ClearForThread();
}

int CallbackThread::exitValue() const
{
    return _returnValue;
}

void Sys_MarkAsMainThread(void)
{
    // This is the main thread.
    mainThreadId = Sys_CurrentThreadId();
}

boolean Sys_InMainThread(void)
{
    return mainThreadId == Sys_CurrentThreadId();
}

void Thread_Sleep(int milliseconds)
{
    de::Time::Delta::fromMilliSeconds(milliseconds).sleep();
}

thread_t Sys_StartThread(systhreadfunc_t startpos, void *parm)
{
    CallbackThread* t = new CallbackThread(startpos, parm);
    t->start();
    return t;
}

void Thread_KillAbnormally(thread_t handle)
{
    QThread* t = reinterpret_cast<QThread*>(handle);
    if(!handle)
    {
        t = QThread::currentThread();
    }
    assert(t);
    t->terminate();
}

int Sys_WaitThread(thread_t handle, int timeoutMs)
{
    CallbackThread* t = reinterpret_cast<CallbackThread*>(handle);
    assert(static_cast<QThread*>(t) != QThread::currentThread());
    t->wait(timeoutMs);
    if(!t->isFinished())
    {
        LOG_WARNING("Thread did not stop in time, forcibly killing it.");
    }
    t->deleteLater(); // get rid of it
    return t->exitValue();
}

uint Sys_ThreadId(thread_t handle)
{
    QThread* t = reinterpret_cast<QThread*>(handle);
    if(!t) t = QThread::currentThread();
    return uint(PTR2INT(t));
}

uint Sys_CurrentThreadId(void)
{
    return Sys_ThreadId(NULL /*this thread*/);
}

/// @todo remove the name parameter
mutex_t Sys_CreateMutex(const char*)
{
    return new QMutex(QMutex::Recursive);
}

void Sys_DestroyMutex(mutex_t handle)
{
    if(handle)
    {
        delete reinterpret_cast<QMutex*>(handle);
    }
}

void Sys_Lock(mutex_t handle)
{
    QMutex* m = reinterpret_cast<QMutex*>(handle);
    assert(m != 0);
    if(m)
    {
        m->lock();
    }
}

void Sys_Unlock(mutex_t handle)
{
    QMutex* m = reinterpret_cast<QMutex*>(handle);
    assert(m != 0);
    if(m)
    {
        m->unlock();
    }
}

#if 0
sem_t Sem_Create(uint32_t initialValue)
{
    return (sem_t) SDL_CreateSemaphore(initialValue);
}

void Sem_Destroy(sem_t semaphore)
{
    if(semaphore)
    {
        SDL_DestroySemaphore((SDL_sem *) semaphore);
    }
}

void Sem_P(sem_t semaphore)
{
    if(semaphore)
    {
        SDL_SemWait((SDL_sem *) semaphore);
    }
}

void Sem_V(sem_t semaphore)
{
    if(semaphore)
    {
        SDL_SemPost((SDL_sem *) semaphore);
    }
}
#endif
