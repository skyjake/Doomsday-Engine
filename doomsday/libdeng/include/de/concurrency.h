/**
 * @file concurrency.h
 * Concurrency: threads, mutexes, semaphores. @ingroup system
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

#ifndef LIBDENG_SYSTEM_CONCURRENCY_H
#define LIBDENG_SYSTEM_CONCURRENCY_H

#include <de/libdeng.h>

typedef void* thread_t;
typedef int (*systhreadfunc_t) (void* parm);

typedef void* mutex_t;
typedef void* sem_t;

typedef enum systhreadexitstatus_e {
    DENG_THREAD_STOPPED_NORMALLY,
    DENG_THREAD_STOPPED_WITH_FORCE, // terminated
    DENG_THREAD_STOPPED_WITH_EXCEPTION
} systhreadexitstatus_t;

#ifdef __cplusplus

#ifdef __DENG__ // libdeng internal
#include <QThread>
/**
 * Thread that runs a user-specified callback function. Exceptions from the callback
 * function are caught.
 */
class CallbackThread : public QThread
{
    Q_OBJECT

public:
    CallbackThread(systhreadfunc_t func, void* parm = 0);
    ~CallbackThread();

    void run();
    int exitValue() const;
    systhreadexitstatus_t exitStatus() const;
    void setTerminationFunc(void (*func)(systhreadexitstatus_t));

protected slots:
    void deleteNow();

private:
    systhreadfunc_t _callback;
    void* _parm;
    int _returnValue;
    systhreadexitstatus_t _exitStatus;
    void (*_terminationFunc)(systhreadexitstatus_t);
};
#endif // __DENG__

extern "C" {
#endif // __cplusplus

/**
 * @def LIBDENG_ASSERT_IN_MAIN_THREAD
 * In a debug build, this asserts that the current code is executing in the main thread.
 */
#ifdef _DEBUG
#  define LIBDENG_ASSERT_IN_MAIN_THREAD() { DENG_ASSERT(Sys_InMainThread()); }
#else
#  define LIBDENG_ASSERT_IN_MAIN_THREAD()
#endif

/**
 * Stars a new thread with the given callback.
 *
 * @param startpos  Executes while the thread is running. When the function exists,
 *                  the thread stops.
 * @param parm      Parameter passed to the callback.
 *
 * @return Thread handle.
 */
DENG_PUBLIC thread_t Sys_StartThread(systhreadfunc_t startpos, void* parm);

DENG_PUBLIC void Thread_Sleep(int milliseconds);

/**
 * Sets a callback function that is called from the worker thread right before
 * it exits. The callback is given the exit status of the thread as a
 * parameter.
 *
 * @param thread           Thread handle.
 * @param terminationFunc  Callback to call before terminating the thread.
 */
DENG_PUBLIC void Thread_SetCallback(thread_t thread, void (*terminationFunc)(systhreadexitstatus_t));

/**
 * Wait for a thread to stop. If the thread does not stop after @a timeoutMs,
 * it will be forcibly terminated.
 *
 * @param handle      Thread handle.
 * @param timeoutMs   How long to wait until the thread terminates.
 * @param exitStatus  If not @c NULL, the exit status is returned here.
 *                    @c true for normal exit, @c false if exception was caught.
 *
 * @return  Return value of the thread.
 */
DENG_PUBLIC int Sys_WaitThread(thread_t handle, int timeoutMs, systhreadexitstatus_t* exitStatus);

/**
 * @param handle  Handle to the thread to return the id of.
 *                Can be @c NULL in which case the current thread is assumed.
 * @return  Identifier of the thread.
 */
DENG_PUBLIC uint Sys_ThreadId(thread_t handle);

DENG_PUBLIC uint Sys_CurrentThreadId(void);

DENG_PUBLIC boolean Sys_InMainThread(void);

DENG_PUBLIC mutex_t Sys_CreateMutex(const char* name);

DENG_PUBLIC void Sys_DestroyMutex(mutex_t mutexHandle);

DENG_PUBLIC void Sys_Lock(mutex_t mutexHandle);

DENG_PUBLIC void Sys_Unlock(mutex_t mutexHandle);

#if 0
/// @todo update these if/when needed
sem_t Sem_Create(uint32_t initialValue);
void Sem_Destroy(sem_t semaphore);
void Sem_P(sem_t semaphore);
void Sem_V(sem_t semaphore);
#endif

/*
 * Private functions.
 */

void Sys_MarkAsMainThread(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_SYSTEM_CONCURRENCY_H
