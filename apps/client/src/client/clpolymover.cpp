/** @file clpolymover.cpp  Clientside polyobj mover (thinker).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/map.h"
#include "world/p_players.h"
#include "world/polyobjdata.h"

#include <doomsday/world/thinkers.h>
#include <de/logbuffer.h>

using namespace de;

thinker_s *ClPolyMover::newThinker(Polyobj &polyobj, bool moving, bool rotating) // static
{
    // If there is an existing mover, modify it.
    if (ClPolyMover *mover = polyobj.data().as<PolyobjData>().mover())
    {
        mover->_move   = moving;
        mover->_rotate = rotating;
        return &mover->thinker();
    }

    Thinker th(Thinker::AllocateMemoryZone);
    th.setData(new ClPolyMover(polyobj, moving, rotating));

    thinker_s *ptr = th.take();
    polyobj.map().thinkers().add(*ptr, false /*not public*/);

    LOGDEV_MAP_XVERBOSE("New polymover %p for polyobj #%i.", ptr << polyobj.indexInMap());

    return ptr;
}

ClPolyMover::ClPolyMover(Polyobj &pobj, bool moving, bool rotating)
    : _polyobj (&pobj    )
    , _move    ( moving  )
    , _rotate  ( rotating)
{
    _polyobj->data().as<PolyobjData>().addMover(*this);
}

ClPolyMover::~ClPolyMover()
{
    _polyobj->data().as<PolyobjData>().removeMover(*this);
}

void ClPolyMover::think()
{
    LOG_AS("ClPolyMover::think");

    Polyobj *po = _polyobj;
    if (_move)
    {
        // How much to go?
        Vec2d delta = Vec2d(po->dest) - Vec2d(po->origin);

        ddouble dist = M_ApproxDistance(delta.x, delta.y);
        if (dist <= po->speed || de::fequal(po->speed, 0))
        {
            // We'll arrive at the destination.
            _move = false;
        }
        else
        {
            // Adjust deltas to fit speed.
            delta = (delta / dist) * po->speed;
        }

        // Do the move.
        po->move(delta);
    }

    if (_rotate)
    {
        // How much to go?
        int dist = po->destAngle - po->angle;
        int speed = po->angleSpeed;

        //dist = FIX2FLT(po->destAngle - po->angle);
        //if (!po->angleSpeed || dist > 0   /*(abs(FLT2FIX(dist) >> 4) <= abs(((signed) po->angleSpeed) >> 4)*/
        //    /* && po->destAngle != -1*/) || !po->angleSpeed)
        if (!po->angleSpeed || ABS(dist >> 2) <= ABS(speed >> 2))
        {
            LOGDEV_MAP_XVERBOSE("Mover %p reached end of turn, destAngle=%i",
                                &thinker() << po->destAngle);

            // We'll arrive at the destination.
            _rotate = false;
        }
        else
        {
            // Adjust to speed.
            dist = /*FIX2FLT((int)*/ po->angleSpeed;
        }

        po->rotate(dist);
    }

    // Can we get rid of this mover?
    if (!_move && !_rotate)
    {
        _polyobj->map().thinkers().remove(thinker()); // we get deleted
    }
}
