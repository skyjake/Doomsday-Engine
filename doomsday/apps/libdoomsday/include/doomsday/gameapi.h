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

/// General constants (not to be used with Get/Set).
/// @note Many of these have become unused as better APIs are introduced for
/// sharing information.
enum {
    DD_DISABLE,
    DD_ENABLE,
    DD_UNUSED22, // DD_MASK
    DD_YES,
    DD_NO,
    DD_UNUSED23, // DD_MATERIAL
    DD_UNUSED24, // DD_OFFSET
    DD_UNUSED25, // DD_HEIGHT
    DD_UNUSED2,
    DD_UNUSED3,
    DD_UNUSED26, // DD_COLOR_LIMIT
    DD_PRE,
    DD_POST,
    DD_PLUGIN_VERSION_SHORT,
    DD_PLUGIN_VERSION_LONG,
    DD_UNUSED27, // DD_HORIZON
    DD_OLD_GAME_ID,
    DD_UNUSED32, // DD_DEF_MOBJ
    DD_UNUSED33, // DD_DEF_MOBJ_BY_NAME
    DD_UNUSED29, // DD_DEF_STATE
    DD_UNUSED34, // DD_DEF_SPRITE
    DD_DEF_SOUND,
    DD_UNUSED14, // DD_DEF_MUSIC
    DD_UNUSED13, // DD_DEF_MAP_INFO
    DD_UNUSED28, // DD_DEF_TEXT
    DD_UNUSED36, // DD_DEF_VALUE
    DD_UNUSED37, // DD_DEF_VALUE_BY_INDEX
    DD_DEF_LINE_TYPE,
    DD_DEF_SECTOR_TYPE,
    DD_PSPRITE_BOB_X,
    DD_PSPRITE_BOB_Y,
    DD_UNUSED19, // DD_DEF_FINALE_AFTER
    DD_UNUSED20, // DD_DEF_FINALE_BEFORE
    DD_UNUSED21, // DD_DEF_FINALE
    DD_RENDER_RESTART_PRE,
    DD_RENDER_RESTART_POST,
    DD_UNUSED35, // DD_DEF_SOUND_BY_NAME
    DD_DEF_SOUND_LUMPNAME,
    DD_UNUSED16, // DD_ID
    DD_LUMP,
    DD_UNUSED17, // DD_CD_TRACK
    DD_UNUSED30, // DD_SPRITE
    DD_UNUSED31, // DD_FRAME
    DD_GAME_CONFIG, ///< String: dm/co-op, jumping, etc.
    DD_PLUGIN_NAME, ///< (e.g., jdoom, jheretic etc..., suitable for use with filepaths)
    DD_PLUGIN_NICENAME, ///< (e.g., jDoom, MyGame:Episode2 etc..., fancy name)
    DD_PLUGIN_HOMEURL,
    DD_PLUGIN_DOCSURL,
    DD_DEF_ACTION,
    DD_UNUSED15, // DD_DEF_MUSIC_CDTRACK

    // Non-integer/special values for Set/Get
    DD_MAP_BOUNDING_BOX,
    DD_UNUSED4, // DD_TRACE_ADDRESS
    DD_SPRITE_REPLACEMENT, ///< Sprite <-> model replacement.
    DD_ACTION_LINK, ///< State action routine addresses.
    DD_UNUSED10, // DD_MAP_NAME
    DD_UNUSED11, // DD_MAP_AUTHOR
    DD_MAP_MUSIC,
    DD_MAP_MIN_X,
    DD_MAP_MIN_Y,
    DD_MAP_MAX_X,
    DD_MAP_MAX_Y,
    DD_WINDOW_WIDTH,
    DD_WINDOW_HEIGHT,
    DD_WINDOW_HANDLE,
    DD_DYNLIGHT_TEXTURE,
    DD_GAME_EXPORTS,
    DD_POLYOBJ_COUNT,
    DD_XGFUNC_LINK, ///< XG line classes
    DD_SHARED_FIXED_TRIGGER_OBSOLETE, ///< obsolete
    DD_GAMETIC,
    DD_NOTIFY_PLAYER_WEAPON_CHANGED, ///< a player's weapon changed (including powerups)
    DD_NOTIFY_PSPRITE_STATE_CHANGED, ///< a player's psprite state has changed
    DD_UNUSED7, // DD_OPENBOTTOM
    DD_UNUSED8, // DD_LOWFLOOR
    DD_CPLAYER_THRUST_MUL_OBSOLETE, ///< obsolete
    DD_GRAVITY,
    DD_PSPRITE_OFFSET_X, ///< 10x
    DD_PSPRITE_OFFSET_Y, ///< 10x
    DD_PSPRITE_LIGHTLEVEL_MULTIPLIER,
    DD_TORCH_RED,
    DD_TORCH_GREEN,
    DD_TORCH_BLUE,
    DD_TORCH_ADDITIVE,
    DD_TM_FLOOR_Z,              ///< output from P_CheckPosition
    DD_TM_CEILING_Z,            ///< output from P_CheckPosition
    DD_SHIFT_DOWN,
    DD_GAME_RECOMMENDS_SAVING,  ///< engine asks whether game should be saved (e.g., when upgrading) (game's GetInteger)
    DD_NOTIFY_GAME_SAVED,       ///< savegame was written
    DD_DEFS                     ///< engine definition database (DED)
};

/**
 * The routines/data exported from the game plugin. @ingroup game
 *
 * @todo Get rid of this struct in favor of individually queried export points.
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
