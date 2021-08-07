/** @file map.h  Base for world maps.
 * @ingroup world
 *
 * @authors Copyright © 2014-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_MAP_H
#define LIBDOOMSDAY_WORLD_MAP_H

#include "../libdoomsday.h"
#include "../resource/mapmanifest.h"
#include <de/Observers>
#include <de/Reader>
#include <de/Writer>

class EntityDatabase;

namespace world {

class IThinkerMapping;

/**
 * Base class for world maps.
 */
class LIBDOOMSDAY_PUBLIC BaseMap
{
public:
    /// No resource manifest is associated with the map. @ingroup errors
    DENG2_ERROR(MissingResourceManifestError);

    /// Required map object is missing. @ingroup errors
    DENG2_ERROR(MissingObjectError);

public:
    /**
     * @param manifest  Resource manifest for the map (Can be set later, @ref setDef).
     */
    explicit BaseMap(res::MapManifest *manifest = nullptr);
    virtual ~BaseMap();

    de::String id() const;

    /**
     * Returns @c true if a resource manifest is associated with the map.
     *
     * @see manifest(), setManifest()
     */
    bool hasManifest() const;

    /**
     * Returns the resource manifest for the map.
     *
     * @see hasManifest(), setManifest()
     */
    res::MapManifest &manifest() const;

    /**
     * Change the associated resource manifest to @a newManifest.
     *
     * @see hasManifest(), manifest()
     */
    void setManifest(res::MapManifest *newManifest);

    /**
     * Provides access to the entity database.
     */
    EntityDatabase &entityDatabase() const;

    virtual void serializeInternalState(de::Writer &to) const;

    virtual void deserializeInternalState(de::Reader &from, IThinkerMapping const &);

    DENG2_CAST_METHODS()
    
    /**
     * When map objects are being created, they will be associated with the map spot number
     * specified with this method. @a mapSpotNum should be set to -1 to disassociate further
     * spawning from any map spot.
     *
     * This Map-wide value exists because the map object creation APIs do not take a map spot
     * as argument, and adding this argument would have an impact on many functions. Also, mobj
     * creation is libdoomsday's responsibility, while only the game knows when map spots are
     * being spawned.
     */
    void setCurrentMapSpot(int mapSpotNum);
    
    int currentMapSpot(void);
    
public:
    /// Notified when the map is about to be deleted.
    DENG2_DEFINE_AUDIENCE2(Deletion, void mapBeingDeleted(BaseMap const &map))

private:
    DENG2_PRIVATE(d)
};

typedef de::duint16 InternalSerialId;

// Identifiers for serialized internal state.
enum InternalSerialIds
{
    THINKER_DATA                = 0x0001,
    MOBJ_THINKER_DATA           = 0x0002,
    CLIENT_MOBJ_THINKER_DATA    = 0x0003,
    STATE_ANIMATOR              = 0x0004,
};

}  // namespace world

#endif  // LIBDOOMSDAY_WORLD_MAP_H
