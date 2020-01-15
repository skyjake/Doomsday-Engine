/** @file mapconversionreporter.cpp  Map converter reporter utility.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/mapconversionreporter.h"
#include "doomsday/world/line.h"
#include "doomsday/world/sector.h"

namespace world {

using namespace de;

const int MapConversionReporter::maxWarningsPerType = 10;

MapConversionReporter::MapConversionReporter(Map *map)
{
    setMap(map);
}

MapConversionReporter::~MapConversionReporter()
{
    observeMap(false);
}

void MapConversionReporter::setMap(Map *newMap)
{
    if (_map != newMap)
    {
        observeMap(false);
        _map = newMap;
        observeMap(true);
    }
}

void MapConversionReporter::clearReport()
{
    _unclosedSectors.clear();
    _oneWayWindows.clear();
}

void MapConversionReporter::writeLog()
{
    if (int numToLog = maxWarnings(unclosedSectorCount()))
    {
        String str;

        UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
        for (int i = 0; i < numToLog; ++i, ++it)
        {
            if (i != 0) str += "\n";
            str += Stringf(
                "Sector #%d is unclosed near %s", it->first, it->second.asText().c_str());
        }

        if (numToLog < unclosedSectorCount())
            str += Stringf("\n(%i more like this)", unclosedSectorCount() - numToLog);

        LOGDEV_MAP_WARNING("%s") << str;
    }

    if (int numToLog = maxWarnings(oneWayWindowCount()))
    {
        String str;

        OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
        for (int i = 0; i < numToLog; ++i, ++it)
        {
            if (i != 0) str += "\n";
            str += Stringf("Line #%i seems to be a One-Way Window (back faces sector #%i).",
                       it->first, it->second);
        }

        if (numToLog < oneWayWindowCount())
            str += Stringf("\n(%i more like this)", oneWayWindowCount() - numToLog);

        LOGDEV_MAP_MSG("%s") << str;
    }
}

/// Observes Map UnclosedSectorFound.
void MapConversionReporter::unclosedSectorFound(Sector &sector, const de::Vec2d &nearPoint)
{
    _unclosedSectors.insert(std::make_pair(sector.indexInArchive(), nearPoint.toVec2i()));
}

/// Observes Map OneWayWindowFound.
void MapConversionReporter::oneWayWindowFound(Line &line, Sector &backFacingSector)
{
    _oneWayWindows.insert(std::make_pair(line.indexInArchive(), backFacingSector.indexInArchive()));
}

/// Observes Map Deletion.
void MapConversionReporter::mapBeingDeleted(const Map &map)
{
    DE_ASSERT(&map == _map);  // sanity check.
    DE_UNUSED(map);
    _map = nullptr;
}

void MapConversionReporter::observeMap(bool yes)
{
    if (!_map) return;

    if (yes)
    {
        _map->audienceForDeletion()            += this;
        _map->audienceForOneWayWindowFound()   += this;
        _map->audienceForUnclosedSectorFound() += this;
    }
    else
    {
        _map->audienceForDeletion()            -= this;
        _map->audienceForOneWayWindowFound()   -= this;
        _map->audienceForUnclosedSectorFound() -= this;
    }
}

} // namespace world
