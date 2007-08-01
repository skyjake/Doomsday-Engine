/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * Doomsday API setup and interaction - jHeretic specific
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

/*
 *  jHeretic's entry points
 */

// Initialization
void    H_PreInit(void);
void    H_PostInit(void);
void    R_InitTranslationTables(void);

// Timeing loop
void    H_Ticker(timespan_t ticLength);

// Drawing
void    D_Display(void);
void    D_Display2(void);
void    H_EndFrame(void);

// Input responders
int     G_PrivilegedResponder(event_t *event);
boolean G_Responder(event_t *ev);
int     D_PrivilegedResponder(event_t *event);

// Map Data
void    P_SetupForMapData(int type, uint num);

// Map Objects
fixed_t P_GetMobjFriction(struct mobj_s *mo);
void    P_MobjThinker(mobj_t *mobj);
void    P_BlasterMobjThinker(mobj_t *mobj);

// Misc
void H_ConsoleBg(int *width, int *height);

// Game state changes
void    G_UpdateState(int step);

// Network
int     D_NetServerStarted(int before);
int     D_NetServerClose(int before);
int     D_NetConnect(int before);
int     D_NetDisconnect(int before);

long int D_NetPlayerEvent(int plrNumber, int peType, void *data);
int     D_NetWorldEvent(int type, int parm, void *data);

// Handlers
void    D_HandlePacket(int fromplayer, int type, void *data, int length);
int     P_HandleMapDataProperty(uint id, int dtype, int prop, int type, void *data);
int     P_HandleMapDataPropertyValue(uint id, int dtype, int prop, int type, void *data);
int     P_HandleMapObjectStatusReport(int code, uint id, int dtype, void *data);

// Shutdown
void    H_Shutdown(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char gameModeString[];
extern char gameConfigString[];

extern struct xgclass_s xgClasses[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The interface to the Doomsday engine.
game_import_t gi;
game_export_t gx;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Get a 32-bit integer value.
 */
int G_GetInteger(int id)
{
    switch (id)
    {
    case DD_PSPRITE_BOB_X:
        return (int) (FRACUNIT +
            FixedMul(FixedMul(FRACUNIT * cfg.bobWeapon, players[consoleplayer].bob),
                     finecosine[(128 * leveltime) & FINEMASK]));

    case DD_PSPRITE_BOB_Y:
        return (int) (32 * FRACUNIT +
            FixedMul(FixedMul(FRACUNIT * cfg.bobWeapon, players[consoleplayer].bob),
                     finesine[(128 * leveltime) & FINEMASK & (FINEANGLES / 2 - 1)]));

    case DD_GAME_DMUAPI_VER:
        return DMUAPI_VER;

    default:
        break;
    }
    // ID not recognized, return NULL.
    return 0;
}

/*
 * Get a pointer to the value of a variable. Added for 64-bit support.
 */
void *G_GetVariable(int id)
{
    switch (id)
    {
    case DD_GAME_NAME:
        return GAMENAMETEXT;

    case DD_GAME_ID:
        return GAMENAMETEXT " " VERSION_TEXT;

    case DD_GAME_MODE:
        return gameModeString;

    case DD_GAME_CONFIG:
        return gameConfigString;

    case DD_VERSION_SHORT:
        return VERSION_TEXT;

    case DD_VERSION_LONG:
        return VERSIONTEXT
            "\n"GAMENAMETEXT" is based on Heretic v1.3 by Raven Software.";

    case DD_ACTION_LINK:
        return actionlinks;

    case DD_ALT_MOBJ_THINKER:
        return P_BlasterMobjThinker;

    case DD_XGFUNC_LINK:
        return xgClasses;
    }
    // ID not recognized, return NULL.
    return 0;
}

/*
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t *GetGameAPI(game_import_t * imports)
{
    // Take a copy of the imports, but only copy as much data as is
    // allowed and legal.
    memset(&gi, 0, sizeof(gi));
    memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

    // Clear all of our exports.
    memset(&gx, 0, sizeof(gx));

    // Fill in the data for the exports.
    gx.apiSize = sizeof(gx);
    gx.PreInit = H_PreInit;
    gx.PostInit = H_PostInit;
    gx.Shutdown = H_Shutdown;
    gx.G_Drawer = D_Display;
    gx.Ticker = H_Ticker;
    gx.G_Drawer2 = D_Display2;
    gx.PrivilegedResponder = (boolean (*)(event_t *)) G_PrivilegedResponder;
    gx.FallbackResponder = M_Responder;
    gx.G_Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (fixed_t (*)(void *)) P_GetMobjFriction;
    gx.EndFrame = H_EndFrame;
    gx.ConsoleBackground = H_ConsoleBg;
    gx.UpdateState = G_UpdateState;
#undef Get
    gx.GetInteger = G_GetInteger;
    gx.GetVariable = G_GetVariable;
    gx.R_Init = R_InitTranslationTables;

    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;

    // The structure sizes.
    gx.ticcmd_size = sizeof(ticcmd_t);

    gx.SetupForMapData = P_SetupForMapData;

    gx.HandleMapDataProperty = P_HandleMapDataProperty;
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    gx.HandleMapObjectStatusReport = P_HandleMapObjectStatusReport;
    return &gx;
}
