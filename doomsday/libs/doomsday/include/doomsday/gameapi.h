/** @file api_gameexport.h  Data structures for the engine/plugin interfaces.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_GAME_API_H
#define LIBDOOMSDAY_GAME_API_H

#include <de/legacy/types.h>
#include <de/legacy/rect.h>
#include "world/valuetype.h"

#ifdef __cplusplus
extern "C" {
#endif

struct event_s;

/// General constants.
enum {
    DD_DISABLE = 0,
    DD_ENABLE = 1,
    DD_YES = 2,
    DD_NO = 3,
    DD_PRE = 4,
    DD_POST = 5,

    DD_GAME_CONFIG = 0x100,         ///< String: dm/co-op, jumping, etc.
    DD_GAME_RECOMMENDS_SAVING,      ///< engine asks whether game should be saved (e.g., when upgrading) (game's GetInteger)

    DD_NOTIFY_GAME_SAVED = 0x200,   ///< savegame was written
    DD_NOTIFY_PLAYER_WEAPON_CHANGED, ///< a player's weapon changed (including powerups)
    DD_NOTIFY_PSPRITE_STATE_CHANGED, ///< a player's psprite state has changed

    DD_PLUGIN_NAME = 0x300,         ///< (e.g., jdoom, jheretic etc..., suitable for use with filepaths)
    DD_PLUGIN_NICENAME,             ///< (e.g., jDoom, MyGame:Episode2 etc..., fancy name)
    DD_PLUGIN_VERSION_SHORT,
    DD_PLUGIN_VERSION_LONG,
    DD_PLUGIN_HOMEURL,
    DD_PLUGIN_DOCSURL,

    DD_DEF_SOUND = 0x400,
    DD_DEF_LINE_TYPE,
    DD_DEF_SECTOR_TYPE,
    DD_DEF_SOUND_LUMPNAME,
    DD_DEF_ACTION,
    DD_LUMP,

    DD_ACTION_LINK = 0x500,         ///< State action routine addresses.
    DD_XGFUNC_LINK,                 ///< XG line classes
    DD_MOBJ_SIZE,
    DD_POLYOBJ_SIZE,

    DD_TM_FLOOR_Z = 0x600,          ///< output from P_CheckPosition
    DD_TM_CEILING_Z,                ///< output from P_CheckPosition

    DD_PSPRITE_BOB_X = 0x700,
    DD_PSPRITE_BOB_Y,
    DD_RENDER_RESTART_PRE,
    DD_RENDER_RESTART_POST
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LIBDOOMSDAY_GAME_API_H
