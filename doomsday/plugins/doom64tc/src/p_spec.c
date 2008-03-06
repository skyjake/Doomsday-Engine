/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/**
 * p_spec.c: Implements special effects.
 *
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing,
 * or shooting special lines, or by timed thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "doom64tc.h"

#include "m_argv.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_player.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// Animating textures and planes

// In Doomsday these are handled via DED definitions.
// In BOOM they invented the ANIMATED lump for the same purpose.

// This struct is directly read from the lump.
// So its important we keep it aligned.
#pragma pack(1)
typedef struct animdef_s {
    /* Do NOT change these members in any way */
    signed char istexture;  //  if false, it is a flat (instead of bool)
    char        endname[9];
    char        startname[9];
    int         speed;
} animdef_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_CrossSpecialLine(linedef_t *line, int side, mobj_t *thing);
static void P_ShootSpecialLine(mobj_t *thing, linedef_t *line);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//// \todo From jHeretic, replace!
int    *TerrainTypes;
struct terraindef_s {
    char   *name;
    int     type;
} TerrainTypeDefs[] =
{
    {"FWATER1", FLOOR_WATER},
    {"LAVA1",   FLOOR_LAVA},
    {"BLOOD1",  FLOOR_BLOOD},
    {"NUKAGE1", FLOOR_SLIME},
    {"SLIME01", FLOOR_SLUDGE},
    {"END",     -1}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * \bug This routine originated from jHeretic, we need to rewrite it!
 */
void P_InitTerrainTypes(void)
{
    int                 i, lump, size;

    size = Get(DD_NUMLUMPS) * sizeof(int);
    TerrainTypes = Z_Malloc(size, PU_STATIC, 0);
    memset(TerrainTypes, 0, size);

    for(i = 0; TerrainTypeDefs[i].type != -1; ++i)
    {
        lump = W_CheckNumForName(TerrainTypeDefs[i].name);
        if(lump != -1)
        {
            TerrainTypes[lump] = TerrainTypeDefs[i].type;
        }
    }
}

/**
 * Return the terrain type of the specified flat.
 *
 * @param flatlumpnum   The flat lump number to check.
 */
int P_FlatToTerrainType(int flatlumpnum)
{
    if(flatlumpnum != -1)
        return TerrainTypes[flatlumpnum];
    else
        return FLOOR_SOLID;
}

/**
 * Returns the terrain type of the specified sector, plane.
 *
 * @param sec           The sector to check.
 * @param plane         The plane id to check.
 */
int P_GetTerrainType(sector_t* sec, int plane)
{
    int                 flatnum =
        P_GetIntp(sec, (plane? DMU_CEILING_MATERIAL : DMU_FLOOR_MATERIAL));

    if(flatnum != -1)
        return TerrainTypes[flatnum];
    else
        return FLOOR_SOLID;
}

/**
 * From PrBoom:
 * Load the table of animation definitions, checking for existence of
 * the start and end of each frame. If the start doesn't exist the sequence
 * is skipped, if the last doesn't exist, BOOM exits.
 *
 * Wall/Flat animation sequences, defined by name of first and last frame,
 * The full animation sequence is given using all lumps between the start
 * and end entry, in the order found in the WAD file.
 *
 * This routine modified to read its data from a predefined lump or
 * PWAD lump called ANIMATED rather than a static table in this module to
 * allow wad designers to insert or modify animation sequences.
 *
 * Lump format is an array of byte packed animdef_t structures, terminated
 * by a structure with istexture == -1. The lump can be generated from a
 * text source file using SWANTBLS.EXE, distributed with the BOOM utils.
 * The standard list of switches and animations is contained in the example
 * source text file DEFSWANI.DAT also in the BOOM util distribution.
 */

/**
 * DJS - We'll support this BOOM extension by reading the data and then
 *       registering the new animations into Doomsday using the animation
 *       groups feature.
 *
 *       Support for this extension should be considered depreciated.
 *       All new features should be added, accessed via DED.
 */
void P_InitPicAnims(void)
{
    int                 i, j;
    int                 groupNum;
    int                 startFrame, endFrame, ticsPerFrame;
    int                 numFrames;
    int                 lump = W_CheckNumForName("ANIMATED");
    const char         *name;
    animdef_t          *animdefs;

    // Has a custom ANIMATED lump been loaded?
    if(lump > 0)
    {
        Con_Message("P_InitPicAnims: \"ANIMATED\" lump found. Reading animations...\n");

        animdefs = (animdef_t *)W_CacheLumpNum(lump, PU_STATIC);

        // Read structures until -1 is found.
        for(i = 0; animdefs[i].istexture != -1 ; ++i)
        {
            materialtype_t type =
                (animdefs[i].istexture? MAT_TEXTURE : MAT_FLAT);

            if(R_CheckMaterialNumForName(animdefs[i].startname, type) == -1)
                continue;

            endFrame = R_MaterialNumForName(animdefs[i].endname, type);
            startFrame = R_MaterialNumForName(animdefs[i].startname, type);

            numFrames = endFrame - startFrame + 1;
            ticsPerFrame = LONG(animdefs[i].speed);

            if(numFrames < 2)
                Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                         animdefs[i].startname, animdefs[i].endname);

            if(startFrame != -1 && endFrame != -1)
            {
                // We have a valid animation.
                // Create a new animation group for it.
                groupNum = R_CreateAnimGroup(type, AGF_SMOOTH);

                // Doomsday's group animation needs to know the texture/flat
                // numbers of ALL frames in the animation group so we'll
                // have to step through the directory adding frames as we go.
                // (DOOM only required the start/end texture/flat numbers and
                // would animate all textures/flats inbetween).

                VERBOSE(Con_Message("P_InitPicAnims: ADD (\"%s\" > \"%s\" %d)\n",
                                    animdefs[i].startname, animdefs[i].endname,
                                    ticsPerFrame));

                // Add all frames from start to end to the group.
                if(endFrame > startFrame)
                {
                    for(j = startFrame; j <= endFrame; j++)
                    {
                        name = (type == MAT_TEXTURE? R_MaterialNameForNum(j, MAT_TEXTURE) :
                                 W_LumpName(j));
                        R_AddToAnimGroup(groupNum, name, ticsPerFrame, 0);
                    }
                }
                else
                {
                    for(j = endFrame; j >= startFrame; j--)
                    {
                        name = (type == MAT_TEXTURE? R_MaterialNameForNum(j, MAT_TEXTURE) :
                                 W_LumpName(j));
                        R_AddToAnimGroup(groupNum, name, ticsPerFrame, 0);
                    }
                }
            }
        }
        Z_Free(animdefs);
        VERBOSE(Con_Message("P_InitPicAnims: Done.\n"));
    }
}

boolean P_ActivateLine(linedef_t *ld, mobj_t *mo, int side, int actType)
{
    switch(actType)
    {
    case SPAC_CROSS:
        P_CrossSpecialLine(ld, side, mo);
        return true;

    case SPAC_USE:
        return P_UseSpecialLine(mo, ld, side);

    case SPAC_IMPACT:
        P_ShootSpecialLine(mo, ld);
        return true;

    default:
        Con_Error("P_ActivateLine: Unknown Activation Type %i", actType);
        break;
    }

    return false;
}

/**
 * Called every time a thing origin is about to cross a line with
 * a non 0 special.
 */
void P_CrossSpecialLine(linedef_t *line, int side, mobj_t *thing)
{
    int                 ok;
    xline_t            *xline;

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing))
        return;

    xline = P_ToXLine(line);

    //  Triggers that other things can activate
    if(!thing->player)
    {
        // Things that should NOT trigger specials...
        switch(thing->type)
        {
        case MT_ROCKET:
        case MT_PLASMA:
        case MT_BFG:
        case MT_TROOPSHOT:
        case MT_HEADSHOT:
        case MT_BRUISERSHOT:
        case MT_BRUISERSHOTRED: // d64tc
        case MT_NTROSHOT: // d64tc
            return;
            break;

        default:
            break;
        }

        ok = 0;
        switch(xline->special)
        {
        case 39:                // TELEPORT TRIGGER
        case 97:                // TELEPORT RETRIGGER
        case 993: // d64tc
        case 125:               // TELEPORT MONSTERONLY TRIGGER
        case 126:               // TELEPORT MONSTERONLY RETRIGGER
        case 4:             // RAISE DOOR
        case 10:                // PLAT DOWN-WAIT-UP-STAY TRIGGER
        case 88:                // PLAT DOWN-WAIT-UP-STAY RETRIGGER
        case 415: // d64tc
            ok = 1;
            break;
        }

        // Anything can trigger this line!
        if(xline->flags & ML_ALLTRIGGER)
            ok = 1;

        if(!ok)
            return;
    }

    // Note: could use some const's here.
    switch(xline->special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
    case 2:
        // Open Door
        EV_DoDoor(line, open);
        xline->special = 0;
        break;

    case 3:
        // Close Door
        EV_DoDoor(line, close);
        xline->special = 0;
        break;

    case 4:
        // Raise Door
        EV_DoDoor(line, normal);
        xline->special = 0;
        break;

    case 5:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        xline->special = 0;
        break;

    case 6:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        xline->special = 0;
        break;

    case 8:
        // Build Stairs
        EV_BuildStairs(line, build8);
        xline->special = 0;
        break;

    case 10:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        xline->special = 0;
        break;

    case 12:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        xline->special = 0;
        break;

    case 13:
        // Light Turn On - max
        EV_LightTurnOn(line, 1);
        xline->special = 0;
        break;

    case 16:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        xline->special = 0;
        break;

    case 17:
        // Start Light Strobing
        EV_StartLightStrobing(line);
        xline->special = 0;
        break;

    case 19:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        xline->special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        xline->special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        xline->special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height
        //  on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        xline->special = 0;
        break;

    case 35:
        // Lights Very Dark
        EV_LightTurnOn(line, 35.0f/255.0f);
        xline->special = 0;
        break;

    case 36:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        xline->special = 0;
        break;

    case 37:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        xline->special = 0;
        break;

    case 38:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        xline->special = 0;
        break;

    case 420: // d64tc
        EV_DoSplitDoor(line, lowerToEight, raiseToHighest);
        xline->special = 0;
        break;

    case 430: // d64tc
        EV_DoFloor(line, customFloor);
        break;

    case 431: // d64tc
        EV_DoFloor(line, customFloor);
        xline->special = 0;
        break;

    case 426: // d64tc
        EV_DoCeiling(line, customCeiling);
        break;

    case 427: // d64tc
        EV_DoCeiling(line, customCeiling);
        xline->special = 0;
        break;

    case 991: // d64tc
        // TELEPORT!
        EV_FadeSpawn(line, thing);
        xline->special = 0;
        break;

    case 993: // d64tc
        if(!thing->player)
            EV_FadeSpawn(line, thing);
        xline->special = 0;
        break;

    case 992: // d64tc
        // Lower Ceiling to Floor
        if(EV_DoCeiling(line, lowerToFloor))
            P_ChangeSwitchTexture(line, 0);
        break;

    case 994: // d64tc
        // FIXME: DJS - Might as well do this in XG.
        // Also, export this text string to DED.
        P_SetMessage(thing->player, "You've found a secret area!", false);
        thing->player->secretCount++;
        xline->special = 0;
        break;

    case 995: // d64tc
        // FIXME: DJS - Might as well do this in XG.
        P_SetMessage(thing->player, "You've found a shrine!", false);
        xline->special = 0;
        break;

    case 998: // d64tc
        // BE GONE!
        EV_FadeAway(line, thing);
        xline->special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        xline->special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor
        EV_DoCeiling(line, raiseToHighest);
        EV_DoFloor(line, lowerFloorToLowest);
        xline->special = 0;
        break;

    case 44:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        xline->special = 0;
        break;

    case 52:
        // EXIT!
        G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
        break;

    case 53:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        xline->special = 0;
        break;

    case 54:
        // Platform Stop
        EV_StopPlat(line);
        xline->special = 0;
        break;

    case 56:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        xline->special = 0;
        break;

    case 57:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        xline->special = 0;
        break;

    case 58:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        xline->special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        xline->special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag)
        EV_TurnTagLightsOff(line);
        xline->special = 0;
        break;

    case 108:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        xline->special = 0;
        break;

    case 109:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        xline->special = 0;
        break;

    case 100:
        // Build Stairs Turbo 16
        EV_BuildStairs(line, turbo16);
        xline->special = 0;
        break;

    case 110:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        xline->special = 0;
        break;

    case 119:
        // Raise floor to nearest surr. floor
        EV_DoFloor(line, raiseFloorToNearest);
        xline->special = 0;
        break;

    case 121:
        // Blazing PlatDownWaitUpStay
        EV_DoPlat(line, blazeDWUS, 0);
        xline->special = 0;
        break;

    case 124:
        // Secret EXIT
        G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, true);
        break;

    case 125:
        // TELEPORT MonsterONLY
        if(!thing->player)
        {
            EV_Teleport(line, side, thing, true);
            xline->special = 0;
        }
        break;

    case 130:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        xline->special = 0;
        break;

    case 141:
        // Silent Ceiling Crush & Raise
        EV_DoCeiling(line, silentCrushAndRaise);
        xline->special = 0;
        break;

        // RETRIGGERS.  All from here till end.
    case 72:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        break;

    case 73:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        break;

    case 74:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        break;

    case 75:
        // Close Door
        EV_DoDoor(line, close);
        break;

    case 76:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        break;

    case 77:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        break;

    case 79:
        // Lights Very Dark
        EV_LightTurnOn(line, 35.0f/255.0f);
        break;

    case 80:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        break;

    case 81:
        // Light Turn On - max
        EV_LightTurnOn(line, 1);
        break;

    case 82:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        break;

    case 83:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        break;

    case 84:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        break;

    case 86:
        // Open Door
        EV_DoDoor(line, open);
        break;

    case 87:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        break;

    case 88:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        break;

    case 415: // d64tc
        if(thing->player)
            EV_DoPlat(line, upWaitDownStay, 0);
        break;

    case 422: // d64tc
        EV_ActivateSpecial(line);
        break;

//  case 430: // d64tc
    //  EV_DestoryLineShield(line);
    //  break;

    case 89:
        // Platform Stop
        EV_StopPlat(line);
        break;

    case 90:
        // Raise Door
        EV_DoDoor(line, normal);
        break;

    case 91:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        break;

    case 92:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        break;

    case 93:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        break;

    case 94:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        break;

    case 95:
        // Raise floor to nearest height
        // and change texture.
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        break;

    case 96:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        break;

    case 97:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        break;

    case 423: // d64tc
        // TELEPORT! (no fog)
        // FIXME: DJS - might as well do this in XG.
        EV_Teleport(line, side, thing, false);
        break;

    case 98:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        break;

    case 105:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        break;

    case 106:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        break;

    case 107:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        break;

    case 120:
        // Blazing PlatDownWaitUpStay.
        EV_DoPlat(line, blazeDWUS, 0);
        break;

    case 126:
        // TELEPORT MonsterONLY.
        if(!thing->player)
            EV_Teleport(line, side, thing, true);
        break;

    case 128:
        // Raise To Nearest Floor
        EV_DoFloor(line, raiseFloorToNearest);
        break;

    case 129:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        break;

    case 155: // d64tc
        // Raise Floor 512
        // FIXME: DJS - again, might as well do this in XG.
        if(EV_DoFloor(line, raiseFloor32))
            P_ChangeSwitchTexture(line, 0);
        break;
    }
}

/**
 * Called when a thing shoots a special line.
 */
void P_ShootSpecialLine(mobj_t *thing, linedef_t *line)
{
    int                 ok;

    //  Impacts that other things can activate.
    if(!thing->player)
    {
        ok = 0;
        switch (P_ToXLine(line)->special)
        {
        case 46:
            // OPEN DOOR IMPACT
            ok = 1;
            break;
        }
        if(!ok)
            return;
    }

    switch(P_ToXLine(line)->special)
    {
    case 24:
        // RAISE FLOOR
        EV_DoFloor(line, raiseFloor);
        P_ChangeSwitchTexture(line, 0);
        break;

    case 46:
        // OPEN DOOR
        EV_DoDoor(line, open);
        P_ChangeSwitchTexture(line, 1);
        break;

    case 47:
        // RAISE FLOOR NEAR AND CHANGE
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        P_ChangeSwitchTexture(line, 0);
        break;

    case 191: // d64tc
        // LOWER FLOOR WAIT RAISE
        EV_DoPlat(line, blazeDWUS, 0);
        P_ChangeSwitchTexture(line, 1);
        break;
    }
}

/**
 * Called every tic frame that the player origin is in a special sector
 */
void P_PlayerInSpecialSector(player_t *player)
{
    sector_t           *sector =
        P_GetPtrp(player->plr->mo->subsector, DMU_SECTOR);

    // Falling, not all the way down yet?
    if(player->plr->mo->pos[VZ] != P_GetFixedp(sector, DMU_FLOOR_HEIGHT))
        return;

    // Has hitten ground.
    switch(P_ToXSector(sector)->special)
    {
    case 5:
        // HELLSLIME DAMAGE
        if(!player->powers[PT_IRONFEET])
            if(!(levelTime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 10);
        break;

    case 7:
        // NUKAGE DAMAGE
        if(!player->powers[PT_IRONFEET])
            if(!(levelTime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 5);
        break;

    case 16:
        // SUPER HELLSLIME DAMAGE
    case 4:
        // STROBE HURT
        if(!player->powers[PT_IRONFEET] || (P_Random() < 5))
        {
            if(!(levelTime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 20);
        }
        break;

    case 9:
        // SECRET SECTOR
        player->secretCount++;
        P_ToXSector(sector)->special = 0;
        if(cfg.secretMsg)
        {
            P_SetMessage(player, "You've found a secret area!", false);
            // S_ConsoleSound(sfx_getpow, 0, player - players); // d64tc
        }
        break;
/*
    case 11:
        // EXIT SUPER DAMAGE! (for E1M8 finale)
        player->cheats &= ~CF_GODMODE;

        if(!(levelTime & 0x1f))
            P_DamageMobj(player->plr->mo, NULL, NULL, 20);

        if(player->health <= 10)
            G_LeaveLevel(G_GetLevelNumber(gameEpisode, gameMap), 0, false);
        break;
*/
    case 11: // d64tc
        // SUPER CHEAT REMOVER!!        //kaiser 9/8/03
        player->cheats &= ~CF_GODMODE;
        player->cheats &= ~CF_NOCLIP;
        player->laserPower = 0;
        break;

    default:
        break;
    }
}

/**
 * Animate planes, scroll walls, etc.
 */
void P_UpdateSpecials(void)
{
    float               x, y; // d64tc added @c y,
    linedef_t          *line;
    sidedef_t          *side;
    button_t           *button;

    // Extended lines and sectors.
    XG_Ticker();

    // Animate line specials.
    if(P_IterListSize(linespecials))
    {
        P_IterListResetIterator(linespecials, false);
        while((line = P_IterListIterator(linespecials)) != NULL)
        {
            switch(P_ToXLine(line)->special)
            {
            case 48:
                side = P_GetPtrp(line, DMU_SIDEDEF0);

                // EFFECT FIRSTCOL SCROLL +
                x = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X, x += 1);
                x = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X, x += 1);
                x = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X, x += 1);
                break;

            case 150: // d64tc
                side = P_GetPtrp(line, DMU_SIDEDEF0);

                // EFFECT FIRSTCOL SCROLL -
                x = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X, x -= 1);
                x = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X, x -= 1);
                x = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X, x -= 1);
                break;

            case 2561: // d64tc
                // Scroll_Texture_Up
                side = P_GetPtrp(line, DMU_SIDEDEF0);

                // EFFECT FIRSTCOL SCROLL +
                y = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y, y += 1);
                y = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y, y += 1);
                y = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y, y += 1);
                break;

            case 2562: // d64tc
                // Scroll_Texture_Down
                side = P_GetPtrp(line, DMU_SIDEDEF0);

                // EFFECT FIRSTCOL SCROLL +
                y = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y, y -= 1);
                y = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y, y -= 1);
                y = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y, y -= 1);
                break;

            case 2080: // d64tc
                // Scroll Up/Right
                side = P_GetPtrp(line, DMU_SIDEDEF0);

                y = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y, y += 1);
                y = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y, y += 1);
                y = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y, y += 1);

                x = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X, x -= 1);
                x = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X, x -= 1);
                x = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X, x -= 1);
                break;

            case 2614: // d64tc
                // Scroll Up/Left
                side = P_GetPtrp(line, DMU_SIDEDEF0);

                y = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_Y, y += 1);
                y = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y, y += 1);
                y = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y, y += 1);

                x = P_GetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_TOP_MATERIAL_OFFSET_X, x += 1);
                x = P_GetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_MIDDLE_MATERIAL_OFFSET_X, x += 1);
                x = P_GetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);
                P_SetFloatp(side, DMU_BOTTOM_MATERIAL_OFFSET_X, x += 1);
                break;

            default:
                break;
            }
        }
    }

    // Handle buttons.
    for(button = buttonlist; button; button = button->next)
    {
        if(button->timer)
        {
            button->timer--;
            if(!button->timer)
            {
                sidedef_t      *sdef = P_GetPtrp(button->line, DMU_SIDEDEF0);
                sector_t       *frontsector = P_GetPtrp(button->line, DMU_FRONT_SECTOR);

                switch(button->section)
                {
                case LS_TOP:
                    P_SetIntp(sdef, DMU_TOP_MATERIAL, button->texture);
                    break;

                case LS_MIDDLE:
                    P_SetIntp(sdef, DMU_MIDDLE_MATERIAL, button->texture);
                    break;

                case LS_BOTTOM:
                    P_SetIntp(sdef, DMU_BOTTOM_MATERIAL, button->texture);
                    break;

                default:
                    Con_Error("P_UpdateSpecials: Unknown sidedef section \"%d\".",
                              (int) button->section);
                }

                S_StartSound(sfx_swtchn,
                             P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));

                button->line = NULL;
                button->section = 0;
                button->texture = 0;
                button->soundOrg = NULL;
            }
        }
    }
}

void P_FreeButtons(void)
{
    button_t           *button, *np;

    button = buttonlist;
    while(button != NULL)
    {
        np = button->next;
        free(button);
        button = np;
    }
    buttonlist = NULL;
}

/**
 * d64tc
 */
void P_ThunderSector(void)
{
    sector_t           *sec = NULL;
    iterlist_t         *list;

    if(!(P_Random() < 10))
        return;

    list = P_GetSectorIterListForTag(20000, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(!(levelTime & 32))
        {
            P_SetFloatp(sec, DMU_LIGHT_LEVEL, 1);
            S_StartSound(sfx_sssit, P_GetPtrp(sec, DMU_SOUND_ORIGIN));
        }
    }
}

/**
 * After the map has been loaded, scan for specials that spawn thinkers.
 */
void P_SpawnSpecials(void)
{
    uint                i;
    linedef_t          *line;
    xline_t            *xline;
    iterlist_t         *list;
    sector_t           *sec;
    xsector_t          *xsec;

    // Init special SECTORs.
    P_DestroySectorTagLists();
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_ToXSector(sec);

        if(xsec->tag)
        {
           list = P_GetSectorIterListForTag(xsec->tag, true);
           P_AddObjectToIterList(list, sec);
        }

        if(!xsec->special)
            continue;

        if(IS_CLIENT)
        {
            switch(xsec->special)
            {
            case 9:
                // SECRET SECTOR
                totalSecret++;
                break;
            }
            continue;
        }

        // d64tc >
        // DJS - yet more hacks! Why use the tag?
        switch(xsec->tag)
        {
        case 10000:
        case 10001:
        case 10002:
        case 10003:
        case 10004:
            P_SpawnGlowingLight(sec);
            break;

        case 11000:
            P_SpawnLightFlash(sec);
            break;

        case 12000:
            P_SpawnFireFlicker(sec);
            break;

        case 13000:
            P_SpawnLightBlink(sec);
            break;

        case 20000:
            P_SpawnGlowingLight(sec);
            break;
        }
        // < d64tc

        switch(xsec->special)
        {
        case 1:
            // FLICKERING LIGHTS
            P_SpawnLightFlash(sec);
            break;

        case 2:
            // STROBE FAST
            P_SpawnStrobeFlash(sec, FASTDARK, 0);
            break;

        case 3:
            // STROBE SLOW
            P_SpawnStrobeFlash(sec, SLOWDARK, 0);
            break;

        case 4:
            // STROBE FAST/DEATH SLIME
            P_SpawnStrobeFlash(sec, FASTDARK, 0);
            xsec->special = 4;
            break;

        case 8:
            // GLOWING LIGHT
            P_SpawnGlowingLight(sec);
            break;

        case 9:
            // SECRET SECTOR
            totalSecret++;
            break;

        case 10:
            // DOOR CLOSE IN 30 SECONDS
            P_SpawnDoorCloseIn30(sec);
            break;

        case 12:
            // SYNC STROBE SLOW
            P_SpawnStrobeFlash(sec, SLOWDARK, 1);
            break;

        case 13:
            // SYNC STROBE FAST
            P_SpawnStrobeFlash(sec, FASTDARK, 1);
            break;

        case 14:
            // DOOR RAISE IN 5 MINUTES
            P_SpawnDoorRaiseIn5Mins(sec);
            break;

        case 17:
            P_SpawnFireFlicker(sec);
            break;
        }
    }

    // Init animating line specials.
    P_EmptyIterList(linespecials);
    P_DestroyLineTagLists();
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINEDEF, i);
        xline = P_ToXLine(line);

        switch(xline->special)
        {
        case 48: // EFFECT FIRSTCOL SCROLL+
        case 150: // d64tc: wall scroll right
        case 2561: // d64tc: wall scroll up
        case 2562: // d64tc: wall scroll down
        case 2080: // d64tc: wall scroll up/right
        case 2614: // d64tc: wall scroll up/left
            P_AddObjectToIterList(linespecials, line);
            break;

        case 994: // d64tc
            // kaiser - be sure to have that linedef count the secrets.
            // FIXME: DJS - secret lines??
            totalSecret++;
            break;
        }

        if(xline->tag)
        {
           list = P_GetLineIterListForTag(xline->tag, true);
           P_AddObjectToIterList(list, line);
        }
    }

    P_RemoveAllActiveCeilings();
    P_RemoveAllActivePlats();

    P_FreeButtons();

    // Init extended generalized lines and sectors.
    XG_Init();
}
