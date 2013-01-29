/** @file event.h Base class for events.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_EVENT_H
#define LIBDENG2_EVENT_H

namespace de {

/**
 * Base class for events.
 *
 * @ingroup widgets
 */
class DENG2_PUBLIC Event
{
public:
    enum {
        KeyPress = 1,
        KeyRelease = 2
    };

public:
    Event(int type_ = 0) : _type(type_) {}

    virtual ~Event() {}

    /**
     * Returns the type code of the event.
     */
    int type() const { return _type; }

private:
    int _type;
};

} // namespace de

#endif // LIBDENG2_EVENT_H
