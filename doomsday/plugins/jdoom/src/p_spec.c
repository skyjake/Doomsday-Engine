/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * Implements special effects:
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing,
 * or shooting special lines, or by timed thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "m_argv.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_player.h"
#include "p_mapspec.h"

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

extern boolean P_UseSpecialLine(mobj_t *thing, line_t *line, int side);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_CrossSpecialLine(line_t *line, int side, mobj_t *thing);
static void P_ShootSpecialLine(mobj_t *thing, line_t *line);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// TODO: From jHeretic, replace!
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

/*
 * TODO: This routine originated from jHeretic, we need to rewrite it!
 */
void P_InitTerrainTypes(void)
{
    int     i;
    int     lump;
    int     size;

    size = Get(DD_NUMLUMPS) * sizeof(int);
    TerrainTypes = Z_Malloc(size, PU_STATIC, 0);
    memset(TerrainTypes, 0, size);
    for(i = 0; TerrainTypeDefs[i].type != -1; i++)
    {
        lump = W_CheckNumForName(TerrainTypeDefs[i].name);
        if(lump != -1)
        {
            TerrainTypes[lump] = TerrainTypeDefs[i].type;
        }
    }
}

/*
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

/*
 * Returns the terrain type of the specified sector, plane.
 *
 * @param sec       The sector to check.
 * @param plane     The plane id to check.
 */
int P_GetTerrainType(sector_t* sec, int plane)
{
    int flatnum = P_GetIntp(sec, (plane? DMU_CEILING_TEXTURE : DMU_FLOOR_TEXTURE));

    if(flatnum != -1)
        return TerrainTypes[flatnum];
    else
        return FLOOR_SOLID;
}

/* From PrBoom:
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

/*
 * DJS - We'll support this BOOM extension by reading the data and then
 *       registering the new animations into Doomsday using the animation
 *       groups feature.
 *
 *       Support for this extension should be considered depreciated.
 *       All new features should be added, accessed via DED.
 */
void P_InitPicAnims(void)
{
    int i, j;
    int groupNum;
    int isTexture, startFrame, endFrame, ticsPerFrame;
    int numFrames;
    int lump = W_CheckNumForName("ANIMATED");
    animdef_t *animdefs;

    // Has a custom ANIMATED lump been loaded?
    if(lump > 0)
    {
        Con_Message("P_InitPicAnims: \"ANIMATED\" lump found. Reading animations...\n");

        animdefs = (animdef_t *)W_CacheLumpNum(lump, PU_STATIC);

        // Read structures until -1 is found
        for(i = 0; animdefs[i].istexture != -1 ; ++i)
        {
            // Is it a texture?
            if(animdefs[i].istexture)
            {
                // different episode ?
                if(R_CheckTextureNumForName(animdefs[i].startname) == -1)
                    continue;

                endFrame = R_TextureNumForName(animdefs[i].endname);
                startFrame = R_TextureNumForName(animdefs[i].startname);
            }
            else // Its a flat.
            {
                if((W_CheckNumForName(animdefs[i].startname)) == -1)
                    continue;

                endFrame = R_FlatNumForName(animdefs[i].endname);
                startFrame = R_FlatNumForName(animdefs[i].startname);
            }

            isTexture = animdefs[i].istexture;
            numFrames = endFrame - startFrame + 1;

            ticsPerFrame = LONG(animdefs[i].speed);

            if(numFrames < 2)
                Con_Error("P_InitPicAnims: bad cycle from %s to %s",
                         animdefs[i].startname, animdefs[i].endname);

            if(startFrame != -1 && endFrame != -1)
            {
                // We have a valid animation.
                // Create a new animation group for it.
                groupNum =
                    R_CreateAnimGroup(isTexture ? DD_TEXTURE : DD_FLAT, AGF_SMOOTH);

                // Doomsday's group animation needs to know the texture/flat
                // numbers of ALL frames in the animation group so we'll have
                // to step through the directory adding frames as we go.
                // (DOOM only required the start/end texture/flat numbers and
                // would animate all textures/flats inbetween).

                VERBOSE(Con_Message("P_InitPicAnims: ADD (\"%s\" > \"%s\" %d)\n",
                                    animdefs[i].startname, animdefs[i].endname,
                                    ticsPerFrame));

                // Add all frames from start to end to the group.
                if(endFrame > startFrame)
                {
                    for(j = startFrame; j <= endFrame; j++)
                        R_AddToAnimGroup(groupNum, j, ticsPerFrame, 0);
                }
                else
                {
                    for(j = endFrame; j >= startFrame; j--)
                        R_AddToAnimGroup(groupNum, j, ticsPerFrame, 0);
                }
            }
        }
        Z_Free(animdefs);
        VERBOSE(Con_Message("P_InitPicAnims: Done.\n"));
    }
}

boolean P_ActivateLine(line_t *ld, mobj_t *mo, int side, int actType)
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

/*
 * Called every time a thing origin is about to cross a line with
 * a non 0 special.
 */
void P_CrossSpecialLine(line_t *line, int side, mobj_t *thing)
{
    int     ok;

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing))
        return;

    //  Triggers that other things can activate
    if(!thing->player)
    {
        // Things that should NOT trigger specials...
        switch (thing->type)
        {
        case MT_ROCKET:
        case MT_PLASMA:
        case MT_BFG:
        case MT_TROOPSHOT:
        case MT_HEADSHOT:
        case MT_BRUISERSHOT:
            return;
            break;

        default:
            break;
        }

        ok = 0;
        switch (P_XLine(line)->special)
        {
        case 39:                // TELEPORT TRIGGER
        case 97:                // TELEPORT RETRIGGER
        case 125:               // TELEPORT MONSTERONLY TRIGGER
        case 126:               // TELEPORT MONSTERONLY RETRIGGER
        case 4:             // RAISE DOOR
        case 10:                // PLAT DOWN-WAIT-UP-STAY TRIGGER
        case 88:                // PLAT DOWN-WAIT-UP-STAY RETRIGGER
            ok = 1;
            break;
        }

        // Anything can trigger this line!
        if(P_GetIntp(line, DMU_FLAGS) & ML_ALLTRIGGER)
            ok = 1;

        if(!ok)
            return;
    }

    // Note: could use some const's here.
    switch (P_XLine(line)->special)
    {
        // TRIGGERS.
        // All from here to RETRIGGERS.
    case 2:
        // Open Door
        EV_DoDoor(line, open);
        P_XLine(line)->special = 0;
        break;

    case 3:
        // Close Door
        EV_DoDoor(line, close);
        P_XLine(line)->special = 0;
        break;

    case 4:
        // Raise Door
        EV_DoDoor(line, normal);
        P_XLine(line)->special = 0;
        break;

    case 5:
        // Raise Floor
        EV_DoFloor(line, raiseFloor);
        P_XLine(line)->special = 0;
        break;

    case 6:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, fastCrushAndRaise);
        P_XLine(line)->special = 0;
        break;

    case 8:
        // Build Stairs
        EV_BuildStairs(line, build8);
        P_XLine(line)->special = 0;
        break;

    case 10:
        // PlatDownWaitUp
        EV_DoPlat(line, downWaitUpStay, 0);
        P_XLine(line)->special = 0;
        break;

    case 12:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        P_XLine(line)->special = 0;
        break;

    case 13:
        // Light Turn On 255
        EV_LightTurnOn(line, 255);
        P_XLine(line)->special = 0;
        break;

    case 16:
        // Close Door 30
        EV_DoDoor(line, close30ThenOpen);
        P_XLine(line)->special = 0;
        break;

    case 17:
        // Start Light Strobing
        EV_StartLightStrobing(line);
        P_XLine(line)->special = 0;
        break;

    case 19:
        // Lower Floor
        EV_DoFloor(line, lowerFloor);
        P_XLine(line)->special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture
        EV_DoPlat(line, raiseToNearestAndChange, 0);
        P_XLine(line)->special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, crushAndRaise);
        P_XLine(line)->special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height
        //  on either side of lines.
        EV_DoFloor(line, raiseToTexture);
        P_XLine(line)->special = 0;
        break;

    case 35:
        // Lights Very Dark
        EV_LightTurnOn(line, 35);
        P_XLine(line)->special = 0;
        break;

    case 36:
        // Lower Floor (TURBO)
        EV_DoFloor(line, turboLower);
        P_XLine(line)->special = 0;
        break;

    case 37:
        // LowerAndChange
        EV_DoFloor(line, lowerAndChange);
        P_XLine(line)->special = 0;
        break;

    case 38:
        // Lower Floor To Lowest
        EV_DoFloor(line, lowerFloorToLowest);
        P_XLine(line)->special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing);
        P_XLine(line)->special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor
        EV_DoCeiling(line, raiseToHighest);
        EV_DoFloor(line, lowerFloorToLowest);
        P_XLine(line)->special = 0;
        break;

    case 44:
        // Ceiling Crush
        EV_DoCeiling(line, lowerAndCrush);
        P_XLine(line)->special = 0;
        break;

    case 52:
        // EXIT!
        G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
        break;

    case 53:
        // Perpetual Platform Raise
        EV_DoPlat(line, perpetualRaise, 0);
        P_XLine(line)->special = 0;
        break;

    case 54:
        // Platform Stop
        EV_StopPlat(line);
        P_XLine(line)->special = 0;
        break;

    case 56:
        // Raise Floor Crush
        EV_DoFloor(line, raiseFloorCrush);
        P_XLine(line)->special = 0;
        break;

    case 57:
        // Ceiling Crush Stop
        EV_CeilingCrushStop(line);
        P_XLine(line)->special = 0;
        break;

    case 58:
        // Raise Floor 24
        EV_DoFloor(line, raiseFloor24);
        P_XLine(line)->special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change
        EV_DoFloor(line, raiseFloor24AndChange);
        P_XLine(line)->special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag)
        EV_TurnTagLightsOff(line);
        P_XLine(line)->special = 0;
        break;

    case 108:
        // Blazing Door Raise (faster than TURBO!)
        EV_DoDoor(line, blazeRaise);
        P_XLine(line)->special = 0;
        break;

    case 109:
        // Blazing Door Open (faster than TURBO!)
        EV_DoDoor(line, blazeOpen);
        P_XLine(line)->special = 0;
        break;

    case 100:
        // Build Stairs Turbo 16
        EV_BuildStairs(line, turbo16);
        P_XLine(line)->special = 0;
        break;

    case 110:
        // Blazing Door Close (faster than TURBO!)
        EV_DoDoor(line, blazeClose);
        P_XLine(line)->special = 0;
        break;

    case 119:
        // Raise floor to nearest surr. floor
        EV_DoFloor(line, raiseFloorToNearest);
        P_XLine(line)->special = 0;
        break;

    case 121:
        // Blazing PlatDownWaitUpStay
        EV_DoPlat(line, blazeDWUS, 0);
        P_XLine(line)->special = 0;
        break;

    case 124:
        // Secret EXIT
        G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, true);
        break;

    case 125:
        // TELEPORT MonsterONLY
        if(!thing->player)
        {
            EV_Teleport(line, side, thing);
            P_XLine(line)->special = 0;
        }
        break;

    case 130:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        P_XLine(line)->special = 0;
        break;

    case 141:
        // Silent Ceiling Crush & Raise
        EV_DoCeiling(line, silentCrushAndRaise);
        P_XLine(line)->special = 0;
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
        EV_LightTurnOn(line, 35);
        break;

    case 80:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        break;

    case 81:
        // Light Turn On 255
        EV_LightTurnOn(line, 255);
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
        EV_Teleport(line, side, thing);
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
            EV_Teleport(line, side, thing);
        break;

    case 128:
        // Raise To Nearest Floor
        EV_DoFloor(line, raiseFloorToNearest);
        break;

    case 129:
        // Raise Floor Turbo
        EV_DoFloor(line, raiseFloorTurbo);
        break;
    }
}

/*
 * Called when a thing shoots a special line.
 */
void P_ShootSpecialLine(mobj_t *thing, line_t *line)
{
    int     ok;

    //  Impacts that other things can activate.
    if(!thing->player)
    {
        ok = 0;
        switch (P_XLine(line)->special)
        {
        case 46:
            // OPEN DOOR IMPACT
            ok = 1;
            break;
        }
        if(!ok)
            return;
    }

    switch(P_XLine(line)->special)
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
    }
}

/*
 * Called every tic frame that the player origin is in a special sector
 */
void P_PlayerInSpecialSector(player_t *player)
{
    sector_t *sector =
        P_GetPtrp(player->plr->mo->subsector, DMU_SECTOR);

    // Falling, not all the way down yet?
    if(player->plr->mo->pos[VZ] != P_GetFixedp(sector, DMU_FLOOR_HEIGHT))
        return;

    // Has hitten ground.
    switch(P_XSector(sector)->special)
    {
    case 5:
        // HELLSLIME DAMAGE
        if(!player->powers[pw_ironfeet])
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 10);
        break;

    case 7:
        // NUKAGE DAMAGE
        if(!player->powers[pw_ironfeet])
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 5);
        break;

    case 16:
        // SUPER HELLSLIME DAMAGE
    case 4:
        // STROBE HURT
        if(!player->powers[pw_ironfeet] || (P_Random() < 5))
        {
            if(!(leveltime & 0x1f))
                P_DamageMobj(player->plr->mo, NULL, NULL, 20);
        }
        break;

    case 9:
        // SECRET SECTOR
        player->secretcount++;
        P_XSector(sector)->special = 0;
        if(cfg.secretMsg)
        {
            P_SetMessage(player, "You've found a secret area!", false);
            S_ConsoleSound(sfx_getpow, 0, player - players);
        }
        break;

    case 11:
        // EXIT SUPER DAMAGE! (for E1M8 finale)
        player->cheats &= ~CF_GODMODE;

        if(!(leveltime & 0x1f))
            P_DamageMobj(player->plr->mo, NULL, NULL, 20);

        if(player->health <= 10)
            G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
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
    int     i;
    int     x;
    line_t *line;
    side_t *side;

    // Extended lines and sectors.
    XG_Ticker();

    //  ANIMATE LINE SPECIALS
    if(P_IterListSize(linespecials))
    {
        P_IterListResetIterator(linespecials);
        while((line = P_IterListIterator(linespecials)) != NULL)
        {
            switch(P_XLine(line)->special)
            {
            case 48:
                side = P_GetPtrp(line, DMU_SIDE0);

                // EFFECT FIRSTCOL SCROLL +
                x = P_GetFixedp(side, DMU_TOP_TEXTURE_OFFSET_X);
                P_SetFixedp(side, DMU_TOP_TEXTURE_OFFSET_X, x += FRACUNIT);
                x = P_GetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_X);
                P_SetFixedp(side, DMU_MIDDLE_TEXTURE_OFFSET_X, x += FRACUNIT);
                x = P_GetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_X);
                P_SetFixedp(side, DMU_BOTTOM_TEXTURE_OFFSET_X, x += FRACUNIT);
                break;

            default:
                break;
            }
        }
    }

    //  DO BUTTONS
    // FIXME! remove fixed limt.
    for(i = 0; i < MAXBUTTONS; i++)
    {
        if(buttonlist[i].btimer)
        {
            buttonlist[i].btimer--;
            if(!buttonlist[i].btimer)
            {
                side_t* sdef = P_GetPtrp(buttonlist[i].line,
                                         DMU_SIDE0);
                sector_t* frontsector = P_GetPtrp(buttonlist[i].line,
                                                  DMU_FRONT_SECTOR);

                switch (buttonlist[i].where)
                {
                case top:
                    P_SetIntp(sdef, DMU_TOP_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                case middle:
                    P_SetIntp(sdef, DMU_MIDDLE_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                case bottom:
                    P_SetIntp(sdef, DMU_BOTTOM_TEXTURE,
                              buttonlist[i].btexture);
                    break;

                default:
                    Con_Error("P_UpdateSpecials: Unknown sidedef section \"%d\".",
                              buttonlist[i].where);
                }

                S_StartSound(sfx_swtchn,
                             P_GetPtrp(frontsector, DMU_SOUND_ORIGIN));

                memset(&buttonlist[i], 0, sizeof(button_t));
            }
        }
    }

}

/**
 * After the map has been loaded, scan for specials that spawn thinkers.
 */
void P_SpawnSpecials(void)
{
    int         i;
    line_t     *line;
    xline_t    *xline;
    iterlist_t *list;
    sector_t   *sector;

    // Init special SECTORs.
    for(i = 0; i < numsectors; ++i)
    {
        sector = P_ToPtr(DMU_SECTOR, i);

        if(!P_XSector(sector)->special)
            continue;

        if(IS_CLIENT)
        {
            switch (P_XSector(sector)->special)
            {
            case 9:
                // SECRET SECTOR
                totalsecret++;
                break;
            }
            continue;
        }

        switch (P_XSector(sector)->special)
        {
        case 1:
            // FLICKERING LIGHTS
            P_SpawnLightFlash(sector);
            break;

        case 2:
            // STROBE FAST
            P_SpawnStrobeFlash(sector, FASTDARK, 0);
            break;

        case 3:
            // STROBE SLOW
            P_SpawnStrobeFlash(sector, SLOWDARK, 0);
            break;

        case 4:
            // STROBE FAST/DEATH SLIME
            P_SpawnStrobeFlash(sector, FASTDARK, 0);
            P_XSector(sector)->special = 4;
            break;

        case 8:
            // GLOWING LIGHT
            P_SpawnGlowingLight(sector);
            break;

        case 9:
            // SECRET SECTOR
            totalsecret++;
            break;

        case 10:
            // DOOR CLOSE IN 30 SECONDS
            P_SpawnDoorCloseIn30(sector);
            break;

        case 12:
            // SYNC STROBE SLOW
            P_SpawnStrobeFlash(sector, SLOWDARK, 1);
            break;

        case 13:
            // SYNC STROBE FAST
            P_SpawnStrobeFlash(sector, FASTDARK, 1);
            break;

        case 14:
            // DOOR RAISE IN 5 MINUTES
            P_SpawnDoorRaiseIn5Mins(sector, i);
            break;

        case 17:
            P_SpawnFireFlicker(sector);
            break;
        }
    }

    // Init animating line specials.
    P_EmptyIterList(linespecials);
    for(i = 0; i < numlines; ++i)
    {
        line = P_ToPtr(DMU_LINE, i);
        xline = P_XLine(line);

        switch(xline->special)
        {
        case 48: // EFFECT FIRSTCOL SCROLL+
            P_AddObjectToIterList(linespecials, line);
            break;
        }

        if(xline->tag)
        {
           list = P_GetLineIterListForTag(xline->tag, true);
           P_AddObjectToIterList(list, line);
        }
    }

    P_RemoveAllActiveCeilings();  // jff 2/22/98 use killough's scheme
    P_RemoveAllActivePlats();     // killough

    // FIXME: Remove fixed limit
    for(i = 0; i < MAXBUTTONS; ++i)
        memset(&buttonlist[i], 0, sizeof(button_t));

    // Init extended generalized lines and sectors.
    XG_Init();
}
