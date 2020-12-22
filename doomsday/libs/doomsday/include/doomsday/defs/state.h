/** @file state.h  Mobj state definition.
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

#ifndef LIBDOOMSDAY_DEFN_STATE_H
#define LIBDOOMSDAY_DEFN_STATE_H

#include "definition.h"

namespace defn {
    
/**
 * Mobj state definition.
 */
class LIBDOOMSDAY_PUBLIC State : public Definition
{
public:
    State()                    : Definition() {}
    State(const State &other)  : Definition(other) {}
    State(de::Record &d)       : Definition(d) {}
    State(const de::Record &d) : Definition(d) {}
    
    void resetToDefaults();
    
    de::dint32 misc(int index) const;
    void setMisc(int index, de::dint32 value);
};
    
} // namespace defn

#endif // LIBDOOMSDAY_DEFN_STATE_H
