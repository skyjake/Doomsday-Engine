/** @file clpolymover.cpp  Clientside polyobj mover (thinker).
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "client/clpolymover.h"

#include "client/cl_def.h"
#include "client/cl_player.h"

#include "world/world.h"
#include "world/map.h"
#include "world/p_players.h"

using namespace de;

void ClPolyMover_Thinker(ClPolyMover *mover)
{
    DENG2_ASSERT(mover != 0);

    LOG_AS("ClPolyMover_Thinker");

    Polyobj *po = mover->polyobj;
    if(mover->move)
    {
        // How much to go?
        Vector2d delta = Vector2d(po->dest) - Vector2d(po->origin);

        ddouble dist = M_ApproxDistance(delta.x, delta.y);
        if(dist <= po->speed || de::fequal(po->speed, 0))
        {
            // We'll arrive at the destination.
            mover->move = false;
        }
        else
        {
            // Adjust deltas to fit speed.
            delta = (delta / dist) * po->speed;
        }

        // Do the move.
        po->move(delta);
    }

    if(mover->rotate)
    {
        // How much to go?
        int dist = po->destAngle - po->angle;
        int speed = po->angleSpeed;

        //dist = FIX2FLT(po->destAngle - po->angle);
        //if(!po->angleSpeed || dist > 0   /*(abs(FLT2FIX(dist) >> 4) <= abs(((signed) po->angleSpeed) >> 4)*/
        //    /* && po->destAngle != -1*/) || !po->angleSpeed)
        if(!po->angleSpeed || ABS(dist >> 2) <= ABS(speed >> 2))
        {
            LOGDEV_MAP_XVERBOSE("Mover %p reached end of turn, destAngle=%i")
                    << de::dintptr(mover) << po->destAngle;

            // We'll arrive at the destination.
            mover->rotate = false;
        }
        else
        {
            // Adjust to speed.
            dist = /*FIX2FLT((int)*/ po->angleSpeed;
        }

        po->rotate(dist);
    }

    // Can we get rid of this mover?
    if(!mover->move && !mover->rotate)
    {
        /// @todo Do not assume the move is from the CURRENT map.
        App_World().map().deleteClPolyobj(mover);
    }
}
