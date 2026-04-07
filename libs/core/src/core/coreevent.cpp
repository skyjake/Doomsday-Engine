/** @file coreevent.cpp  Internal libcore events.
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

#include "de/coreevent.h"
#include "de/nonevalue.h"

namespace de {

CoreEvent::CoreEvent(int type)
    : Event(type)
{}

CoreEvent::CoreEvent(int type, const Value &value)
    : Event(type)
    , _value(value.duplicate())
{}

CoreEvent::CoreEvent(int type, const std::function<void()> &callback)
    : Event(type)
    , _callback(callback)
{}

CoreEvent::CoreEvent(const std::function<void()> &callback)
    : Event(Callback)
    , _callback(callback)
{}

CoreEvent::CoreEvent(void *context, const std::function<void()> &callback)
    : Event(Callback)
    , _callback(callback)
    , _context(context)
{}

void CoreEvent::setContent(void *context)
{
    _context = context;
}

const Value &CoreEvent::value() const
{
    if (!_value)
    {
        static NoneValue null;
        return null;
    }
    return *_value;
}

const std::function<void()> &CoreEvent::callback() const
{
    return _callback;
}

} // namespace de
