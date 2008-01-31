/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * doomdata.h: Thing and linedef attributes
 */

#ifndef __DOOMDATA__
#define __DOOMDATA__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// Base plane ids.
enum {
    PLN_FLOOR,
    PLN_CEILING
};

typedef struct spawnspot_s {
    float           pos[2];
    float           height;
    short           angle;
    short           type;
    short           options;
} spawnspot_t;

extern spawnspot_t* things;

//
// LineDef attributes.
//

// Blocks monsters only.
#define ML_BLOCKMONSTERS    2

// In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SECRET           32

// Sound rendering: don't let sound cross two of these.
#define ML_SOUNDBLOCK       64

// Don't draw on the automap at all.
#define ML_DONTDRAW         128

// Set if already seen, thus drawn in automap.
#define ML_MAPPED           256

// FIXME! DJS - This is important!
// Doom64tc unfortunetly used non standard values for the line flags
// it implemented from BOOM. It will make life simpler if we simply
// update the Doom64TC IWAD rather than carry this on much further as
// once Doom64TC is released with 1.9.0 I imagine we'll see a bunch
// PWADs start cropping up.

// Allows a USE action to pass through a line with a special
//#define ML_PASSUSE          512

// If set allows any mobj to trigger the line's special
//#define ML_ALLTRIGGER       1024

// If set ALL flags NOT in DOOM v1.9 will be zeroed upon map load.
// ML_BLOCKING -> ML_MAPPED inc will persist.
//#define ML_INVALID          2048
//#define VALIDMASK           0x000001ff

// Anything can use line if this is set - kaiser
#define ML_ALLTRIGGER       512

#define ML_PASSUSE          1024

#define ML_BLOCKALL         2048

//
// Thing attributes.
//

// Appears in Easy skill modes
#define MTF_EASY            1

// Appears in Medium skill modes
#define MTF_MEDIUM          2

// Appears in Hard skill modes
#define MTF_HARD            4

// THING is deaf
#define MTF_DEAF            8

// Appears in Multiplayer game modes only
#define MTF_NOTSINGLE       16

// d64tc FIXME:
// DJS - Unfortunetly Doom64TC has already used the thing bits for
// other purposes so we should update the IWAD! No doubt that some
// of these can be pruned anyway...

// Doesn't appear in Deathmatch
//#define MTF_NOTDM           32

// Doesn't appear in Coop
//#define MTF_NOTCOOP         64

// THING is invulnerble and inert
//#define MTF_DORMANT         512

// Forces a MF_SPAWNCEILING enemies (d64 custom) to spawn on floor
#define MTF_FORCECEILING    32
#define MTF_WALKOFF         64
#define MTF_TRANSLUCENT     128
#define MTF_RESPAWN         256
#define MTF_SPAWNPLAYERZ    512
#define MTF_FLOAT           1024


// Special activation types
#define SPAC_CROSS      0          // when player crosses line
#define SPAC_USE        1          // when player uses line
#define SPAC_IMPACT     3          // when projectile hits line

#endif
