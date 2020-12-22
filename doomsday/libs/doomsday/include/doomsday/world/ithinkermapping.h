/** @file ithinkermapping.h  Thinker mapping.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_WORLD_ITHINKERMAPPING_H
#define LIBDOOMSDAY_WORLD_ITHINKERMAPPING_H

#include "../libdoomsday.h"
#include "thinker.h"
#include <de/id.h>

namespace world {

/**
 * Utility for mapping private identifiers to thinker objects. Used when writing and
 * reading save files.
 */
class LIBDOOMSDAY_PUBLIC IThinkerMapping
{
public:
    virtual ~IThinkerMapping() = default;

    /**
     * Retrieves the thinker corresponding a private identifier.
     * @param id  Private identifier.
     * @return Thinker instance.
     */
    virtual thinker_t *thinkerForPrivateId(de::Id::Type id) const = 0;
};

} // namespace world

#endif // LIBDOOMSDAY_WORLD_ITHINKERMAPPING_H
