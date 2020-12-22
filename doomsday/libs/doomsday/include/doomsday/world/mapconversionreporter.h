/** @file mapconversionreporter.h  Map converter reporter utility.
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

#pragma once

#include "map.h"
#include <de/string.h>
#include <de/vector.h>
#include <map>

namespace world {

class Line;
class Sector;

/**
 * Observes the progress of a map conversion and records any issues/problems that
 * are encountered in the process. When asked, compiles a human-readable report
 * intended to assist mod authors in debugging their maps.
 *
 * @todo Consolidate with the missing material reporting done elsewhere -ds
 */
class LIBDOOMSDAY_PUBLIC MapConversionReporter
: DE_OBSERVES(Map, UnclosedSectorFound)
, DE_OBSERVES(Map, OneWayWindowFound)
, DE_OBSERVES(Map, Deletion)
{
    /// Record "unclosed sectors".
    /// Sector index => world point relatively near to the problem area.
    typedef std::map<int, de::Vec2i> UnclosedSectorMap;

    /// Record "one-way window lines".
    /// Line index => Sector index the back side faces.
    typedef std::map<int, int> OneWayWindowMap;

    /// Maximum number of warnings to output (of each type) about any problems
    /// encountered during the build process.
    static const int maxWarningsPerType;

public:
    MapConversionReporter(Map *map = nullptr);
    ~MapConversionReporter();

    /**
     * Change the map to be reported on. Note that any existing report data is
     * retained until explicitly cleared.
     */
    void setMap(Map *newMap);

    inline void clearMap() { setMap(nullptr); }

    inline void setMapAndClearReport(Map *newMap)
    {
        setMap(newMap);
        clearReport();
    }

    /**
     * Clear any existing conversion report data.
     */
    void clearReport();

    /**
     * Compile and output any existing report data to the message log.
     */
    void writeLog();

protected:
    void unclosedSectorFound(Sector &sector, const de::Vec2d &nearPoint) override;
    void oneWayWindowFound(Line &line, Sector &backFacingSector) override;
    void mapBeingDeleted(const Map &map) override;

private:
    inline int unclosedSectorCount() const { return int(_unclosedSectors.size()); }
    inline int oneWayWindowCount() const   { return int(_oneWayWindows.size()); }

    static inline int maxWarnings(int issueCount)
    {
#ifdef DE_DEBUG
        return issueCount; // No limit.
#else
        return de::min(issueCount, maxWarningsPerType);
#endif
    }

    void observeMap(bool yes);

    Map *             _map = nullptr; // Map currently being reported on, if any (not owned).
    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
};

} // namespace world
