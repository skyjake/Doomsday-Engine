/** @file task.h  Concurrent task.
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

#ifndef LIBCORE_TASK_H
#define LIBCORE_TASK_H

#include "de/libcore.h"
#include "de/taskpool.h"
#include "de/deletable.h"

namespace de {

class DE_PUBLIC IRunnable
{
public:
    virtual void run() = 0;
    virtual ~IRunnable() = default;
};

/**
 * Concurrent task that will be executed asynchronously by a TaskPool. Override
 * runTask() in a derived class.
 *
 * @ingroup concurrency
 */
class DE_PUBLIC Task : public IRunnable, public Deletable
{
public:
    Task();
    virtual ~Task();

    void run();

    /**
     * Task classes must override this.
     */
    virtual void runTask() = 0;

private:
    friend class TaskPool;

    TaskPool::IPool *_pool;
};

} // namespace de

#endif // LIBCORE_TASK_H
