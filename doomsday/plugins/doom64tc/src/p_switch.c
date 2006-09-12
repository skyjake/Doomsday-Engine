/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

#include "doom64tc.h"

#include "d_net.h"
#include "dmu_lib.h"
#include "p_mapsetup.h" // d64tc

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

button_t buttonlist[MAXBUTTONS];

// This array is treated as a hardcoded replacement for data that can be loaded
// from a lump, so we need to use little-endian byte ordering.
switchlist_t alphSwitchList[] = {
    // Doom shareware episode 1 switches
    {"SW1BRCOM", "SW2BRCOM", MACRO_SHORT(1)},
    {"SW1BRN1", "SW2BRN1", MACRO_SHORT(1)},
    {"SW1BRN2", "SW2BRN2", MACRO_SHORT(1)},
    {"SW1BRNGN", "SW2BRNGN", MACRO_SHORT(1)},
    {"SW1BROWN", "SW2BROWN", MACRO_SHORT(1)},
    {"SW1COMM", "SW2COMM", MACRO_SHORT(1)},
    {"SW1COMP", "SW2COMP", MACRO_SHORT(1)},
    {"SW1DIRT", "SW2DIRT", MACRO_SHORT(1)},
    {"SW1EXIT", "SW2EXIT", MACRO_SHORT(1)},
    {"SW1GRAY", "SW2GRAY", MACRO_SHORT(1)},
    {"SW1GRAY1", "SW2GRAY1", MACRO_SHORT(1)},
    {"SW1METAL", "SW2METAL", MACRO_SHORT(1)},
    {"SW1PIPE", "SW2PIPE", MACRO_SHORT(1)},
    {"SW1SLAD", "SW2SLAD", MACRO_SHORT(1)},
    {"SW1STARG", "SW2STARG", MACRO_SHORT(1)},
    {"SW1STON1", "SW2STON1", MACRO_SHORT(1)},
    {"SW1STON2", "SW2STON2", MACRO_SHORT(1)},
    {"SW1STONE", "SW2STONE", MACRO_SHORT(1)},
    {"SW1STRTN", "SW2STRTN", MACRO_SHORT(1)},

    // Doom registered episodes 2&3 switches
    {"SW1BLUE", "SW2BLUE", MACRO_SHORT(2)},
    {"SW1CMT", "SW2CMT", MACRO_SHORT(2)},
    {"SW1GARG", "SW2GARG", MACRO_SHORT(2)},
    {"SW1GSTON", "SW2GSTON", MACRO_SHORT(2)},
    {"SW1HOT", "SW2HOT", MACRO_SHORT(2)},
    {"SW1LION", "SW2LION", MACRO_SHORT(2)},
    {"SW1SATYR", "SW2SATYR", MACRO_SHORT(2)},
    {"SW1SKIN", "SW2SKIN", MACRO_SHORT(2)},
    {"SW1VINE", "SW2VINE", MACRO_SHORT(2)},
    {"SW1WOOD", "SW2WOOD", MACRO_SHORT(2)},

    // Doom II switches
    {"SW1PANEL", "SW2PANEL", MACRO_SHORT(3)},
    {"SW1ROCK", "SW2ROCK", MACRO_SHORT(3)},
    {"SW1MET2", "SW2MET2", MACRO_SHORT(3)},
    {"SW1WDMET", "SW2WDMET", MACRO_SHORT(3)},
    {"SW1BRIK", "SW2BRIK", MACRO_SHORT(3)},
    {"SW1MOD1", "SW2MOD1", MACRO_SHORT(3)},
    {"SW1ZIM", "SW2ZIM", MACRO_SHORT(3)},
    {"SW1STON6", "SW2STON6", MACRO_SHORT(3)},
    {"SW1TEK", "SW2TEK", MACRO_SHORT(3)},
    {"SW1MARB", "SW2MARB", MACRO_SHORT(3)},
    {"SW1SKULL", "SW2SKULL", MACRO_SHORT(3)},

    {"\0", "\0", MACRO_SHORT(0)}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int *switchlist;
static int max_numswitches;
static int numswitches;

// CODE --------------------------------------------------------------------

/*
 * Called at game initialization or when the engine's state must be updated
 * (eg a new WAD is loaded at runtime). This routine will populate the list
 * of known switches and buttons. This enables their texture to change when
 * activated, and in the case of buttons, change back after a timeout.
 *
 * This routine modified to read its data from a predefined lump or
 * PWAD lump called SWITCHES rather than a static table in this module to
 * allow wad designers to insert or modify switches.
 *
 * Lump format is an array of byte packed switchlist_t structures, terminated
 * by a structure with episode == -0. The lump can be generated from a
 * text source file using SWANTBLS.EXE, distributed with the BOOM utils.
 * The standard list of switches and animations is contained in the example
 * source text file DEFSWANI.DAT also in the BOOM util distribution.
 */

/*
 * DJS - We'll support this BOOM extension but we should discourage it's use
 *       and instead implement a better method for creating new switches.
 */
void P_InitSwitchList(void)
{
    int i, index, episode;
    int lump = W_CheckNumForName("SWITCHES");
    switchlist_t* sList = alphSwitchList;

    if(gamemode == registered || gamemode == retail)
        episode = 2;
    else if(gamemode == commercial)
        episode = 3;
    else
        episode = 1;

    // Has a custom SWITCHES lump been loaded?
    if(lump > 0)
    {
        Con_Message("P_InitSwitchList: \"SWITCHES\" lump found. Reading switches...\n");
        sList = (switchlist_t *) W_CacheLumpNum(lump, PU_STATIC);
    }

    for(index = 0, i = 0; ; ++i)
    {
        if(index+1 >= max_numswitches)
        {
            switchlist = realloc(switchlist, sizeof *switchlist *
                (max_numswitches = max_numswitches ? max_numswitches*2 : 8));
        }

        if(SHORT(sList[i].episode) <= episode)
        {
            if(!SHORT(sList[i].episode))
                break;

            switchlist[index++] = R_TextureNumForName(sList[i].name1);
            switchlist[index++] = R_TextureNumForName(sList[i].name2);
            VERBOSE(Con_Message("P_InitSwitchList: ADD (\"%s\" | \"%s\" #%d)\n",
                                sList[i].name1, sList[i].name2,
                                SHORT(sList[i].episode)));
        }
    }

    numswitches = index / 2;
    switchlist[index] = -1;
}

/*
 * Start a button (retriggerable switch) counting down till it turns off.
 *
 * Passed the linedef the button is on, which texture on the sidedef contains
 * the button, the texture number of the button, and the time the button is
 * to remain active in gametics.
 */
void P_StartButton(line_t *line, bwhere_e w, int texture, int time)
{
    int     i;

    // See if button is already pressed
    for(i = 0; i < MAXBUTTONS; ++i)
    {
        if(buttonlist[i].btimer && buttonlist[i].line == line)
            return;
    }

    for(i = 0; i < MAXBUTTONS; i++)
    {
        if(!buttonlist[i].btimer) // use first unused element of list
        {
            buttonlist[i].line = line;
            buttonlist[i].where = w;
            buttonlist[i].btexture = texture;
            buttonlist[i].btimer = time;
            buttonlist[i].soundorg =
                P_GetPtrp(P_GetPtrp(line, DMU_FRONT_SECTOR), DMU_SOUND_ORIGIN);
            return;
        }
    }

    Con_Error("P_StartButton: no button slots left!");
}

/*
 * Function that changes wall texture.
 * Tell it if switch is ok to use again (1=yes, it's a button).
 */
void P_ChangeSwitchTexture(line_t *line, int useAgain)
{
    int     texTop;
    int     texMid;
    int     texBot;
    int     i;
    int     sound;
    side_t*     sdef = P_GetPtrp(line, DMU_SIDE0);
    sector_t*   frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);

    if(!useAgain)
        P_XLine(line)->special = 0;

    texTop = P_GetIntp(sdef, DMU_TOP_TEXTURE);
    texMid = P_GetIntp(sdef, DMU_MIDDLE_TEXTURE);
    texBot = P_GetIntp(sdef, DMU_BOTTOM_TEXTURE);

    sound = sfx_swtchn;

    // EXIT SWITCH?
    if(P_XLine(line)->special == 11)
        sound = sfx_swtchx;

    for(i = 0; i < numswitches * 2; i++)
    {
        if(switchlist[i] == texTop)
        {
            S_StartSound(sound, P_GetPtrp(frontsector,
                                          DMU_SOUND_ORIGIN));

            P_SetIntp(sdef, DMU_TOP_TEXTURE, switchlist[i ^ 1]);

            if(useAgain)
                P_StartButton(line, top, switchlist[i], BUTTONTIME);

            return;
        }
        else if(switchlist[i] == texMid)
        {
            S_StartSound(sound, P_GetPtrp(frontsector,
                                          DMU_SOUND_ORIGIN));

            P_SetIntp(sdef, DMU_MIDDLE_TEXTURE, switchlist[i ^ 1]);

            if(useAgain)
                P_StartButton(line, middle, switchlist[i], BUTTONTIME);

            return;
        }
        else if(switchlist[i] == texBot)
        {
            S_StartSound(sound, P_GetPtrp(frontsector,
                                          DMU_SOUND_ORIGIN));

            P_SetIntp(sdef, DMU_BOTTOM_TEXTURE, switchlist[i ^ 1]);

            if(useAgain)
                P_StartButton(line, bottom, switchlist[i], BUTTONTIME);

            return;
        }
    }
}

/**
 * d64tc
 * DJS - What the heck is this used for??
 * FIXME: Use XG instead. We do not want hardcoded kludges like this.
 *        Now that we have line data references in XG this kind of thing
 *        can be done there much more easily.
 */
int EV_DestoryLineShield(line_t* line)
{
    int         i, k, tag, flags;
    sector_t   *sec;
    line_t     *li;
    xline_t    *xline;

    if(!line)
        return false;

    tag = P_XLine(line)->tag;

    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        if(P_XSector(sec)->tag != tag)
            continue; // wrong sector.

        for(k = 0; k < P_GetIntp(sec, DMU_LINE_COUNT); ++k)
        {
            li = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | k);
            xline = P_XLine(li);
            if(xline->tag != 999 || !xline->special ||
               !(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
                continue;

            xline->special = 0;

            P_SetIntp(li, DMU_SIDE0_OF_LINE | DMU_MIDDLE_TEXTURE, 0);
            P_SetIntp(li, DMU_SIDE1_OF_LINE | DMU_MIDDLE_TEXTURE, 0);

            flags = P_GetIntp(li, DMU_FLAGS);
            flags &= ~ML_BLOCKING;
            P_SetIntp(li, DMU_FLAGS, flags);
        }
    }

    return true;
}

/**
 * d64tc
 * DJS - What the heck is this used for??
 * FIXME: Use XG instead. We do not want hardcoded kludges like this.
 *        Now that we have line data references in XG this kind of thing
 *        can be done there much more easily.
 */
int EV_SwitchTextureFree(line_t* line)
{
    int         i, k, tag, flags;
    sector_t   *sec;
    line_t     *li;
    xline_t    *xline;

    if(!line)
        return false;

    tag = P_XLine(line)->tag;

    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        if(P_XSector(sec)->tag != tag)
            continue; // wrong sector.

        for(k = 0; k < P_GetIntp(sec, DMU_LINE_COUNT); ++k)
        {
            li = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | k);
            xline = P_XLine(li);
            if(xline->special != 418)
                continue;

            P_SetIntp(li, DMU_SIDE0_OF_LINE | DMU_MIDDLE_TEXTURE, xline->tag);

            // If there is a back side, set that too.
            if(P_GetPtrp(li, DMU_SIDE1) != NULL)
                P_SetIntp(li, DMU_SIDE1_OF_LINE | DMU_MIDDLE_TEXTURE,
                          xline->tag);

            flags = P_GetIntp(li, DMU_FLAGS);
            flags &= ~ML_BLOCKING;
            P_SetIntp(li, DMU_FLAGS, flags);
        }
    }

    return true;
}

/**
 * d64tc
 * DJS - What the heck is this used for??
 * FIXME: Use XG instead. We do not want hardcoded kludges like this.
 *        Now that we have line data references in XG this kind of thing
 *        can be done there much more easily.
 */
int EV_ActivateSpecial(line_t *line)
{
    int         i, k, tag;
    int         bitmip;
    sector_t   *sec;
    line_t     *li;
    xline_t    *xline;
    side_t     *back;

    if(!line)
        return false;

    back = P_GetPtrp(line, DMU_SIDE1);
    if(!back)
        return false; // We need a twosided line

    tag = P_XLine(line)->tag;
    bitmip = P_GetIntp(back, DMU_MIDDLE_TEXTURE_OFFSET_Y);

    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        if(P_XSector(sec)->tag != tag)
            continue; // wrong sector.

        for(k = 0; k < P_GetIntp(sec, DMU_LINE_COUNT); ++k)
        {
            li = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | k);
            xline = P_XLine(li);
            if(xline->special != 418)
                continue;

            if(bitmip == 0)
                xline->special = P_XSector(sec)->tag;
            else
                xline->special = bitmip;
        }
    }

    return true;
}

/**
 * d64tc
 * FIXME: Use XG instead. We do not want hardcoded kludges like this.
 *        Now that we have line data references in XG this kind of thing
 *        can be done there much more easily.
 */
void P_SetSectorColor(line_t *line)
{
    int         i, tag;
    side_t     *front, *back;
    sector_t   *sec;
    byte        rgb[4];

    if(!line)
        return;

    front = P_GetPtrp(line, DMU_SIDE0);
    back = P_GetPtrp(line, DMU_SIDE0);

    if(!back)
        return; // We need a twosided line.

    // Determine the color based on line texture offsets!?
    rgb[0] = (byte) P_GetIntp(front, DMU_MIDDLE_TEXTURE_OFFSET_X);
    rgb[1] = (byte) P_GetIntp(front, DMU_MIDDLE_TEXTURE_OFFSET_Y);
    rgb[2] = (byte) P_GetIntp(back, DMU_MIDDLE_TEXTURE_OFFSET_X);

    rgb[0] = CLAMP(rgb[0], 0, 255);
    rgb[1] = CLAMP(rgb[1], 0, 255);
    rgb[2] = CLAMP(rgb[2], 0, 255);

    tag = P_XLine(line)->tag;
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        if(P_XSector(sec)->tag != tag)
            continue; // wrong sector.

        P_SetBytepv(sec, DMU_COLOR, rgb);
    }
}

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
    case 525: // d64tc
    case 526: // d64tc
    case 527: // d64tc
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
        if(EV_DoLockedDoor(line, blazeRaise, thing)) // d64tc was "blazeOpen"
            P_ChangeSwitchTexture(line, 1);
        break;

    case 138:
        // Light Turn On
        EV_LightTurnOn(line, 255);
        P_ChangeSwitchTexture(line, 1);
        break;

    case 139:
        // Light Turn Off
        EV_LightTurnOn(line, 35);
        P_ChangeSwitchTexture(line, 1);
        break;

    case 343: // d64tc - BlzOpenDoor LaserPowerup 1
    case 344: // d64tc - BlzOpenDoor LaserPowerup 2
    case 345: // d64tc - BlzOpenDoor LaserPowerup 3
        if(EV_DoLockedDoor(line, blazeOpen, thing))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 414: // d64tc
        if(EV_DoPlat(line, upWaitDownStay, 1))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 416: // d64tc
        if(EV_DoSplitDoor(line, lowerToEight, raiseToHighest))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 417: // d64tc
        if(EV_DestoryLineShield(line))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 419: // d64tc
        if(EV_SwitchTextureFree(line))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 421: // d64tc
        if(EV_ActivateSpecial(line))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 424: // d64tc
        if(EV_DoCeiling(line, customCeiling))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 425: // d64tc
        if(EV_DoCeiling(line, customCeiling))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 428: // d64tc
        if(EV_DoFloor(line, customFloor))
            P_ChangeSwitchTexture(line, 1);
        break;

    case 429: // d64tc
        if(EV_DoFloor(line, customFloor))
            P_ChangeSwitchTexture(line, 0);
        break;
    }

    return true;
}
