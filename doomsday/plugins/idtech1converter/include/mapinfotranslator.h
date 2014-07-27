/** @file mapinfotranslator.h  Hexen-format MAPINFO definition translator.
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

#ifndef IDTECH1CONVERTER_MAPINFOTRANSLATOR_H
#define IDTECH1CONVERTER_MAPINFOTRANSLATOR_H

#include "idtech1converter.h"
#include <map>
#include <de/Error>
#include <de/Record>

namespace idtech1 {

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

    /**
     * To be called once all definitions have been parsed to translate Hexen's
     * map "warp numbers" to URIs where used as map definition references.
     */
    void translateMapWarpNumbers();

private:
    de::Uri translateMapWarpNumber(uint map);
};

} // namespace idtech1

#endif // IDTECH1CONVERTER_MAPINFOTRANSLATOR_H
