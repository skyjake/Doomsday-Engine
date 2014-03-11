/** @file mapstatereader.h  Saved map state reader.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_MAPSTATEREADER_H
#define LIBCOMMON_MAPSTATEREADER_H

#include "common.h"
#include "thingarchive.h"
#include <de/Error>
#include <de/game/IMapStateReader>

/**
 * Performs saved game map state deserialization.
 *
 * @ingroup libcommon
 * @see MapStateWriter
 */
class MapStateReader : public de::game::IMapStateReader
{
public:
    /// Base class for read-related errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    MapStateReader();
    ~MapStateReader();

    static de::game::IMapStateReader *make();

    /**
     * Deserialize the saved map state.
     */
    void read(de::Path const &filePath, de::game::SessionMetadata const &metadata);

    /**
     * Returns the logical map version of the saved map state.
     */
    int mapVersion();

    /**
     * Returns the reader to use when deserializing the saved map state.
     */
    Reader *reader();

    /**
     * Lookup a pointer to a Mobj with the given @a serialId.
     */
    struct mobj_s *mobj(ThingArchive::SerialId serialId, void *address) const;

    /**
     * Lookup a pointer to a Material with the given @a serialId.
     *
     * @param serialId  Unique identifier for the material in the material archive.
     * @param group     Used with previous versions of the material archive, which
     *                  separated materials into groups (0= Flats 1= Textures).
     *
     * @return  Pointer to the associated material; otherwise @c 0 (not archived).
     */
    Material *material(materialarchive_serialid_t serialId, int group) const;

    /**
     * Lookup a pointer to a Side with the given @a sideIndex.
     */
    Side *side(int sideIndex) const;

    /**
     * Lookup a pointer to a player with the given @a serialId.
     */
    struct player_s *player(int serialId) const;

public: /// @todo refactor away:
    void addMobjToThingArchive(struct mobj_s *mobj, ThingArchive::SerialId);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_MAPSTATEREADER_H
