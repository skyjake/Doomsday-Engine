/** @file coreevent.h  Internal libcore events.
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

#ifndef LIBCORE_COREEVENT_H
#define LIBCORE_COREEVENT_H

#include "../Event"
#include "../Value"

namespace de {

/**
 * Internal libcore events.
 * @ingroup core
 */
class DE_PUBLIC CoreEvent : public Event
{
public:
    CoreEvent(int type);
    CoreEvent(int type, const Value &value);
    CoreEvent(int type, const std::function<void()> &callback);
    CoreEvent(const std::function<void()> &callback);

    const Value &value() const;
    inline int   valuei() const { return value().asInt(); }

    const std::function<void()> &callback() const;

private:
    std::unique_ptr<Value> _value;
    std::function<void()>  _callback;
};

} // namespace de

#endif // LIBCORE_COREEVENT_H
