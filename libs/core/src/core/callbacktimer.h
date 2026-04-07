/**
 * @file callbacktimer.h
 * Internal helper class for making callbacks.
 *
 * @authors Copyright © 2012-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_CALLBACKTIMER_H
#define LIBCORE_CALLBACKTIMER_H

#include "de/timer.h"

namespace de {
namespace internal {

/**
 * Helper for making timed callbacks to C code.
 */
class CallbackTimer : public Timer
{
public:
    explicit CallbackTimer(const std::function<void ()> &func);

private:
    std::function<void ()> _func;
};

} // namespace internal
} // namespace de

#endif // LIBCORE_CALLBACKTIMER_H
