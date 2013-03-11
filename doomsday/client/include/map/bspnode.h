/** @file bspnode.h Map BSP Node.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_BSPNODE
#define LIBDENG_MAP_BSPNODE

#include <de/Error>
#include <de/aabox.h>
#include "resource/r_data.h"
#include "p_dmu.h"
#include "MapElement"

/**
 * An infinite line of the form point + direction vectors.
 *
 * @ingroup map
 */
struct partition_t
{
    coord_t origin[2];
    coord_t direction[2];
};

/// Child node identifiers:
#define RIGHT                   0
#define LEFT                    1

/**
 * Node in the BSP tree. Children of a node can be either instances of BspNode
 * or BspLeaf.
 *
 * @ingroup map
 */
class BspNode : public de::MapElement
{
public: /// @todo make private:
    partition_t partition;

    /// Bounding box for each child.
    AABoxd aaBox[2];

    de::MapElement *children[2];

    /// Unique. Set when saving the BSP.
    uint index;

public:
    /**
     * @param origin  2D point in the map coordinate space which describes the
     *                origin of the half-plane.
     * @param angle   2D vector in the map coordinate space which describes the
     *                angle of the half-plane.
     */
    BspNode(coord_t const partitionOrigin[2], coord_t const partitionDirection[2]);
    ~BspNode();

    void setChild(int left, de::MapElement *newChild);

    inline void setRight(de::MapElement *newChild) { setChild(RIGHT, newChild); }

    inline void setLeft(de::MapElement *newChild) { setChild(LEFT, newChild); }

    void setChildBounds(int left, AABoxd *newBounds);

    inline void setRightBounds(AABoxd *newBounds) { setChildBounds(RIGHT, newBounds); }

    inline void setLeftBounds(AABoxd *newBounds) { setChildBounds(LEFT, newBounds); }
};

#endif // LIBDENG_MAP_BSPNODE
