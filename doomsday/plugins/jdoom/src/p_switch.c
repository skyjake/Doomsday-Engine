/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
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
 * Switches, buttons. Two-state animation. Exits.
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "d_net.h"
#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Called when a thing uses a special line.
 * Only the front sides of lines are usable.
 */
boolean P_UseSpecialLine(mobj_t *thing, line_t *line, int side)
{
    xline_t *xline = P_XLine(line);

    // Extended functionality overrides old.
    if(XL_UseLine(line, side, thing))
        return true;

    // Err...
    // Use the back sides of VERY SPECIAL lines...
    if(side)
    {
        switch (xline->special)
        {
        case 124:
            // Sliding door open&close
            // UNUSED?
            break;

        default:
            return false;
            break;
        }
    }

    // Switches that other things can activate.
    if(!thing->player)
    {
        // never open secret doors
        if(P_GetIntp(line, DMU_FLAGS) & ML_SECRET)
            return false;

        switch (xline->special)
        {
        case 1:             // MANUAL DOOR RAISE
        case 32:                // MANUAL BLUE
        case 33:                // MANUAL RED
        case 34:                // MANUAL YELLOW
            break;

        default:
            return false;
            break;
        }
    }

    // do something
    switch (xline->special)
    {
        // MANUALS
    case 1:                 // Vertical Door
    case 26:                    // Blue Door/Locked
    case 27:                    // Yellow Door /Locked
    case 28:                    // Red Door /Locked

    case 31:                    // Manual door open
    case 32:                    // Blue locked door open
    case 33:                    // Red locked door open
    case 34:                    // Yellow locked door open

    case 117:                   // Blazing door raise
    case 118:                   // Blazing door open
        EV_VerticalDoor(line, thing);
        break;

        //UNUSED - Door Slide Open&Close
        // case 124:
        // EV_SlidingDoor (line, thing);
        // break;

        // SWITCHES
    case 7:
        // Build Stairs
        if(EV_BuildStairs(line, build8))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 9:
        // Change Donut
        if(EV_DoDonut(line))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 11:
        // Exit level
        if(cyclingMaps && mapCycleNoExit)
            break;

        // killough 10/98: prevent zombies from exiting levels
        if(thing->player && thing->player->health <= 0 && !cfg.zombiesCanExit)
        {
            S_StartSound(sfx_noway, thing);
            return false;
        }

        P_ChangeSwitchTexture(line, 0);
        G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
        break;

    case 14:
        // Raise Floor 32 and change texture
        if(EV_DoPlat(line, raiseAndChange, 32))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 15:
        // Raise Floor 24 and change texture
        if(EV_DoPlat(line, raiseAndChange, 24))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 18:
        // Raise Floor to next highest floor
        if(EV_DoFloor(line, raiseFloorToNearest))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 20:
        // Raise Plat next highest floor and change texture
        if(EV_DoPlat(line, raiseToNearestAndChange, 0))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 21:
        // PlatDownWaitUpStay
        if(EV_DoPlat(line, downWaitUpStay, 0))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 23:
        // Lower Floor to Lowest
        if(EV_DoFloor(line, lowerFloorToLowest))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 29:
        // Raise Door
        if(EV_DoDoor(line, normal))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 41:
        // Lower Ceiling to Floor
        if(EV_DoCeiling(line, lowerToFloor))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 71:
        // Turbo Lower Floor
        if(EV_DoFloor(line, turboLower))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 49:
        // Ceiling Crush And Raise
        if(EV_DoCeiling(line, crushAndRaise))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 50:
        // Close Door
        if(EV_DoDoor(line, close))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 51:
        // Secret EXIT
        if(cyclingMaps && mapCycleNoExit)
            break;

        // killough 10/98: prevent zombies from exiting levels
        if(thing->player && thing->player->health <= 0 && !cfg.zombiesCanExit)
        {
            S_StartSound(sfx_noway, thing);
            return false;
        }

        P_ChangeSwitchTexture(line, 0);
        G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, true);
        break;

    case 55:
        // Raise Floor Crush
        if(EV_DoFloor(line, raiseFloorCrush))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 101:
        // Raise Floor
        if(EV_DoFloor(line, raiseFloor))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 102:
        // Lower Floor to Surrounding floor height
        if(EV_DoFloor(line, lowerFloor))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 103:
        // Open Door
        if(EV_DoDoor(line, open))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 111:
        // Blazing Door Raise (faster than TURBO!)
        if(EV_DoDoor(line, blazeRaise))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 112:
        // Blazing Door Open (faster than TURBO!)
        if(EV_DoDoor(line, blazeOpen))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 113:
        // Blazing Door Close (faster than TURBO!)
        if(EV_DoDoor(line, blazeClose))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 122:
        // Blazing PlatDownWaitUpStay
        if(EV_DoPlat(line, blazeDWUS, 0))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 127:
        // Build Stairs Turbo 16
        if(EV_BuildStairs(line, turbo16))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 131:
        // Raise Floor Turbo
        if(EV_DoFloor(line, raiseFloorTurbo))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 133:
        // BlzOpenDoor BLUE
    case 135:
        // BlzOpenDoor RED
    case 137:
        // BlzOpenDoor YELLOW
        if(EV_DoLockedDoor(line, blazeOpen, thing))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 140:
        // Raise Floor 512
        if(EV_DoFloor(line, raiseFloor512))
            P_ChangeSwitchTexture(line, 0);
        break;

        // BUTTONS
    case 42:
        // Close Door
        if(EV_DoDoor(line, close))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 43:
        // Lower Ceiling to Floor
        if(EV_DoCeiling(line, lowerToFloor))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 45:
        // Lower Floor to Surrounding floor height
        if(EV_DoFloor(line, lowerFloor))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 60:
        // Lower Floor to Lowest
        if(EV_DoFloor(line, lowerFloorToLowest))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 61:
        // Open Door
        if(EV_DoDoor(line, open))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 62:
        // PlatDownWaitUpStay
        if(EV_DoPlat(line, downWaitUpStay, 1))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 63:
        // Raise Door
        if(EV_DoDoor(line, normal))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 64:
        // Raise Floor to ceiling
        if(EV_DoFloor(line, raiseFloor))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 66:
        // Raise Floor 24 and change texture
        if(EV_DoPlat(line, raiseAndChange, 24))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 67:
        // Raise Floor 32 and change texture
        if(EV_DoPlat(line, raiseAndChange, 32))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 65:
        // Raise Floor Crush
        if(EV_DoFloor(line, raiseFloorCrush))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 68:
        // Raise Plat to next highest floor and change texture
        if(EV_DoPlat(line, raiseToNearestAndChange, 0))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 69:
        // Raise Floor to next highest floor
        if(EV_DoFloor(line, raiseFloorToNearest))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 70:
        // Turbo Lower Floor
        if(EV_DoFloor(line, turboLower))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 114:
        // Blazing Door Raise (faster than TURBO!)
        if(EV_DoDoor(line, blazeRaise))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 115:
        // Blazing Door Open (faster than TURBO!)
        if(EV_DoDoor(line, blazeOpen))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 116:
        // Blazing Door Close (faster than TURBO!)
        if(EV_DoDoor(line, blazeClose))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 123:
        // Blazing PlatDownWaitUpStay
        if(EV_DoPlat(line, blazeDWUS, 0))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 132:
        // Raise Floor Turbo
        if(EV_DoFloor(line, raiseFloorTurbo))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 99:
        // BlzOpenDoor BLUE
    case 134:
        // BlzOpenDoor RED
    case 136:
        // BlzOpenDoor YELLOW
        if(EV_DoLockedDoor(line, blazeOpen, thing))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 138:
        // Light Turn On
        EV_LightTurnOn(line, 1);
        P_ChangeSwitchTexture(line, 1);
        break;

    case 139:
        // Light Turn Off
        EV_LightTurnOn(line, 35.0f/255.0f);
        P_ChangeSwitchTexture(line, 1);
        break;

    }

    return true;
}
