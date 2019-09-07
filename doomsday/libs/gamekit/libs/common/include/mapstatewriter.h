/** @file mapstatewriter.h  Saved map state writer.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_MAPSTATEWRITER_H
#define LIBCOMMON_MAPSTATEWRITER_H

#include "common.h"
#include "thingarchive.h"
#include <doomsday/world/ithinkermapping.h>

namespace world { class Material; }

/**
 * Serialized map states are separated into identifiable chunks (in Hexen).
 */
enum MapStateChunkId
{
    ASEG_MAP_HEADER = 102,  // Hexen only
    ASEG_MAP_ELEMENTS,
    ASEG_POLYOBJS,          // Hexen only
    ASEG_MOBJS,             // Hexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS,           // Hexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS,            // Hexen only
    ASEG_MISC,              // Hexen only
    ASEG_END,               // = 111
    ASEG_MATERIAL_ARCHIVE,
    ASEG_MAP_HEADER2,
    ASEG_PLAYER_HEADER
};

/**
 * Performs saved game map state serialization.
 *
 * @ingroup libcommon
 * @see MapStateReader
 */
class MapStateWriter
{
public:
    MapStateWriter();

    /**
     * Serialize the @em current map state using the specified @a writer.
     *
     * @param excludePlayers  @c true= Do not include players in the ThingArchive.
     */
    void write(Writer1 *writer, bool excludePlayers = false);

    /**
     * Returns the writer to use when serializing the map state.
     */
    Writer1 *writer();

    ThingArchive::SerialId serialIdFor(const struct mobj_s *mobj);
    materialarchive_serialid_t serialIdFor(world::Material *material);
    materialarchive_serialid_t serialIdFor(struct material_s *material);

    // Implements world::IThinkerMapping.
    //int mappedNumberfromThinkerId(const de::Id &thinkerId) const override;
    //de::Id thinkerIdFromMappedNumber(int mappedNumber) const override;

private:
    DE_PRIVATE(d)
};

#endif // LIBCOMMON_MAPSTATEWRITER_H
