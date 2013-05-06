/** @file task.h  Concurrent task.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_TASK_H
#define LIBDENG2_TASK_H

#include <QRunnable>

#include "../libdeng2.h"

namespace de {

class TaskPool;

/**
 * Concurrent task that will be executed asynchronously by a TaskPool. Override
 * runTask() in a derived class.
 */
class DENG2_PUBLIC Task : public QRunnable
{
public:
    Task();
    virtual ~Task() {}

    TaskPool &pool() const;
    void run();

    /**
     * Task classes must override this.
     */
    virtual void runTask() = 0;

private:
    friend class TaskPool;

    TaskPool *_pool;
};

} // namespace de

#endif // LIBDENG2_TASK_H
