/** @file bspbuilder.cpp BSP Builder.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include <map>
#include <utility>

#include <de/Error>
#include <de/Log>
#include <de/Observers>
#include <de/Vector>

#include "BspLeaf"
#include "Segment"
#include "Sector"
#include "world/bsp/partitioner.h"

#include "world/bspbuilder.h"

using namespace de;
using namespace bsp;

DENG2_PIMPL_NOREF(BspBuilder)
{
    /// World space partitioner.
    Partitioner partitioner;

    Instance(GameMap const &map) : partitioner(map) {}
};

BspBuilder::BspBuilder(GameMap const &map, int splitCostFactor)
    : d(new Instance(map))
{
    d->partitioner.setSplitCostFactor(splitCostFactor);
}

void BspBuilder::setSplitCostFactor(int newFactor)
{
    d->partitioner.setSplitCostFactor(newFactor);
}

/// Maximum number of warnings to output (of each type) about any problems
/// encountered during the build process.
static int const maxWarningsPerType = 10;

/**
 * Observes the progress of the build and records any issues/problems encountered
 * in the process. When asked, compiles a human-readable report intended to assist
 * mod authors in debugging their maps.
 *
 * @todo Consolidate with the missing material reporting done elsewhere -ds
 */
class Reporter : DENG2_OBSERVES(Partitioner, UnclosedSectorFound),
                 DENG2_OBSERVES(Partitioner, OneWayWindowFound)
{
    /// Record "unclosed sectors".
    /// Sector => world point relatively near to the problem area.
    typedef std::map<Sector *,  Vector2d> UnclosedSectorMap;

    /// Record "one-way window lines".
    /// Line => Sector the back side faces.
    typedef std::map<Line *,  Sector *> OneWayWindowMap;

public:

    static inline int maxWarnings(int issueCount)
    {
#ifdef DENG_DEBUG
        return issueCount; // No limit.
#else
        return de::min(issueCount, maxWarningsPerType);
#endif
    }

    inline int unclosedSectorCount() const { return (int)_unclosedSectors.size(); }
    inline int oneWayWindowCount() const { return (int)_oneWayWindows.size(); }

    void writeLog()
    {
        if(int numToLog = maxWarnings(unclosedSectorCount()))
        {
            UnclosedSectorMap::const_iterator it = _unclosedSectors.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                LOG_WARNING("Sector #%d is unclosed near %s.")
                    << it->first->indexInMap() << it->second.asText();
            }

            if(numToLog < unclosedSectorCount())
                LOG_INFO("(%d more like this)") << (unclosedSectorCount() - numToLog);
        }

        if(int numToLog = maxWarnings(oneWayWindowCount()))
        {
            OneWayWindowMap::const_iterator it = _oneWayWindows.begin();
            for(int i = 0; i < numToLog; ++i, ++it)
            {
                LOG_VERBOSE("Line #%d seems to be a One-Way Window (back faces sector #%d).")
                    << it->first->indexInMap() << it->second->indexInMap();
            }

            if(numToLog < oneWayWindowCount())
                LOG_INFO("(%d more like this)") << (oneWayWindowCount() - numToLog);
        }
    }

protected:
    // Observes Partitioner UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        _unclosedSectors.insert(std::make_pair(&sector, nearPoint));
    }

    // Observes Partitioner OneWayWindowFound.
    void oneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        _oneWayWindows.insert(std::make_pair(&line, &backFacingSector));
    }

private:
    UnclosedSectorMap _unclosedSectors;
    OneWayWindowMap   _oneWayWindows;
};

bool BspBuilder::buildBsp()
{
    Reporter reporter;

    d->partitioner.audienceForUnclosedSectorFound += reporter;
    d->partitioner.audienceForOneWayWindowFound   += reporter;

    bool builtOk = false;
    try
    {
        d->partitioner.build();
        builtOk = true;
    }
    catch(Error const &er)
    {
        LOG_AS("BspBuilder");
        LOG_WARNING("%s.") << er.asText();
    }

    reporter.writeLog();

    return builtOk;
}

BspTreeNode *BspBuilder::root() const
{
    return d->partitioner.root();
}

int BspBuilder::numNodes()
{
    return d->partitioner.numNodes();
}

int BspBuilder::numLeafs()
{
    return d->partitioner.numLeafs();
}

int BspBuilder::numSegments()
{
    return d->partitioner.numSegments();
}

int BspBuilder::numVertexes()
{
    return d->partitioner.numVertexes();
}

Vertex &BspBuilder::vertex(int idx)
{
    return d->partitioner.vertex(idx);
}

void BspBuilder::take(MapElement *mapElement)
{
    d->partitioner.release(mapElement);
}
