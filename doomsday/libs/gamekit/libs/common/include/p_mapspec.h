/** @file p_mapspec.h  Crossed line special list utilities.
 *
 * Line Tag handling. Line and Sector groups. Specialized iterators, etc...
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_PLAYSIM_MAP_SPECIAL_H
#define LIBCOMMON_PLAYSIM_MAP_SPECIAL_H

#include "doomsday.h"
#include "p_iterlist.h"

/// For crossed line specials.
DE_EXTERN_C iterlist_t *spechit;

#ifdef __cplusplus
extern "C" {
#endif

/// Recursively traverse adjacent sectors, sound blocking lines cut off traversal.
void P_RecursiveSound(struct mobj_s *soundTarget, Sector *sec, int soundBlocks);

//dd_bool P_SectorTagIsBusy(int tag);

void P_NotifySectorFinished(int tag);
void P_NotifyPolyobjFinished(int tag);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // LIBCOMMON_PLAYSIM_MAP_SPECIAL_H
