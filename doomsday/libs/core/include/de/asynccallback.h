/** @file asynccallback.h  Asynchronous callback utility.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_ASYNCCALLBACK_H
#define LIBCORE_ASYNCCALLBACK_H

#include "de/waitable.h"

namespace de {

template <typename Type>
class AsyncCallback
{
public:
    AsyncCallback(Type callback)
        : _callback(callback)
    {}

    bool isValid() const
    {
        return _callback != nullptr;
    }

    template <typename... Args>
    void call(Args... args)
    {
        if (isValid())
        {
            _callback(args...);
        }
        _done.post();
    }

    void cancel()
    {
        _callback = nullptr;
        _done.post();
    }

    void wait(TimeSpan timeout)
    {
        try
        {
            _done.wait(timeout);
        }
        catch (const Waitable::TimeOutError &)
        {
            cancel();
        }
    }

private:
    Type _callback;
    Waitable _done;
};

} // namespace de

#endif // LIBCORE_ASYNCCALLBACK_H
