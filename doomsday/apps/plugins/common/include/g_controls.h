/**\file g_controls.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * Common code for game controls
 */

#ifndef LIBCOMMON_CONTROLS_H
#define LIBCOMMON_CONTROLS_H

#include <api_player.h>

// Control identifiers.
enum {
    CTL_SPEED = CTL_FIRST_GAME_CONTROL,
    //CTL_STRAFE,
    CTL_LOOK_CENTER,
    CTL_LOOK_PITCH,     ///< Absolute lookdir pitch.
    CTL_HEAD_YAW,       ///< Offset applied to viewing direction only (yaw); not body turn angle.
    CTL_BODY_YAW,       ///< Absolute offset applied to player angle.
    CTL_FALL_DOWN,
    CTL_USE,
    CTL_ATTACK,
    CTL_JUMP,
    CTL_WEAPON1,
    CTL_WEAPON2,
    CTL_WEAPON3,
    CTL_WEAPON4,
    CTL_WEAPON5,
    CTL_WEAPON6,
    CTL_WEAPON7,
    CTL_WEAPON8,
    CTL_WEAPON9,
#if __JDOOM64__
    CTL_WEAPON10,
#endif
    CTL_WEAPON0,
    CTL_NEXT_WEAPON,
    CTL_PREV_WEAPON,
#if __JHERETIC__ || __JHEXEN__
    CTL_USE_ITEM,
    CTL_NEXT_ITEM,
    CTL_PREV_ITEM,
    CTL_PANIC,
#endif
#if __JHERETIC__
    CTL_TOME_OF_POWER,
    CTL_INVISIBILITY,
    CTL_FLY,
    CTL_TORCH,
    CTL_HEALTH,
    CTL_SUPER_HEALTH,
    CTL_TELEPORT,
    CTL_FIREBOMB,
    CTL_INVULNERABILITY,
    CTL_EGG,
#endif
#if __JHEXEN__
    CTL_FLY,
    CTL_TORCH,
    CTL_HEALTH,
    CTL_MYSTIC_URN,
    CTL_KRATER,
    CTL_SPEED_BOOTS,
    CTL_BLAST_RADIUS,
    CTL_TELEPORT,
    CTL_TELEPORT_OTHER,
    CTL_FIREBOMB,
    CTL_POISONBAG,
    CTL_INVULNERABILITY,
    CTL_DARK_SERVANT,
    CTL_EGG,
#endif
    CTL_MAP,
    CTL_MAP_PAN_X,
    CTL_MAP_PAN_Y,
    CTL_MAP_ZOOM,
    CTL_MAP_ZOOM_MAX,
    CTL_MAP_FOLLOW,
    CTL_MAP_ROTATE,
    CTL_MAP_MARK_ADD,
    CTL_MAP_MARK_CLEAR_ALL,
    CTL_HUD_SHOW,
    CTL_SCORE_SHOW,
    CTL_LOG_REFRESH
};

// This structure replaced ticcmd as the place where players store the intentions
// of their human operators.
typedef struct playerbrain_s {
    float       forwardMove;        // 1.0 for maximum movement
    float       sideMove;           // 1.0 for maximum movement
    float       upMove;             // 1.0 for maximum movement
    int         changeWeapon;       // WT_NOCHANGE, or the weapon to change to
    int         cycleWeapon;        // +1 or -1
#if __JHERETIC__ || __JHEXEN__
    int         cycleInvItem;       // +1 or -1
#endif
    // Bits:
    uint        speed : 1;
    uint        use : 1;
    uint        lunge : 1;
    uint        attack : 1;
    uint        lookCenter : 1;
    uint        fallDown : 1;
    uint        jump : 1;
    uint        mapToggle : 1;
    uint        mapZoomMax : 1;
    uint        mapFollow : 1;
    uint        mapRotate : 1;
    uint        mapMarkAdd : 1;
    uint        mapMarkClearAll : 1;
    uint        hudShow : 1;
    uint        scoreShow : 1;
    uint        doReborn: 1; // Set when the player wishes to be reborn.
#if __JHERETIC__ || __JHEXEN__
    uint        useInvItem: 1;
#endif
    uint        logRefresh: 1;
} playerbrain_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register the CVars and CCmds for input/controls.
 */
void        G_ControlRegister(void);

void        G_DefineControls(void);
void        G_DefaultBindings(void);
void        G_RegisterBindClasses(void);

dd_bool     G_UsingSharpInput();

/**
 * Reset controls for all local players.
 */
void G_ControlReset(void);

float       G_GetLookOffset(int pnum);
void        G_ResetLookOffset(int pnum);

void        P_PlayerThinkHeadTurning(int pnum, timespan_t ticLength);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_CONTROLS_H */
