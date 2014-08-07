/** @file defs/mapgraphnode.h  MapGraphNode definition accessor.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_DEFN_MAPGRAPHNODE_H
#define LIBDOOMSDAY_DEFN_MAPGRAPHNODE_H

#include "../libdoomsday.h"
#include <de/RecordAccessor>

namespace defn {

/**
 * Utility for handling "map-connectivity graph, node" definitions.
 */
class LIBDOOMSDAY_PUBLIC MapGraphNode : public de::RecordAccessor
{
public:
    MapGraphNode() : RecordAccessor(0), _def(0) {}
    MapGraphNode(MapGraphNode const &other)
        : RecordAccessor(other)
        , _def(other._def) {}
    MapGraphNode(de::Record &d) : RecordAccessor(d), _def(&d) {}
    MapGraphNode(de::Record const &d) : RecordAccessor(d), _def(0) {}

    void resetToDefaults();

    MapGraphNode &operator = (de::Record *d);

    operator bool() const;
    int order() const;

    de::Record &addExit();

    int exitCount() const;
    bool hasExit(int index) const;
    de::Record &exit(int index);
    de::Record const &exit(int index) const;

private:
    de::Record *_def; ///< Modifiable access.
};

} // namespace defn

#endif // LIBDOOMSDAY_DEFN_MAPGRAPHNODE_H
