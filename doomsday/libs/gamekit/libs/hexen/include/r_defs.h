/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * r_defs.h:
 */

#ifndef __R_DEFS_H__
#define __R_DEFS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"
#include "p_mobj.h"
#include "s_sequence.h"

typedef struct xsector_s {
    short           special, tag;
    int             soundTraversed; // 0 = untraversed, 1,2 = sndlines -1
    mobj_t*         soundTarget; // Thing that made a sound (or null)
    seqtype_t       seqType; // Stone, metal, heavy, etc...
    void*           specialData; // thinker_t for reversable actions
} xsector_t;

typedef struct xline_s {
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
    short           flags;
    // Has been rendered at least once and needs to appear in the map,
    // for each player.
    dd_bool         mapped[MAXPLAYERS];
    int             validCount;
} xline_t;

DE_EXTERN_C xline_t* xlines;
DE_EXTERN_C xsector_t* xsectors;

#endif
