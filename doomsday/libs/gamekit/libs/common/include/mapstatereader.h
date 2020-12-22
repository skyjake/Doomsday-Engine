/** @file mapstatereader.h  Common (native) saved map state reader.
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

#ifndef LIBCOMMON_MAPSTATEREADER_H
#define LIBCOMMON_MAPSTATEREADER_H

#include <de/legacy/reader.h>
#include <de/string.h>
#include <doomsday/gamestatefolder.h>
#include "common.h"
#include "thingarchive.h"

namespace world { class Material; }

/**
 * Performs native saved game map state deserialization.
 *
 * @ingroup libcommon
 * @see MapStateWriter
 */
class MapStateReader : public GameStateFolder::MapStateReader
{
public:
    MapStateReader(const GameStateFolder &session);
    virtual ~MapStateReader();

    /**
     * Deserialize the saved map state.
     */
    void read(const de::String &mapUriStr);

    /**
     * Returns the logical map version of the saved map state.
     */
    int mapVersion();

    /**
     * Returns the reader to use when deserializing the saved map state.
     */
    Reader1 *reader();

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
    world_Material *material(materialarchive_serialid_t serialId, int group) const;

    /**
     * Lookup a pointer to a Side with the given @a sideIndex.
     */
    Side *side(int sideIndex) const;

    /**
     * Lookup a pointer to a player with the given @a serialId.
     */
    struct player_s *player(int serialId) const;

    // Implements IThinkerMapping.
    thinker_t *thinkerForPrivateId(de::Id::Type id) const override;

public: /// @todo refactor away:
    void addMobjToThingArchive(struct mobj_s *mobj, ThingArchive::SerialId);

private:
    DE_PRIVATE(d)
};

#endif // LIBCOMMON_MAPSTATEREADER_H
