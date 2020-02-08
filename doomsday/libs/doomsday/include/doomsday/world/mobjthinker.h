/** @file mobjthinker.h  Map object thinker.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "mobj.h"
#include "thinker.h"

class LIBDOOMSDAY_PUBLIC MobjThinker : public ThinkerT<mobj_t>
{
public:
    MobjThinker(AllocMethod alloc = AllocateStandard) : ThinkerT(Mobj_Sizeof(), alloc) {}
    MobjThinker(const mobj_t &existingToCopy) : ThinkerT(existingToCopy, Mobj_Sizeof()) {}
    MobjThinker(mobj_t *existingToTake) : ThinkerT(existingToTake, Mobj_Sizeof()) {}

    static void zap(mobj_t &mob) { ThinkerT::zap(mob, Mobj_Sizeof()); }
};
