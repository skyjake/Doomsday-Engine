/** @file clpolymover.h  Clientside polyobj mover (thinker).
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

#ifndef DENG_CLIENT_POLYMOVER_H
#define DENG_CLIENT_POLYMOVER_H

#include "api_thinker.h"
#include "Polyobj"
#include <doomsday/world/thinkerdata.h>

/**
 * Polyobj movement thinker.
 *
 * @ingroup world
 */
class ClPolyMover : public ThinkerData
{
    Polyobj *_polyobj;
    bool _move;
    bool _rotate;

public:
    ClPolyMover(Polyobj &pobj, bool moving, bool rotating);
    ~ClPolyMover();

    void think();

    static thinker_s *newThinker(Polyobj &polyobj, bool moving, bool rotating);

protected:
    static void thinkerFunc(thinker_s *mover);
};

#endif // DENG_CLIENT_POLYMOVER_H
