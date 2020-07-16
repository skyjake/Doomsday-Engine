/**
 * @file concurrency.h
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

#ifndef DE_SYSTEM_CONCURRENCY_H
#define DE_SYSTEM_CONCURRENCY_H

#include <de/liblegacy.h>

/// @addtogroup legacy
/// @{

typedef void *thread_t;
typedef void *mutex_t;
typedef void *sem_t;

typedef enum systhreadexitstatus_e {
    DE_THREAD_STOPPED_NORMALLY,
    DE_THREAD_STOPPED_WITH_FORCE, // terminated
    DE_THREAD_STOPPED_WITH_EXCEPTION
} systhreadexitstatus_t;

#ifdef __cplusplus

#include <functional>
typedef std::function<int (void *)> systhreadfunc_t;

#ifdef __DE__ // libdeng internal
#include <de/thread.h>
/**
 * Thread that runs a user-specified callback function. Exceptions from the callback
 * function are caught.
 */
class CallbackThread
    : public de::Thread
//    , DE_OBSERVES(de::Thread, Finished)
{
public:
    CallbackThread(systhreadfunc_t func, void *parm = 0);
    ~CallbackThread();

    void                  run() override;
    int                   exitValue() const;
    systhreadexitstatus_t exitStatus() const;
    void                  setTerminationFunc(void (*func)(systhreadexitstatus_t));

//    void threadFinished(Thread &) override;

private:
    systhreadfunc_t       _callback;
    void *                _parm;
    int                   _returnValue;
    systhreadexitstatus_t _exitStatus;
    void (*_terminationFunc)(systhreadexitstatus_t);
};

#endif // __DE__

/**
 * Starts a new thread.
 *
 * @param startpos  Executes while the thread is running. When the function exists,
 *                  the thread stops.
 * @param parm             Parameter given to the thread function.
 * @param terminationFunc  Callback function that is called from the worker thread
 *                         right before it exits. The callback is given the exit status
 *                         of the thread as a parameter.
 * @return Thread handle.
 */
DE_PUBLIC thread_t Sys_StartThread(systhreadfunc_t startpos,
                                   void *          parm,
                                   void (*terminationFunc)(systhreadexitstatus_t));

extern "C" {
#endif // __cplusplus

/**
 * Starts a new thread.
 *
 * @param startpos  Executes while the thread is running. When the function exists,
 *                  the thread stops.
 * @param parm             Parameter given to the thread function.
 * @param terminationFunc  Callback function that is called from the worker thread
 *                         right before it exits. The callback is given the exit status
 *                         of the thread as a parameter.
 * @return Thread handle.
 */
DE_PUBLIC thread_t Sys_StartThread(int (*startpos)(void *),
                                   void *parm,
                                   void (*terminationFunc)(systhreadexitstatus_t));

DE_PUBLIC void Thread_Sleep(int milliseconds);

DE_PUBLIC void Thread_KillAbnormally(thread_t handle);

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
DE_PUBLIC int Sys_WaitThread(thread_t handle, int timeoutMs, systhreadexitstatus_t *exitStatus);

/**
 * @param handle  Handle to the thread to return the id of.
 *                Can be @c NULL in which case the current thread is assumed.
 * @return  Identifier of the thread.
 */
DE_PUBLIC uint32_t Sys_ThreadId(thread_t handle);

DE_PUBLIC uint32_t Sys_CurrentThreadId(void);

DE_PUBLIC dd_bool Sys_InMainThread(void);

DE_PUBLIC mutex_t Sys_CreateMutex(const char *name);

DE_PUBLIC void Sys_DestroyMutex(mutex_t mutexHandle);

DE_PUBLIC void Sys_Lock(mutex_t mutexHandle);

DE_PUBLIC void Sys_Unlock(mutex_t mutexHandle);

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_SYSTEM_CONCURRENCY_H
