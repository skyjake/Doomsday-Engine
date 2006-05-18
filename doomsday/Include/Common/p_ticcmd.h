/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_ticcmd.h: Ticcmds
 */

#ifndef __COMMON_TICCMD_H__
#define __COMMON_TICCMD_H__

typedef struct ticcmd_s {
    char            forwardMove;   // *2048 for move
    char            sideMove;      // *2048 for move
    short           angle;         // <<16 for angle delta
    short           pitch;         // view pitch
    char            fly;           // Fly up/down; fall down
    byte            arti;
    // Actions:
    char            attack;
    char            use;
    char            jump;
    short           changeWeapon;  // Absolute number, next/prev
    char            pause;
    char            suicide;       // Now ignored in ticcmds
} ticcmd_t;

// Special flyspeed for falling down.
#define TICCMD_FALL_DOWN        DDMINCHAR

// Special weapon numbers for the changeWeapon field.
#define TICCMD_NEXT_WEAPON      DDMINSHORT
#define TICCMD_PREV_WEAPON      (DDMINSHORT + 1)

#endif
