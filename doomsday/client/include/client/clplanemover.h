/** @file clplanemover.h  Clientside plane mover (thinker).
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_PLANEMOVER_H
#define DENG_CLIENT_PLANEMOVER_H

#include "api_thinker.h"

/**
 * Plane movement thinker. Makes changes to planes using DMU.
 *
 * @ingroup world
 */
struct ClPlaneMover
{
    thinker_t thinker;
    int sectorIndex;
    int planeIndex;
    coord_t destination;
    float speed;
};

void ClPlaneMover_Thinker(ClPlaneMover *mover);

#endif // DENG_CLIENT_PLANEMOVER_H
