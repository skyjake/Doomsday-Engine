/** @file thread.h  Thread.
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

#ifndef LIBCORE_THREAD_H
#define LIBCORE_THREAD_H

#include "de/libcore.h"
#include "de/time.h"
#include "de/observers.h"
#include "de/waitable.h"

namespace de {

/**
 * Base class for running a thread.
 */
class DE_PUBLIC Thread : public Waitable
{
public:
    Thread();

    virtual ~Thread();

    void setName(const String &name);
    void setTerminationEnabled(bool enable);

    void start();
    void join();
    void terminate();
    bool isRunning() const;
    bool isFinished() const;
    bool isCurrentThread() const;

    virtual void run() = 0;

    static void sleep(TimeSpan span);
    static Thread *currentThread();

public:
    DE_AUDIENCE(Finished, void threadFinished(Thread &))

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_THREAD_H
