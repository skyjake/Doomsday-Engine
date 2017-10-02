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

#include <de/types.h>
#include <de/rect.h>
#include "world/valuetype.h"

#ifdef __cplusplus
extern "C" {
#endif

struct event_s;

/// General constants.
/// @note Many of these have become unused as better APIs are introduced for
/// sharing information.
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

    DD_FUNC_OBJECT_STATE_INFO_STR,  ///< Information about mobjs in plain text Info format.
    DD_FUNC_RESTORE_OBJECT_STATE,   ///< Restore object state according to a parsed Info block.

    DD_TM_FLOOR_Z = 0x600,          ///< output from P_CheckPosition
    DD_TM_CEILING_Z,                ///< output from P_CheckPosition

    DD_PSPRITE_BOB_X = 0x700,
    DD_PSPRITE_BOB_Y,
    DD_RENDER_RESTART_PRE,
    DD_RENDER_RESTART_POST
};

/**
 * The routines/data exported from the game plugin. @ingroup game
 *
 * @todo Get rid of this struct in favor of individually queried entrypoints.
 */
typedef struct {
    size_t apiSize; ///< sizeof(game_export_t)

    // Base-level.
    void          (*PreInit) (char const *gameId);
    void          (*PostInit) (void);
    dd_bool       (*TryShutdown) (void);
    void          (*Shutdown) (void);
    void          (*UpdateState) (int step);
    int           (*GetInteger) (int id);
    void         *(*GetVariable) (int id);

    // Networking.
    int           (*NetServerStart) (int before);
    int           (*NetServerStop) (int before);
    int           (*NetConnect) (int before);
    int           (*NetDisconnect) (int before);
    long int      (*NetPlayerEvent) (int playernum, int type, void *data);
    int           (*NetWorldEvent) (int type, int parm, void *data);
    void          (*HandlePacket) (int fromplayer, int type, void *data,
                                   size_t length);

    // Tickers.
    void          (*Ticker) (timespan_t ticLength);

    // Responders.
    int           (*FinaleResponder) (void const *ddev);
    int           (*PrivilegedResponder) (struct event_s *ev);
    int           (*Responder) (struct event_s *ev);
    int           (*FallbackResponder) (struct event_s *ev);

    // Refresh.
    void          (*BeginFrame) (void);

    /**
     * Called at the end of a refresh frame. This is the last chance the game
     * will have at updating the engine state before rendering of the frame
     * begins. Once rendering begins, the viewer can still be updated however
     * any changes will not take effect until the subsequent frame. Therefore
     * this is the place where games should strive to update the viewer to
     * ensure latency-free world refresh.
     */
    void          (*EndFrame) (void);

    /**
     * Draw the view port display of the identified console @a player.
     * The engine will configure a orthographic GL projection in real pixel
     * dimensions prior to calling this.
     *
     * Example subdivision of the game window into four view ports:
     * <pre>
     *     (0,0)-----------------------. X
     *       | .--------. |            |
     *       | | window | |            |
     *       | '--------' |            |
     *       |    port #0 |    port #1 |
     *       |-------------------------|
     *       |            |            |
     *       |            |            |
     *       |            |            |
     *       |    port #2 |    port #3 |
     *       '--------------------(xn-1, yn-1)
     *       Y               Game Window
     * </pre>
     *
     * @param port  Logical number of this view port.
     * @param portGeometry  Geometry of the view port in real screen pixels.
     * @param windowGeometry  Geometry of the view window within the port, in
     *                        real screen pixels.
     *
     * @param player  Console player number associated with the view port.
     * @param layer  Logical layer identifier for the content to be drawn:
     *      - 0: The bottom-most layer and the one which generally contains the
     *        call to R_RenderPlayerView.
     *      - 1: Displays to be drawn on top of view window (after bordering),
     *        such as the player HUD.
     */
    void          (*DrawViewPort) (int port, RectRaw const *portGeometry,
                                   RectRaw const *windowGeometry, int player, int layer);

    /**
     * Draw over-viewport displays covering the whole game window. Typically
     * graphical user interfaces such as game menus are done here.
     *
     * @param windowSize  Dimensions of the game window in real screen pixels.
     */
    void          (*DrawWindow) (Size2Raw const *windowSize);

    // Miscellaneous.
    void          (*MobjThinker) (void *mobj);
    coord_t       (*MobjFriction) (struct mobj_s const *mobj);  // Returns a friction factor.
    dd_bool       (*MobjCheckPositionXYZ) (struct mobj_s *mobj, coord_t x, coord_t y, coord_t z);
    dd_bool       (*MobjTryMoveXYZ) (struct mobj_s *mobj, coord_t x, coord_t y, coord_t z);
    void          (*SectorHeightChangeNotification)(int sectorIdx);  // Applies necessary checks on objects.

    // Main structure sizes.
    size_t          mobjSize;     ///< sizeof(mobj_t)
    size_t          polyobjSize;  ///< sizeof(Polyobj)

    // Map setup

    /**
     * Called once a map change (i.e., P_MapChange()) has completed to allow the
     * game to do any post change finalization it needs to do at this time.
     */
    void          (*FinalizeMapChange) (void const *uri);

    /**
     * Called when trying to assign a value read from the map data (to a
     * property known to us) that we don't know what to do with.
     *
     * (eg the side->toptexture field contains a text string that
     * we don't understand but the game might).
     *
     * @return The action code returned by the game depends on the context.
     */
    int           (*HandleMapDataPropertyValue) (uint id, int dtype, int prop,
                                                 valuetype_t type, void *data);
    // Post map setup
    /**
     * The engine calls this to inform the game of any changes it is
     * making to map data object to which the game might want to
     * take further action.
     */
    int           (*HandleMapObjectStatusReport) (int code, uint id, int dtype, void *data);
} game_export_t;

/// Function pointer for @c GetGameAPI() (exported by game plugin). @ingroup game
typedef game_export_t *(*GETGAMEAPI) (void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LIBDOOMSDAY_GAME_API_H
