/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_waggle.h:
 */

#ifndef __P_WAGGLE_H__
#define __P_WAGGLE_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

typedef enum {
    WS_EXPAND = 1,
    WS_STABLE,
    WS_REDUCE
} wagglestate_e;

typedef struct {
    thinker_t       thinker;
    sector_t*       sector;
    float           originalHeight;
    float           accumulator;
    float           accDelta;
    float           targetScale;
    float           scale;
    float           scaleDelta;
    int             ticker;
    wagglestate_e   state;
} waggle_t;

void        T_FloorWaggle(waggle_t* waggle);
boolean     EV_StartFloorWaggle(int tag, int height, int speed, int offset,
                                int timer);
#endif
