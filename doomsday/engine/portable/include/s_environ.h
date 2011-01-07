/**\file s_environ.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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
 * Environmental Sound Effects
 */

#ifndef LIBDENG_SOUND_ENVIRON_H
#define LIBDENG_SOUND_ENVIRON_H

#include "p_mapdata.h"

void            S_CalcSectorReverb(sector_t* sec);
void            S_DetermineSubSecsAffectingSectorReverb(gamemap_t* map);
material_env_class_t S_MaterialClassForName(const dduri_t* path);

#endif /* LIBDENG_SOUND_ENVIRON_H */

