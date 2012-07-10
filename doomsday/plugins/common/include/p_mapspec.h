/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Crossed line special list and other Map utility functions.
 */

#ifndef LIBCOMMON_MAP_SPEC_H
#define LIBCOMMON_MAP_SPEC_H

#include "p_iterlist.h"

/// For crossed line specials.
extern iterlist_t* spechit;

typedef struct spreadsoundtoneighborsparams_s {
    Sector* baseSec;
    int soundBlocks;
    mobj_t* soundTarget;
} spreadsoundtoneighborsparams_t;

/// Recursively traverse adjacent sectors, sound blocking lines cut off traversal.
void P_RecursiveSound(struct mobj_s* soundTarget, Sector* sec, int soundBlocks);

#endif /* LIBCOMMON_MAP_SPEC_H */
