/** @file thing.h  Thing definition.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_DEFS_THING_H
#define LIBDOOMSDAY_DEFS_THING_H

#include "definition.h"

namespace defn {
    
/**
 * Thing definition.
 */
class LIBDOOMSDAY_PUBLIC Thing : public Definition
{
public:
    Thing()                    : Definition() {}
    Thing(const Thing &other)  : Definition(other) {}
    Thing(de::Record &d)       : Definition(d) {}
    Thing(const de::Record &d) : Definition(d) {}
    
    void resetToDefaults();
    
    void setSound(int soundId, const de::String &sound);
    de::String sound(int soundId) const;
    
    int flags(int index) const;
    void setFlags(int index, int flags);
};
    
} // namespace defn

#endif // LIBDOOMSDAY_DEFS_THING_H
