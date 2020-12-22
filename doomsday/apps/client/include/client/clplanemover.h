/** @file clplanemover.h  Clientside plane mover (thinker).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_PLANEMOVER_H
#define DE_CLIENT_PLANEMOVER_H

#if !defined(__CLIENT__)
#  error clplanemover.h is client only
#endif

#include "api_thinker.h"
#include "world/plane.h"
#include <doomsday/world/thinkerdata.h>

/**
 * Plane movement thinker. Makes changes to planes using DMU.
 *
 * @ingroup world
 */
class ClPlaneMover : public ThinkerData
{
    Plane *_plane;
    coord_t _destination;
    float _speed;

public:
    ClPlaneMover(Plane &plane, coord_t dest, float speed);
    ~ClPlaneMover();

    void think();

    /**
     * Constructs a new plane mover and adds its thinker to the map.
     *
     * @param plane  Plane to move.
     * @param dest   Destination height.
     * @param speed  Speed of move.
     *
     * @return The mover thinker. Ownership retained by the Plane's Map.
     */
    static thinker_s *newThinker(Plane &plane, coord_t dest, float speed);
};

#endif // DE_CLIENT_PLANEMOVER_H
