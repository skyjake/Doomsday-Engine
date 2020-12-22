/** @file event.h Base class for events.
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

#ifndef LIBCORE_EVENT_H
#define LIBCORE_EVENT_H

#include "de/libcore.h"

#ifdef KeyPress
#  undef KeyPress
#endif
#ifdef KeyRelease
#  undef KeyRelease
#endif

namespace de {

/**
 * Base class for events.
 *
 * @ingroup core
 */
class DE_PUBLIC Event
{
public:
    /// Event type codes.
    enum {
        Quit     = 1,
        Callback = 2,
        Timer    = 3,

        KeyPress   = 100,
        KeyRelease = 111,
        KeyRepeat  = 112,

        MouseButton   = 120,
        MouseMotion   = 121,
        MousePosition = 122,
        MouseWheel    = 123,
    };

public:
    Event(int type_ = 0) : _type(type_) {}

    virtual ~Event() {}

    /**
     * Returns the type code of the event.
     */
    int type() const { return _type; }

    bool isKey() const { return _type == KeyPress || _type == KeyRepeat || _type == KeyRelease; }
    bool isKeyDown() const { return _type == KeyPress || _type == KeyRepeat; }
    bool isMouse() const
    {
        return _type == MouseButton || _type == MouseMotion || _type == MousePosition ||
               _type == MouseWheel;
    }

    DE_CAST_METHODS()

private:
    int _type;
};

} // namespace de

#endif // LIBCORE_EVENT_H
