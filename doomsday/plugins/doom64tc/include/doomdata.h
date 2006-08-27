/* $Id: doomdata.h 3554 2006-08-12 14:06:30Z danij $
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Thing and linedef attributes
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

// This is the common thing_t
typedef struct thing_s {
    short           x;
    short           y;
    short           height;
    short           angle;
    short           type;
    short           options;
} thing_t;

extern thing_t* things;

//
// LineDef attributes.
//

// Solid, is an obstacle.
#define ML_BLOCKING     1

// Blocks monsters only.
#define ML_BLOCKMONSTERS    2

// Backside will not be present at all if not two sided.
#define ML_TWOSIDED     4

// upper texture unpegged
#define ML_DONTPEGTOP       8

// lower texture unpegged
#define ML_DONTPEGBOTTOM    16

// In AutoMap: don't map as two sided: IT'S A SECRET!
#define ML_SECRET       32

// Sound rendering: don't let sound cross two of these.
#define ML_SOUNDBLOCK       64

// Don't draw on the automap at all.
#define ML_DONTDRAW     128

// Set if already seen, thus drawn in automap.
#define ML_MAPPED       256

// Allows a USE action to pass through a line with a special
#define ML_PASSUSE    512

// If set allows any mobj to trigger the line's special
#define ML_ALLTRIGGER 1024

// If set ALL flags NOT in DOOM v1.9 will be zeroed upon map load.
// ML_BLOCKING -> ML_MAPPED inc will persist.
#define ML_INVALID    2048
#define VALIDMASK     0x000001ff

//
// Thing attributes.
//

// Appears in Easy skill modes
#define MTF_EASY    1

// Appears in Medium skill modes
#define MTF_MEDIUM    2

// Appears in Hard skill modes
#define MTF_HARD    4

// THING is deaf
#define MTF_DEAF    8

// Appears in Multiplayer game modes only
#define MTF_NOTSINGLE    16

// Doesn't appear in Deathmatch
#define MTF_NOTDM    32

// Doesn't appear in Coop
#define MTF_NOTCOOP    64

// THING is invulnerble and inert
#define MTF_DORMANT    512

// Special activation types
#define SPAC_CROSS      0          // when player crosses line
#define SPAC_USE        1          // when player uses line
#define SPAC_IMPACT     3          // when projectile hits line

#endif
