/** @file mapinfo.h  Hexen-format MAPINFO definition parsing.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef LIBCOMMON_MAPINFO_H
#define LIBCOMMON_MAPINFO_H
#ifdef __cplusplus

#include "common.h"

namespace common {

struct HexDefs; // Forward

class MapInfo : public de::Record
{
public:
    MapInfo();
    MapInfo &operator = (MapInfo const &other);

    void resetToDefaults();
};

class EpisodeInfo : public de::Record
{
public:
    EpisodeInfo();
    EpisodeInfo &operator = (EpisodeInfo const &other);

    void resetToDefaults();
};

/**
 * Parser for Hexen's MAPINFO definition lumps.
 */
class MapInfoParser
{
public:
    /// Base class for all parse-related errors. @ingroup errors
    DENG2_ERROR(ParseError);

public:
    MapInfoParser(HexDefs &db);

    void parse(AutoStr const &buffer, de::String sourceFile);

    /**
     * Clear any custom default MapInfo definition currently in use. MapInfos
     * read after this is called will use the games' default definition as a
     * basis (unless specified otherwise).
     */
    void clearDefaultMap();

private:
    DENG2_PRIVATE(d)
};

/**
 * Central database of definitions read from Hexen-derived definition formats.
 *
 * @note Ultimately the definitions this contains should instead have their sources
 * translated into DED syntax and be made available from the main DED db instead.
 */
struct HexDefs
{
    typedef std::map<std::string, EpisodeInfo> EpisodeInfos;
    EpisodeInfos episodeInfos;
    typedef std::map<std::string, MapInfo> MapInfos;
    MapInfos mapInfos;

    void clear();

    /**
     * @param id  Identifier of the episode to lookup info for.
     *
     * @return  EpisodeInfo for the specified @a id; otherwise @c 0 (not found).
     */
    EpisodeInfo *getEpisodeInfo(de::String id);

    /**
     * @param mapUri  Identifier of the map to lookup info for.
     *
     * @return  MapInfo for the specified @a mapUri; otherwise @c 0 (not found).
     */
    MapInfo *getMapInfo(de::Uri const &mapUri);
};
extern HexDefs hexDefs;

/**
 * @param id  Identifier of the episode to lookup info for.
 *
 * @return  EpisodeInfo for the specified @a id; otherwise @c 0 (not found).
 */
EpisodeInfo *P_EpisodeInfo(de::String id);

EpisodeInfo *P_CurrentEpisodeInfo();

/**
 * @param mapUri  Identifier of the map to lookup info for.
 *
 * @return  MapInfo for the specified @a mapUri; otherwise @c 0 (not found).
 */
MapInfo *P_MapInfo(de::Uri const &mapUri);

MapInfo *P_CurrentMapInfo();

/**
 * Translates a warp map number to unique map identifier. Always returns a valid
 * map identifier.
 *
 * @note This should only be used where necessary for compatibility reasons as
 * the "warp translation" mechanic is redundant in the context of Doomsday's
 * altogether better handling of map resources and their references. Instead,
 * use the map URI mechanism.
 *
 * @param map  The warp map number to translate.
 *
 * @return The unique identifier of the map given a warp map number. If the map
 * is not found a URI to the first available map is returned (i.e., Maps:MAP01)
 */
de::Uri P_TranslateMap(uint map);

} // namespace common

#endif // __cplusplus
#endif // LIBCOMMON_MAPINFO_H
