/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

/**
 * x_api.c : Doomsday API setup and interaction - jHexen specific
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "hu_menu.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

/**
 * jHexen's entry points.
 */

// Initialization
void    H2_PreInit(void);
void    H2_PostInit(void);

// Timeing loop
void    H2_Ticker(timespan_t ticLength);

// Drawing
void    D_Display(void);
void    H2_EndFrame(void);
void    G_Drawer(void);
void    G_Drawer2(void);

// Input responders
boolean G_Responder(event_t *ev);
int     G_PrivilegedResponder(event_t *event);

// Map Data
void    P_SetupForMapData(int type, uint num);

// Map Objects
float   P_MobjGetFriction(struct mobj_s *mo);
void    P_MobjThinker(mobj_t *mobj);
void    P_BlasterMobjThinker(mobj_t *mobj);

// Misc
void    H2_ConsoleBg(int *width, int *height);

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
void    H2_Shutdown(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char gameModeString[];
extern char gameConfigString[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The interface to the Doomsday engine.
game_export_t gx;
game_import_t gi;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Get a 32-bit integer value.
 */
int G_GetInteger(int id)
{
    switch(id)
    {
    case DD_GAME_DMUAPI_VER:
        return DMUAPI_VER;

    default:
        break;
    }

    // ID not recognized, return NULL.
    return 0;
}

/**
 * Get a pointer to the value of a variable. Added for 64-bit support.
 */
void *G_GetVariable(int id)
{
    static float bob[2];

    switch(id)
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
            "\n"GAMENAMETEXT" is based on Hexen v1.1 by Raven Software.";

    case DD_ACTION_LINK:
        return actionlinks;

    case DD_ALT_MOBJ_THINKER:
        return P_BlasterMobjThinker;

    case DD_XGFUNC_LINK:
        return NULL;

    case DD_PSPRITE_BOB_X:
        if(players[consoleplayer].morphTics > 0)
        {
            bob[VX] = 0;
        }
        else
        {
            bob[VX] = 1 + (cfg.bobWeapon * players[consoleplayer].bob) *
                FIX2FLT(finecosine[(128 * leveltime) & FINEMASK]);
        }

        return &bob[VX];

    case DD_PSPRITE_BOB_Y:
        if(players[consoleplayer].morphTics > 0)
        {
            bob[VY] = 0;
        }
        else
        {
            bob[VY] = 32 + (cfg.bobWeapon * players[consoleplayer].bob) *
                FIX2FLT(finesine[(128 * leveltime) & FINEMASK & (FINEANGLES / 2 - 1)]);
        }

        return &bob[VY];

    default:
        break;
    }

    // ID not recognized, return NULL.
    return 0;
}

/**
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t *GetGameAPI(game_import_t *imports)
{
    // Take a copy of the imports, but only copy as much data as is
    // allowed and legal.
    memset(&gi, 0, sizeof(gi));
    memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

    // Clear all of our exports.
    memset(&gx, 0, sizeof(gx));

    // Fill in the data for the exports.
    gx.apiSize = sizeof(gx);
    gx.PreInit = H2_PreInit;
    gx.PostInit = H2_PostInit;
    gx.Shutdown = H2_Shutdown;
    gx.Ticker = H2_Ticker;
    gx.G_Drawer = G_Drawer;
    gx.G_Drawer2 = G_Drawer2;
    gx.PrivilegedResponder = (boolean (*)(event_t *)) G_PrivilegedResponder;
    gx.FallbackResponder = Hu_MenuResponder;
    gx.G_Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (float (*)(void *)) P_MobjGetFriction;
    gx.EndFrame = H2_EndFrame;
    gx.ConsoleBackground = H2_ConsoleBg;
    gx.UpdateState = G_UpdateState;
#undef Get
    gx.GetInteger = G_GetInteger;
    gx.GetVariable = G_GetVariable;
    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;

    // The structure sizes.
    gx.ticcmdSize = sizeof(ticcmd_t);
    gx.mobjSize = sizeof(mobj_t);

    gx.SetupForMapData = P_SetupForMapData;

    // These really need better names. Ideas?
    gx.HandleMapDataProperty = P_HandleMapDataProperty;
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    gx.HandleMapObjectStatusReport = P_HandleMapObjectStatusReport;
    return &gx;
}
