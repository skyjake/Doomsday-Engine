/** @file cledgeloop.h  Client-side world map subsector boundary edge loop.
 * @ingroup world
 *
 * @authors Copyright © 2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_WORLD_CLEDGELOOP_H
#define DENG_CLIENT_WORLD_CLEDGELOOP_H

#include <de/String>
#include "misc/hedge.h"
#include "client/clientsubsector.h"

namespace world {

class ClEdgeLoop
{
public:
    ClEdgeLoop(ClientSubsector &owner, de::HEdge &first,
               de::dint edgeId = ClientSubsector::OuterLoop);

    ClientSubsector &owner() const;

    de::String description() const;

    de::dint loopId() const;

    inline bool isInner() const { return loopId() == ClientSubsector::InnerLoop; }
    inline bool isOuter() const { return loopId() == ClientSubsector::OuterLoop; }

    bool isSelfReferencing() const;

    de::HEdge &firstHEdge() const;

    bool hasBackSubsector() const;

    ClientSubsector &backSubsector() const;

    /**
     * Do as in the original DOOM if the texture has not been defined - extend the
     * floor/ceiling to fill the space (unless it is skymasked).
     */
    void fixSurfacesMissingMaterials();

private:
    DENG2_PRIVATE(d)
};

} // namespace world

#endif // DENG_CLIENT_WORLD_CLEDGELOOP_H
