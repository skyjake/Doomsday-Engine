/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * g_controls.h: Common code for game controls
 */

#ifndef __COMMON_CONTROLS_H__
#define __COMMON_CONTROLS_H__

// Game registered bindClasses
enum {
    GBC_CLASS1 = NUM_DDBINDCLASSES,
    GBC_CLASS2,
    GBC_CLASS3,
    GBC_MENUHOTKEY,
    GBC_CHAT,
    GBC_MESSAGE
};

// Control identifiers.
enum {
    CTL_SPEED = CTL_FIRST_GAME_CONTROL,
    CTL_STRAFE,
    CTL_LOOK_CENTER,
    CTL_USE,
    CTL_ATTACK,
    CTL_WEAPON1,
    CTL_WEAPON2,
    CTL_WEAPON3,
    CTL_WEAPON4,
    CTL_WEAPON5,
    CTL_WEAPON6,
    CTL_WEAPON7,
    CTL_WEAPON8,
    CTL_WEAPON9,
    CTL_WEAPON0,
    CTL_NEXT_WEAPON,
    CTL_PREV_WEAPON
};

// This structure replaced ticcmd as the place where players store the intentions
// of their human operators.
typedef struct playerbrain_s {
    float       forwardMove;        // 1.0 for maximum movement
    float       sideMove;           // 1.0 for maximum movement
    int         changeWeapon;       // WT_NOCHANGE, or the weapon to change to
    int         cycleWeapon;        // +1 or -1
    // Bits:
    uint        speed : 1;
    uint        use : 1;
    uint        attack : 1;
    uint        lookCenter : 1;
} playerbrain_t;

void        G_ControlRegister(void);
void        G_DefaultBindings(void);
void        G_RegisterBindClasses(void);

int         G_PrivilegedResponder(event_t *event);

boolean     G_AdjustControlState(event_t *ev);

void        G_LookAround(int pnum);
void        G_SetPause(boolean yes);

void        G_ResetMousePos(void);
void        G_ControlReset(int pnum);

float       G_GetLookOffset(int pnum);
void        G_ResetLookOffset(int pnum);

#endif
