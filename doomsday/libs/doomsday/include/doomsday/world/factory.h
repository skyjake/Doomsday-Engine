/** @file factory.h  Factory for constructing world objects.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "../libdoomsday.h"

#include <de/Id>

class MobjThinkerData;

namespace world {

class LIBDOOMSDAY_PUBLIC Factory
{
public:
    static void setMobjThinkerDataConstructor(const std::function<MobjThinkerData *(const de::Id &)> &);

    static MobjThinkerData *newMobjThinkerData(const de::Id &);
};

} // namespace world
