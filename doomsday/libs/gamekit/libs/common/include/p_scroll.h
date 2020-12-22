/** @file p_scroll.h  Common surface material scroll thinker.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_THINKER_SCROLL_H
#define LIBCOMMON_THINKER_SCROLL_H

#include "doomsday.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

typedef struct scroll_s {
  thinker_t thinker;
  void *dmuObject; ///< Affected DMU object (either a sector or a side).
  int elementBits; ///< Identifies which subelements of the dmuObject are affected.
  float offset[2]; ///< [x, y] scroll vector delta.

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} scroll_t;

#ifdef __cplusplus
extern "C" {
#endif

void T_Scroll(scroll_t *scroll);

scroll_t *P_SpawnSideMaterialOriginScroller(Side *side, short special);

scroll_t *P_SpawnSectorMaterialOriginScroller(Sector *sector, uint planeId, short special);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_THINKER_SCROLL_H
