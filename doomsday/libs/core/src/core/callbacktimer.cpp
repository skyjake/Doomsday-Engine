/**
 * @file callbacktimer.cpp
 * Internal helper class for making callbacks to C code.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../src/core/callbacktimer.h"
#if 0
namespace de {
namespace internal {

CallbackTimer::CallbackTimer(std::function<void ()> func, QObject *parent)
    : QTimer(parent), _func(func)
{
    setSingleShot(true);
    connect(this, SIGNAL(timeout()), this, SLOT(callbackAndDeleteLater()));
}

void CallbackTimer::callbackAndDeleteLater()
{
    if (_func) _func();

    // The timer will be gone.
    this->deleteLater();
}

} // namespace internal
} // namespace de
#endif
