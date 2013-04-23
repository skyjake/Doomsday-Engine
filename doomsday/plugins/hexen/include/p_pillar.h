/**\file p_pillar.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_P_PILLAR_H
#define LIBHEXEN_P_PILLAR_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

typedef struct {
    thinker_t thinker;
    Sector* sector;
    float ceilingSpeed;
    float floorSpeed;
    coord_t floorDest;
    coord_t ceilingDest;
    int direction;
    int crush;
} pillar_t;

#ifdef __cplusplus
extern "C" {
#endif

void T_BuildPillar(pillar_t* pillar);
int EV_BuildPillar(Line* line, byte* args, boolean crush);
int EV_OpenPillar(Line* line, byte* args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_P_PILLAR_H
