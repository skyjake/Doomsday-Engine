/** @file bossbrain.h  Playsim "Boss Brain" management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#ifndef LIBDOOM_PLAY_BOSSBRAIN_H
#define LIBDOOM_PLAY_BOSSBRAIN_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "jdoom.h"

#ifdef __cplusplus
class MapStateReader;
class MapStateWriter;

/**
 * Global state of boss brain.
 *
 * @ingroup libdoom
 */
class BossBrain
{
public:
    BossBrain();

    void clearTargets();

    int targetCount() const;

    void addTarget(struct mobj_s *mo);

    struct mobj_s *nextTarget();

    void write(MapStateWriter *msw) const;
    void read(MapStateReader *msr);

private:
    DE_PRIVATE(d)
};
#endif // __cplusplus

// C wrapper API ---------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#else
typedef void *BossBrain;
#endif

void BossBrain_ClearTargets(BossBrain *brain);

int BossBrain_TargetCount(const BossBrain *brain);

void BossBrain_AddTarget(BossBrain *brain, struct mobj_s *mo);

struct mobj_s *BossBrain_NextTarget(BossBrain *brain);

#ifdef __cplusplus
} // extern "C"
#endif

/// The One BossBrain instance.
DE_EXTERN_C BossBrain *theBossBrain;

#endif // LIBDOOM_PLAY_BOSSBRAIN_H
