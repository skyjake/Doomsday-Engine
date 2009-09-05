/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_THREAD_H
#define LIBDENG2_THREAD_H

#include "../deng.h"
#include "../Waitable"

namespace de
{
    /**
     * The Thread class runs its own thread of execution.  The run() method 
     * should be overridden by subclasses to perform a background task.
     *
     * This is an abstract class because no implementation has been
     * specified for run().
     *
     * @ingroup core
     */
    class Thread
    {
    public:
        Thread();

        /// If not already stopped, the thread is forcibly killed in
        /// the destructor.
        virtual ~Thread();

        /// Start executing the thread.
        virtual void start();

        /// Signals the thread to stop. Returns immediately.
        virtual void stop();
        
        /** 
         * Signals the thread to stop and waits until it does.
          *
         * @param timeOut  Maximum period of time to wait.
         */
        virtual void join(const Time::Delta& timeOut);

        /// This method is executed when the thread is started.
        virtual void run() = 0;
        
        /// @return @c true, if the thread should stop itself as soon as
        /// possible.
        bool shouldStopNow() const;
        
        /// @return @c true, if the thread is currently running.
        bool isRunning() const;
    
    private:
        static int runner(void* owner);
 
        /// This is set to true when the thread should stop.
        volatile bool _stopNow;
        
        /// Pointer to the internal thread data.
        typedef void* Handle;
        volatile Handle _thread;
        
        /// Signals the end of the thread.
        Waitable _endOfThread;
    };
}

#endif /* LIBDENG2_THREAD_H */
