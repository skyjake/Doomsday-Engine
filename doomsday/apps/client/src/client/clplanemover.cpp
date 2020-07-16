/** @file clplanemover.cpp  Clientside plane mover (thinker).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "client/clplanemover.h"
#include "client/cl_def.h"
#include "client/cl_player.h"
#include "world/map.h"
#include "world/p_players.h"

#include <doomsday/world/sector.h>
#include <doomsday/world/thinkers.h>
#include <de/logbuffer.h>

using namespace de;

thinker_s *ClPlaneMover::newThinker(Plane &plane, coord_t dest, float speed) // static
{
    Thinker th(Thinker::AllocateMemoryZone);
    th.setData(new ClPlaneMover(plane, dest, speed));

    // Add to the map.
    thinker_s *ptr = th.take();
    plane.map().thinkers().add(*ptr, false /* not public */);
    LOGDEV_MAP_XVERBOSE("New mover %p", ptr);

    // Immediate move?
    if (fequal(speed, 0))
    {
        // This will remove the thinker immediately if the move is ok.
        THINKER_DATA(*ptr, ClPlaneMover).think();
    }

    return ptr;
}

ClPlaneMover::ClPlaneMover(Plane &plane, coord_t dest, float speed)
    : _plane       (&plane)
    , _destination ( dest )
    , _speed       ( speed)
{
    // Set the right sign for speed.
    if (_destination < P_GetDoublep(_plane, DMU_HEIGHT))
    {
        _speed = -_speed;
    }

    // Update speed and target height.
    P_SetDoublep(_plane, DMU_TARGET_HEIGHT, _destination);
    P_SetFloatp(_plane, DMU_SPEED, _speed);

    _plane->addMover(*this);
}

ClPlaneMover::~ClPlaneMover()
{
    _plane->removeMover(*this);
}

void ClPlaneMover::think()
{
    LOG_AS("ClPlaneMover::think");

    // Can we think yet?
    if (!Cl_GameReady()) return;

    DE_ASSERT(_plane != 0);

    // The move is cancelled if the consolePlayer becomes obstructed.
    const bool freeMove = ClPlayer_IsFreeToMove(consolePlayer);

    // How's the gap?
    bool remove = false;
    const coord_t original = P_GetDoublep(_plane, DMU_HEIGHT);
    if (de::abs(_speed) > 0 && de::abs(_destination - original) > de::abs(_speed))
    {
        // Do the move.
        P_SetDoublep(_plane, DMU_HEIGHT, original + _speed);
    }
    else
    {
        // We have reached the destination.
        P_SetDoublep(_plane, DMU_HEIGHT, _destination);

        // This thinker can now be removed.
        remove = true;
    }

    LOGDEV_MAP_XVERBOSE_DEBUGONLY("plane height %f in sector #%i",
            P_GetDoublep(_plane, DMU_HEIGHT)
            << _plane->sector().indexInMap());

    // Let the game know of this.
    if (gx.SectorHeightChangeNotification)
    {
        gx.SectorHeightChangeNotification(_plane->sector().indexInMap());
    }

    // Make sure the client didn't get stuck as a result of this move.
    if (freeMove != ClPlayer_IsFreeToMove(consolePlayer))
    {
        LOG_MAP_VERBOSE("move blocked in sector #%i, undoing move")
                << _plane->sector().indexInMap();

        // Something was blocking the way! Go back to original height.
        P_SetDoublep(_plane, DMU_HEIGHT, original);

        if (gx.SectorHeightChangeNotification)
        {
            gx.SectorHeightChangeNotification(_plane->sector().indexInMap());
        }
    }
    else
    {
        // Can we remove this thinker?
        if (remove)
        {
            LOG_MAP_VERBOSE("finished in sector #%i")
                    << _plane->sector().indexInMap();

            // It stops.
            P_SetDoublep(_plane, DMU_SPEED, 0);

            _plane->map().thinkers().remove(thinker()); // we get deleted
        }
    }
}
