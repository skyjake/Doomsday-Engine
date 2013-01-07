/**
 * @file p_switch.h
 * Common playsim routines relating to switches.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
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

#ifndef LIBCOMMON_PLAY_SWITCH_H
#define LIBCOMMON_PLAY_SWITCH_H

#include "doomsday.h"
#include "p_mobj.h"

#define BUTTONTIME              (TICSPERSEC) // 1 second, in ticks.

typedef struct {
    thinker_t thinker;
    int timer;
    SideDef* side;
    SideDefSection section;
    material_t* material;
} materialchanger_t;

/**
 * Called at game initialization or when the engine's state must be updated
 * (eg a new WAD is loaded at runtime). This routine will populate the list
 * of known switches and buttons. This enables their texture to change when
 * activated, and in the case of buttons, change back after a timeout.
 */
void P_InitSwitchList(void);

/**
 * @param side          Sidedef where the surface to be changed is found.
 * @param ssurfaceID    Id of the sidedef surface to be changed.
 * @param sound         If non-zero, play this sound, ELSE the sound to
 *                      play will be taken from the switchinfo. Note that
 *                      a sound will play iff a switch state change occurs
 *                      and param silent is non-zero.
 * @param silent        @c true = no sound will be played.
 * @param tics          @c <= 0 = A permanent change.
 *                      @c  > 0 = Change back after this many tics.
 */
boolean P_ToggleSwitch2(SideDef* side, SideDefSection ssurfaceID, int sound,
    boolean silent, int tics);

/**
 * @param side          Sidedef where the switch to be changed is found.
 * @param sound         If non-zero, play this sound, ELSE the sound to
 *                      play will be taken from the switchinfo. Note that
 *                      a sound will play iff a switch state change occurs
 *                      and param silent is non-zero.
 * @param silent        @c true = no sound will be played.
 * @param tics          @c <= 0 = A permanent change.
 *                      @c  > 0 = Change back after this many tics.
 */
boolean P_ToggleSwitch(SideDef* side, int sound, boolean silent, int tics);

/**
 * To be called to execute any action(s) assigned to the specified LineDef's
 * special.
 */
boolean P_UseSpecialLine(mobj_t* activator, LineDef* line, int side);

void T_MaterialChanger(materialchanger_t* mchanger);

#endif /// LIBCOMMON_PLAY_SWITCH_H
