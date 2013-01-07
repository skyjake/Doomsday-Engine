/** @file api_gameexport.h
 * Data structures for the engine/plugin interfaces.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_GAME_EXPORT_API_H
#define DOOMSDAY_GAME_EXPORT_API_H

#include "dd_share.h"
#include "api_mapedit.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The routines/data exported from the game plugin. @ingroup game
 */
typedef struct {
    size_t apiSize; ///< sizeof(game_export_t)

    // Base-level.
    void          (*PreInit) (gameid_t gameId);
    void          (*PostInit) (void);
    boolean       (*TryShutdown) (void);
    void          (*Shutdown) (void);
    void          (*UpdateState) (int step);
    int           (*GetInteger) (int id);
    void         *(*GetVariable) (int id);

    // Networking.
    int           (*NetServerStart) (int before);
    int           (*NetServerStop) (int before);
    int           (*NetConnect) (int before);
    int           (*NetDisconnect) (int before);
    long int      (*NetPlayerEvent) (int playernum, int type, void* data);
    int           (*NetWorldEvent) (int type, int parm, void* data);
    void          (*HandlePacket) (int fromplayer, int type, void* data,
                                   size_t length);

    // Tickers.
    void          (*Ticker) (timespan_t ticLength);

    // Responders.
    int           (*FinaleResponder) (const void* ddev);
    int           (*PrivilegedResponder) (event_t* ev);
    int           (*Responder) (event_t* ev);
    int           (*FallbackResponder) (event_t* ev);

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
    void          (*DrawViewPort) (int port, const RectRaw* portGeometry,
                        const RectRaw* windowGeometry, int player, int layer);

    /**
     * Draw over-viewport displays covering the whole game window. Typically
     * graphical user interfaces such as game menus are done here.
     *
     * @param windowSize  Dimensions of the game window in real screen pixels.
     */
    void          (*DrawWindow) (const Size2Raw* windowSize);

    // Miscellaneous.
    void          (*MobjThinker) ();
    coord_t       (*MobjFriction) (void* mobj); // Returns a friction factor.
    boolean       (*MobjCheckPositionXYZ) (struct mobj_s* mobj, coord_t x, coord_t y, coord_t z);
    boolean       (*MobjTryMoveXYZ) (struct mobj_s* mobj, coord_t x, coord_t y, coord_t z);
    void          (*SectorHeightChangeNotification)(int sectorIdx); // Applies necessary checks on objects.

    // Main structure sizes.
    size_t          mobjSize; // sizeof(mobj_t)
    size_t          polyobjSize; // sizeof(Polyobj)

    // Map data setup
    /**
     * Called before any data is read (with the number of items to be read) to
     * allow the game do any initialization it needs (eg create an array of its
     * own private data structures).
     */
    void          (*SetupForMapData) (int type, uint num);

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
                                                 valuetype_t type, void* data);
    // Post map setup
    /**
     * The engine calls this to inform the game of any changes it is
     * making to map data object to which the game might want to
     * take further action.
     */
    int           (*HandleMapObjectStatusReport) (int code, uint id, int dtype, void* data);
} game_export_t;

/// Function pointer for @c GetGameAPI() (exported by game plugin). @ingroup game
typedef game_export_t* (*GETGAMEAPI) (void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DOOMSDAY_GAME_EXPORT_API_H */
