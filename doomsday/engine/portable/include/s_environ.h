/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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

/*
 * s_environ.h: Environmental Sound Effects
 */

#ifndef __DOOMSDAY_SOUND_ENVIRON_H__
#define __DOOMSDAY_SOUND_ENVIRON_H__

typedef enum {
    MATTYPE_UNKNOWN = -1,
    MATTYPE_METAL = 0,
    MATTYPE_ROCK,
    MATTYPE_WOOD,
    MATTYPE_CLOTH,
    NUM_MATERIAL_TYPES
} materialtype_t;

void            S_CalcSectorReverb(struct sector_s *sec);
void            S_DetermineSubSecsAffectingSectorReverb(void);
materialtype_t  S_MaterialTypeForName(const char *name, boolean isFlat);

#endif
