/** @file p_switch.h  Common playsim routines relating to switches.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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
#include "dmu_lib.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

#define BUTTONTIME              (TICSPERSEC) // 1 second, in ticks.

typedef struct materialchanger_s {
    thinker_t thinker;
    int timer;
    Side *side;
    SideSection section;
    world_Material *material;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} materialchanger_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Called at game initialization or when the engine's state must be updated
 * (eg a new WAD is loaded at runtime). This routine will populate the list
 * of known switches and buttons. This enables their texture to change when
 * activated, and in the case of buttons, change back after a timeout.
 */
void P_InitSwitchList(void);

/**
 * @param side          Side where the surface to be changed is found.
 * @param ssurfaceID    Id of the side surface to be changed.
 * @param sound         If non-zero, play this sound, ELSE the sound to
 *                      play will be taken from the switchinfo. Note that
 *                      a sound will play iff a switch state change occurs
 *                      and param silent is non-zero.
 * @param silent        @c true = no sound will be played.
 * @param tics          @c <= 0 = A permanent change.
 *                      @c  > 0 = Change back after this many tics.
 */
dd_bool P_ToggleSwitch2(Side *side, SideSection ssurfaceID, int sound,
    dd_bool silent, int tics);

/**
 * @param side          Side where the switch to be changed is found.
 * @param sound         If non-zero, play this sound, ELSE the sound to
 *                      play will be taken from the switchinfo. Note that
 *                      a sound will play iff a switch state change occurs
 *                      and param silent is non-zero.
 * @param silent        @c true = no sound will be played.
 * @param tics          @c <= 0 = A permanent change.
 *                      @c  > 0 = Change back after this many tics.
 */
dd_bool P_ToggleSwitch(Side *side, int sound, dd_bool silent, int tics);

/**
 * To be called to execute any action(s) assigned to the specified Line's
 * special.
 */
dd_bool P_UseSpecialLine(mobj_t *activator, Line *line, int side);

void T_MaterialChanger(void *materialChangedThinker);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_PLAY_SWITCH_H
