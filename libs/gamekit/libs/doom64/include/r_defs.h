/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * r_defs.h: shared data struct definitions.
 */

#ifndef __R_DEFS__
#define __R_DEFS__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "p_xg.h"

#define SP_floororigheight  planes[PLN_FLOOR].origHeight
#define SP_ceilorigheight   planes[PLN_CEILING].origHeight

// Stair build flags.
#define BL_BUILT            0x1
#define BL_WAS_BUILT        0x2
#define BL_SPREADED         0x4

typedef struct xsector_s {
    short           special;
    short           tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int             soundTraversed;

    // thing that made a sound (or null)
    struct mobj_s*  soundTarget;

    // thinker_t for reversable actions
    void*           specialData;

    byte            blFlags; // Used during stair building.

    // stone, metal, heavy, etc...
    byte            seqType;       // NOT USED ATM

    struct {
        float       origHeight;
    } planes[2];    // {floor, ceiling}

    float           origLight;
    float           origRGB[3];
    xgsector_t*     xg;
} xsector_t;

/**
 * xline_t flags:
 */

#define ML_BLOCKMONSTERS        0x0002 // Blocks monsters only.

/**
 * If not present on a two-sided line suppress the back sector and instead
 * consider the line as if it were one-sided. For mod compatibility purposes.
 */
#define ML_TWOSIDED             0x0004

#define ML_SECRET               0x0020 // In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SOUNDBLOCK           0x0040 // Sound rendering: don't let sound cross two of these.
#define ML_DONTDRAW             0x0080 // Don't draw on the automap at all.
#define ML_MAPPED               0x0100 // Set if already seen, thus drawn in automap.

// FIXME! DJS - This is important!
// Doom64tc unfortunetly used non standard values for the line flags
// it implemented from BOOM. It will make life simpler if we simply
// update the Doom64TC IWAD rather than carry this on much further as
// once jDoom64 is released with 1.9.0 I imagine we'll see a bunch
// PWADs start cropping up.

//#define ML_PASSUSE            0x0200 // Allows a USE action to pass through a line with a special
//#define ML_ALLTRIGGER         0x0400 // If set allows any mobj to trigger the line's special

#define ML_ALLTRIGGER           0x0200 // Anything can use line if this is set - kaiser
#define ML_PASSUSE              0x0400
#define ML_BLOCKALL             0x0800

#define ML_VALID_MASK           (ML_BLOCKMONSTERS|ML_TWOSIDED|ML_SECRET|ML_SOUNDBLOCK|ML_DONTDRAW|ML_MAPPED|ML_ALLTRIGGER|ML_PASSUSE|ML_BLOCKALL)

typedef struct xline_s {
    short           special;
    short           tag;
    short           flags;
    // Has been rendered at least once and needs to appear in the map,
    // for each player.
    dd_bool         mapped[MAXPLAYERS];
    int             validCount;

    // Extended generalized lines.
    xgline_t*       xg;

    // jDoom64 specific:
    short           useOn;
} xline_t;

// Our private map data structures.
DE_EXTERN_C xsector_t* xsectors;
DE_EXTERN_C xline_t* xlines;

// If true we are in the process of setting up a map.
DE_EXTERN_C dd_bool mapSetup;

#endif
