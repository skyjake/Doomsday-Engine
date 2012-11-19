/**\file p_spec.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Implements special effects.
 *
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing, or shooting special
 * lines, or by timed thinkers.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jheretic.h"

#include "m_argv.h"
#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_player.h"
#include "p_tick.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "p_user.h"
#include "p_scroll.h"
#include "p_switch.h"
#include "d_netsv.h"

// MACROS ------------------------------------------------------------------

#define MAX_AMBIENT_SFX 8       // Per level

// TYPES -------------------------------------------------------------------

typedef enum afxcmd_e {
    afxcmd_play,                // (sound)
    afxcmd_playabsvol,          // (sound, volume)
    afxcmd_playrelvol,          // (sound, volume)
    afxcmd_delay,               // (ticks)
    afxcmd_delayrand,           // (andbits)
    afxcmd_end                  // ()
} afxcmd_t;

// Animating textures and planes

// In Doomsday these are handled via DED definitions.
// In BOOM they invented the ANIMATED lump for the same purpose.

// This struct is directly read from the lump.
// So its important we keep it aligned.
#pragma pack(1)
typedef struct animdef_s {
    /* Do NOT change these members in any way */
    signed char istexture;  // if false, it is a flat (instead of bool)
    char        endname[9];
    char        startname[9];
    int         speed;
} animdef_t;
#pragma pack()

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_CrossSpecialLine(LineDef* line, int side, mobj_t* thing);
static void P_ShootSpecialLine(mobj_t* thing, LineDef* line);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobj_t LavaInflictor;

int* LevelAmbientSfx[MAX_AMBIENT_SFX];
int* AmbSfxPtr;
int AmbSfxCount;
int AmbSfxTics;
int AmbSfxVolume;

int AmbSndSeqInit[] = { // Startup
    afxcmd_end
};

int AmbSndSeq1[] = { // Scream
    afxcmd_play, SFX_AMB1,
    afxcmd_end
};

int AmbSndSeq2[] = { // Squish
    afxcmd_play, SFX_AMB2,
    afxcmd_end
};

int AmbSndSeq3[] = { // Drops
    afxcmd_play, SFX_AMB3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB3,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_play, SFX_AMB7,
    afxcmd_delay, 16,
    afxcmd_delayrand, 31,
    afxcmd_end
};

int AmbSndSeq4[] = { // SlowFootSteps
    afxcmd_play, SFX_AMB4,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 15,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_end
};

int AmbSndSeq5[] = { // Heartbeat
    afxcmd_play, SFX_AMB5,
    afxcmd_delay, 35,
    afxcmd_play, SFX_AMB5,
    afxcmd_delay, 35,
    afxcmd_play, SFX_AMB5,
    afxcmd_delay, 35,
    afxcmd_play, SFX_AMB5,
    afxcmd_end
};

int AmbSndSeq6[] = { // Bells
    afxcmd_play, SFX_AMB6,
    afxcmd_delay, 17,
    afxcmd_playrelvol, SFX_AMB6, -8,
    afxcmd_delay, 17,
    afxcmd_playrelvol, SFX_AMB6, -8,
    afxcmd_delay, 17,
    afxcmd_playrelvol, SFX_AMB6, -8,
    afxcmd_end
};

int AmbSndSeq7[] = { // Growl
    afxcmd_play, SFX_BSTSIT,
    afxcmd_end
};

int AmbSndSeq8[] = { // Magic
    afxcmd_play, SFX_AMB8,
    afxcmd_end
};

int AmbSndSeq9[] = { // Laughter
    afxcmd_play, SFX_AMB9,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB9, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB9, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB10, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB10, -4,
    afxcmd_delay, 16,
    afxcmd_playrelvol, SFX_AMB10, -4,
    afxcmd_end
};

int AmbSndSeq10[] = { // FastFootsteps
    afxcmd_play, SFX_AMB4,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB4, -3,
    afxcmd_delay, 8,
    afxcmd_playrelvol, SFX_AMB11, -3,
    afxcmd_end
};

int* AmbientSfx[] = {
    AmbSndSeq1, // Scream
    AmbSndSeq2, // Squish
    AmbSndSeq3, // Drops
    AmbSndSeq4, // SlowFootsteps
    AmbSndSeq5, // Heartbeat
    AmbSndSeq6, // Bells
    AmbSndSeq7, // Growl
    AmbSndSeq8, // Magic
    AmbSndSeq9, // Laughter
    AmbSndSeq10 // FastFootsteps
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static animdef_t anims[] = {
    {0, "FLTWAWA3", "FLTWAWA1", MACRO_LONG(8)},
    {0, "FLTSLUD3", "FLTSLUD1", MACRO_LONG(8)},
    {0, "FLTTELE4", "FLTTELE1", MACRO_LONG(6)},
    {0, "FLTFLWW3", "FLTFLWW1", MACRO_LONG(9)},
    {0, "FLTLAVA4", "FLTLAVA1", MACRO_LONG(8)},
    {0, "FLATHUH4", "FLATHUH1", MACRO_LONG(8)},
    {1, "LAVAFL3",  "LAVAFL1",  MACRO_LONG(6)},
    {1, "WATRWAL3", "WATRWAL1", MACRO_LONG(4)},
    {-1, "\0",      "\0"}
};

// CODE --------------------------------------------------------------------

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
static void loadAnimDefs(animdef_t* animDefs, boolean isCustom)
{
    boolean lastIsTexture = false; // Shut up compiler!
    ddstring_t framePath, startPath, endPath;
    Uri* frameUrn = Uri_NewWithPath2("urn:", FC_NONE);
    Uri* startUri = Uri_New();
    Uri* endUri = Uri_New();
    int i;

    Str_Init(&framePath);
    Str_Init(&startPath);
    Str_Init(&endPath);

    // Read structures until -1 is found
    for(i = 0; animDefs[i].istexture != -1 ; ++i)
    {
        boolean isTexture = animDefs[i].istexture != 0;
        int groupNum, ticsPerFrame, numFrames;
        int startFrame, endFrame, n;

        if(i == 0 || isTexture != lastIsTexture)
        {
            Uri_SetScheme(startUri, isTexture? TN_TEXTURES_NAME : TN_FLATS_NAME);
            Uri_SetScheme(endUri, isTexture? TN_TEXTURES_NAME : TN_FLATS_NAME);
            lastIsTexture = isTexture;
        }
        Str_PercentEncode(Str_StripRight(Str_Set(&startPath, animDefs[i].startname)));
        Uri_SetPath(startUri, Str_Text(&startPath));

        Str_PercentEncode(Str_StripRight(Str_Set(&endPath, animDefs[i].endname)));
        Uri_SetPath(endUri, Str_Text(&endPath));

        startFrame = R_TextureUniqueId2(startUri, !isCustom);
        endFrame   = R_TextureUniqueId2(endUri, !isCustom);
        if(-1 == startFrame || -1 == endFrame) continue;

        numFrames = (endFrame > startFrame? endFrame - startFrame : startFrame - endFrame) + 1;
        if(numFrames < 2)
        {
            Con_Message("Warning:loadAnimDefs: Bad cycle from '%s' to '%s' in sequence #%i, ignoring.\n", animDefs[i].startname, animDefs[i].endname, i);
            continue;
        }

        /**
         * A valid animation.
         *
         * Doomsday's group animation needs to know the texture/flat
         * numbers of ALL frames in the animation group so we'll
         * have to step through the directory adding frames as we
         * go. (DOOM only required the start/end texture/flat
         * numbers and would animate all textures/flats inbetween).
         */
        ticsPerFrame = LONG(animDefs[i].speed);

        if(verbose > (isCustom? 1 : 2))
        {
            AutoStr* from = Uri_ToString(startUri);
            AutoStr* to = Uri_ToString(endUri);
            Con_Message("  %d: From:\"%s\" To:\"%s\" Tics:%i\n", i, Str_Text(from), Str_Text(to), ticsPerFrame);
        }

        // Find an animation group for this.
        groupNum = R_CreateAnimGroup(AGF_SMOOTH);

        // Add all frames to the group.
        for(n = startFrame; n <= endFrame; ++n)
        {
            Str_Clear(&framePath);
            Str_Appendf(&framePath, "%s:%i", isTexture? TN_TEXTURES_NAME : TN_FLATS_NAME, n);
            Uri_SetPath(frameUrn, Str_Text(&framePath));

            R_AddAnimGroupFrame(groupNum, frameUrn, ticsPerFrame, 0);
        }
    }

    Str_Free(&endPath);
    Str_Free(&startPath);
    Str_Free(&framePath);
    Uri_Delete(endUri);
    Uri_Delete(startUri);
    Uri_Delete(frameUrn);
}

void P_InitPicAnims(void)
{
    { lumpnum_t lumpNum;
    if((lumpNum = W_CheckLumpNumForName2("ANIMATED", true)) > 0)
    {
        /**
         * We'll support this BOOM extension by reading the data and then
         * registering the new animations into Doomsday using the animation
         * groups feature.
         *
         * Support for this extension should be considered depreciated.
         * All new features should be added, accessed via DED.
         */
        VERBOSE( Con_Message("Processing lump %s::ANIMATED...\n", F_PrettyPath(Str_Text(W_LumpSourceFile(lumpNum)))) )
        loadAnimDefs((animdef_t*)W_CacheLump(lumpNum), true);
        W_UnlockLump(lumpNum);
        return;
    }}

    VERBOSE( Con_Message("Registering default texture animations...\n") );
    loadAnimDefs(anims, false);
}

boolean P_ActivateLine(LineDef* ld, mobj_t* mo, int side, int actType)
{
    // Clients do not activate lines.
    if(IS_CLIENT) return false;

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
static void P_CrossSpecialLine(LineDef* line, int side, mobj_t* thing)
{
    xline_t* xline;

    // Extended functionality overrides old.
    if(XL_CrossLine(line, side, thing)) return;

    xline = P_ToXLine(line);

    // Triggers that other things can activate.
    if(!thing->player)
    {
        switch(xline->special)
        {
        default: return;

        case 39: ///< TELEPORT TRIGGER
        case 97: ///< TELEPORT RETRIGGER
        case 4:  ///< RAISE DOOR
            break;
        }
    }

    // Note: could use some const's here.
    switch(xline->special)
    {
    default: break;

        // TRIGGERS.
    // All from here to RETRIGGERS.
    case 2:
        // Open Door
        EV_DoDoor(line, DT_OPEN);
        xline->special = 0;
        break;

    case 3:
        // Close Door
        EV_DoDoor(line, DT_CLOSE);
        xline->special = 0;
        break;

    case 4:
        // Raise Door
        EV_DoDoor(line, DT_NORMAL);
        xline->special = 0;
        break;

    case 5:
        // Raise Floor
        EV_DoFloor(line, FT_RAISEFLOOR);
        xline->special = 0;
        break;

    case 6:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISEFAST);
        xline->special = 0;
        break;

    case 8:
        // Build Stairs
        EV_BuildStairs(line, build8);
        xline->special = 0;
        break;

    case 10:
        // PlatDownWaitUp
        EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0);
        xline->special = 0;
        break;

    case 12:
        // Light Turn On - brightest near
        EV_LightTurnOn(line, 0);
        xline->special = 0;
        break;

    case 13:
        // Light Turn On 255
        EV_LightTurnOn(line, 1);
        xline->special = 0;
        break;

    case 16:
        // Close Door 30
        EV_DoDoor(line, DT_CLOSE30THENOPEN);
        xline->special = 0;
        break;

    case 17:
        // Start Light Strobing
        EV_StartLightStrobing(line);
        xline->special = 0;
        break;

    case 19:
        // Lower Floor
        EV_DoFloor(line, FT_LOWER);
        xline->special = 0;
        break;

    case 22:
        // Raise floor to nearest height and change texture
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        xline->special = 0;
        break;

    case 25:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISE);
        xline->special = 0;
        break;

    case 30:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, FT_RAISETOTEXTURE);
        xline->special = 0;
        break;

    case 35:
        // Lights Very Dark
        EV_LightTurnOn(line, 35.0f/255.0f);
        xline->special = 0;
        break;

    case 36:
        // Lower Floor (TURBO)
        EV_DoFloor(line, FT_LOWERTURBO);
        xline->special = 0;
        break;

    case 37:
        // LowerAndChange
        EV_DoFloor(line, FT_LOWERANDCHANGE);
        xline->special = 0;
        break;

    case 38:
        // Lower Floor To Lowest
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        xline->special = 0;
        break;

    case 39:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        xline->special = 0;
        break;

    case 40:
        // RaiseCeilingLowerFloor
        EV_DoCeiling(line, CT_RAISETOHIGHEST);
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        xline->special = 0;
        break;

    case 44:
        // Ceiling Crush
        EV_DoCeiling(line, CT_LOWERANDCRUSH);
        xline->special = 0;
        break;

    case 52:
        // EXIT!
        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
        break;

    case 53:
        // Perpetual Platform Raise
        EV_DoPlat(line, PT_PERPETUALRAISE, 0);
        xline->special = 0;
        break;

    case 54:
        // Platform Stop
        P_PlatDeactivate(xline->tag);
        xline->special = 0;
        break;

    case 56:
        // Raise Floor Crush
        EV_DoFloor(line, FT_RAISEFLOORCRUSH);
        xline->special = 0;
        break;

    case 57:
        // Ceiling Crush Stop
        P_CeilingDeactivate(xline->tag);
        xline->special = 0;
        break;

    case 58:
        // Raise Floor 24
        EV_DoFloor(line, FT_RAISE24);
        xline->special = 0;
        break;

    case 59:
        // Raise Floor 24 And Change
        EV_DoFloor(line, FT_RAISE24ANDCHANGE);
        xline->special = 0;
        break;

    case 104:
        // Turn lights off in sector(tag)
        EV_TurnTagLightsOff(line);
        xline->special = 0;
        break;

    case 105:
        // Secret EXIT
        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, true), 0, true);
        break;

    case 106:
        // Build Stairs
        EV_BuildStairs(line, build16);
        xline->special = 0;
        break;

    // RETRIGGERS.  All from here till end.
    case 72:
        // Ceiling Crush
        EV_DoCeiling(line, CT_LOWERANDCRUSH);
        break;

    case 73:
        // Ceiling Crush and Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISE);
        break;

    case 74:
        // Ceiling Crush Stop
        P_CeilingDeactivate(xline->tag);
        break;

    case 75:
        // Close Door
        EV_DoDoor(line, DT_CLOSE);
        break;

    case 76:
        // Close Door 30
        EV_DoDoor(line, DT_CLOSE30THENOPEN);
        break;

    case 77:
        // Fast Ceiling Crush & Raise
        EV_DoCeiling(line, CT_CRUSHANDRAISEFAST);
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
        // Light Turn On 255
        EV_LightTurnOn(line, 1);
        break;

    case 82:
        // Lower Floor To Lowest
        EV_DoFloor(line, FT_LOWERTOLOWEST);
        break;

    case 83:
        // Lower Floor
        EV_DoFloor(line, FT_LOWER);
        break;

    case 84:
        // LowerAndChange
        EV_DoFloor(line, FT_LOWERANDCHANGE);
        break;

    case 86:
        // Open Door
        EV_DoDoor(line, DT_OPEN);
        break;

    case 87:
        // Perpetual Platform Raise
        EV_DoPlat(line, PT_PERPETUALRAISE, 0);
        break;

    case 88:
        // PlatDownWaitUp
        EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0);
        break;

    case 89:
        // Platform Stop
        P_PlatDeactivate(xline->tag);
        break;

    case 90:
        // Raise Door
        EV_DoDoor(line, DT_NORMAL);
        break;

    case 91:
        // Raise Floor
        EV_DoFloor(line, FT_RAISEFLOOR);
        break;

    case 92:
        // Raise Floor 24
        EV_DoFloor(line, FT_RAISE24);
        break;

    case 93:
        // Raise Floor 24 And Change
        EV_DoFloor(line, FT_RAISE24ANDCHANGE);
        break;

    case 94:
        // Raise Floor Crush
        EV_DoFloor(line, FT_RAISEFLOORCRUSH);
        break;

    case 95:
        // Raise floor to nearest height
        // and change texture.
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        break;

    case 96:
        // Raise floor to shortest texture height
        // on either side of lines.
        EV_DoFloor(line, FT_RAISETOTEXTURE);
        break;

    case 97:
        // TELEPORT!
        EV_Teleport(line, side, thing, true);
        break;

    case 100:
        // DJS - Heretic has one turbo door raise
        EV_DoDoor(line, DT_BLAZEOPEN);
        break;
    }
}

/**
 * Called when a thing shoots a special line.
 */
static void P_ShootSpecialLine(mobj_t* thing, LineDef* line)
{
    xline_t* xline = P_ToXLine(line);

    // Impacts that other things can activate.
    if(!thing->player)
    {
        switch(xline->special)
        {
        default: return;

        case 46: ///< OPEN DOOR IMPACT
            break;
        }
    }

    switch(xline->special)
    {
    default: break;

    case 24:
        // RAISE FLOOR
        EV_DoFloor(line, FT_RAISEFLOOR);
        P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;

    case 46:
        // OPEN DOOR
        EV_DoDoor(line, DT_OPEN);
        P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 47:
        // RAISE FLOOR NEAR AND CHANGE
        EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0);
        P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;
    }
}

/**
 * Called every tic frame that the player origin is in a special sector.
 */
void P_PlayerInSpecialSector(player_t* player)
{
    Sector* sector = P_GetPtrp(player->plr->mo->bspLeaf, DMU_SECTOR);

    // Falling, not all the way down yet?
    if(!FEQUAL(player->plr->mo->origin[VZ], P_GetDoublep(sector, DMU_FLOOR_HEIGHT))) return;

    // Has hitten ground.
    switch(P_ToXSector(sector)->special)
    {
    case 5:
        // LAVA DAMAGE WEAK
        if(!(mapTime & 15))
        {
            P_DamageMobj(player->plr->mo, &LavaInflictor, NULL, 5, false);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 7:
        // SLUDGE DAMAGE
        if(!(mapTime & 31))
            P_DamageMobj(player->plr->mo, NULL, NULL, 4, false);
        break;

    case 16:
        // LAVA DAMAGE HEAVY
        if(!(mapTime & 15))
        {
            P_DamageMobj(player->plr->mo, &LavaInflictor, NULL, 8, false);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 4:
        // LAVA DAMAGE WEAK PLUS SCROLL EAST
        P_Thrust(player, 0, FIX2FLT(2048 * 28));
        if(!(mapTime & 15))
        {
            P_DamageMobj(player->plr->mo, &LavaInflictor, NULL, 5, false);
            P_HitFloor(player->plr->mo);
        }
        break;

    case 9:
        if(!IS_CLIENT)
        {
            // SECRET SECTOR
            player->secretCount++;
            P_ToXSector(sector)->special = 0;
            if(cfg.secretMsg)
            {
                P_SetMessage(player, 0, "You've found a secret area!");
                S_ConsoleSound(SFX_SECRET, 0, player - players);
            }
        }
        break;

    case 11:
        // EXIT SUPER DAMAGE! (for E1M8 finale)
/*      // Not used in Heretic
        player->cheats &= ~CF_GODMODE;

        if(!(leveltime & 0x1f))
            P_DamageMobj(player->plr->mo, NULL, NULL, 20);

        if(player->health <= 10)
            G_ExitLevel();
*/
        break;

    // These specials are handled elsewhere in jHeretic.
    case 15: // LOW FRICTION

    case 40: // WIND SPECIALS
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
        break;

    default:
        P_PlayerInWindSector(player);
        break;
    }
}

void P_SpawnSectorSpecialThinkers(void)
{
    uint i;

    // Clients spawn specials only on the server's instruction.
    if(IS_CLIENT) return;

    for(i = 0; i < numsectors; ++i)
    {
        Sector* sec     = P_ToPtr(DMU_SECTOR, i);
        xsector_t* xsec = P_ToXSector(sec);

        switch(xsec->special)
        {
        default: break;

        case 1: ///< FLICKERING LIGHTS
            P_SpawnLightFlash(sec);
            break;

        case 2: ///< STROBE FAST
            P_SpawnStrobeFlash(sec, FASTDARK, 0);
            break;

        case 3: ///, STROBE SLOW
            P_SpawnStrobeFlash(sec, SLOWDARK, 0);
            break;

        case 4: ///< STROBE FAST/DEATH SLIME
            P_SpawnStrobeFlash(sec, FASTDARK, 0);
            xsec->special = 4;
            break;

        case 8: ///< GLOWING LIGHT
            P_SpawnGlowingLight(sec);
            break;

        case 12: ///< SYNC STROBE SLOW
            P_SpawnStrobeFlash(sec, SLOWDARK, 1);
            break;

        case 13: ///< SYNC STROBE FAST
            P_SpawnStrobeFlash(sec, FASTDARK, 1);
            break;

        case 10: ///< DOOR CLOSE IN 30 SECONDS
            P_SpawnDoorCloseIn30(sec);
            break;

        case 14: ///< DOOR RAISE IN 5 MINUTES
            P_SpawnDoorRaiseIn5Mins(sec);
            break;
        }
    }
}

void P_SpawnLineSpecialThinkers(void)
{
    // Stub.
}

void P_SpawnAllSpecialThinkers(void)
{
    P_SpawnSectorSpecialThinkers();
    P_SpawnLineSpecialThinkers();
}

void P_InitLava(void)
{
    memset(&LavaInflictor, 0, sizeof(mobj_t));
    LavaInflictor.type = MT_PHOENIXFX2;
    LavaInflictor.flags2 = MF2_FIREDAMAGE | MF2_NODMGTHRUST;
}

void P_PlayerInWindSector(player_t* player)
{
    static const coord_t pushTab[5] = {
        2048.0 / FRACUNIT * 5,
        2048.0 / FRACUNIT * 10,
        2048.0 / FRACUNIT * 25,
        2048.0 / FRACUNIT * 30,
        2048.0 / FRACUNIT * 35
    };
    Sector* sector = P_GetPtrp(player->plr->mo->bspLeaf, DMU_SECTOR);
    xsector_t* xsector = P_ToXSector(sector);

    switch(xsector->special)
    {
    case 20:
    case 21:
    case 22:
    case 23:
    case 24: // Scroll_East
        P_Thrust(player, 0, pushTab[xsector->special - 20]);
        break;

    case 25:
    case 26:
    case 27:
    case 28:
    case 29: // Scroll_North
        P_Thrust(player, ANG90, pushTab[xsector->special - 25]);
        break;

    case 30:
    case 31:
    case 32:
    case 33:
    case 34: // Scroll_South
        P_Thrust(player, ANG270, pushTab[xsector->special - 30]);
        break;

    case 35:
    case 36:
    case 37:
    case 38:
    case 39: // Scroll_West
        P_Thrust(player, ANG180, pushTab[xsector->special - 35]);
        break;

    default:
        break;
    }

    // The other wind types (40..51).
    P_WindThrust(player->plr->mo);
}

void P_InitAmbientSound(void)
{
    AmbSfxCount = 0;
    AmbSfxVolume = 0;
    AmbSfxTics = 10 * TICSPERSEC;
    AmbSfxPtr = AmbSndSeqInit;
}

/**
 * Called by spawnMapThing during (P_setup):P_SetupMap.
 */
void P_AddAmbientSfx(int sequence)
{
    if(AmbSfxCount == MAX_AMBIENT_SFX)
    {
        Con_Error("Too many ambient sound sequences");
    }
    LevelAmbientSfx[AmbSfxCount++] = AmbientSfx[sequence];
}

/**
 * Called every tic by (P_tick):P_Ticker.
 */
void P_AmbientSound(void)
{
    afxcmd_t cmd;
    int     sound;
    boolean done;

    // No ambient sound sequences on current level
    if(!AmbSfxCount)
        return;

    if(--AmbSfxTics)
        return;

    done = false;
    do
    {
        cmd = *AmbSfxPtr++;
        switch(cmd)
        {
        case afxcmd_play:
            AmbSfxVolume = P_Random() >> 2;
            S_StartSoundAtVolume(*AmbSfxPtr++, NULL, AmbSfxVolume / 127.0f);
            break;

        case afxcmd_playabsvol:
            sound = *AmbSfxPtr++;
            AmbSfxVolume = *AmbSfxPtr++;
            S_StartSoundAtVolume(sound, NULL, AmbSfxVolume / 127.0f);
            break;

        case afxcmd_playrelvol:
            sound = *AmbSfxPtr++;
            AmbSfxVolume += *AmbSfxPtr++;

            if(AmbSfxVolume < 0)
                AmbSfxVolume = 0;
            else if(AmbSfxVolume > 127)
                AmbSfxVolume = 127;

            S_StartSoundAtVolume(sound, NULL, AmbSfxVolume / 127.0f);
            break;

        case afxcmd_delay:
            AmbSfxTics = *AmbSfxPtr++;
            done = true;
            break;

        case afxcmd_delayrand:
            AmbSfxTics = P_Random() & (*AmbSfxPtr++);
            done = true;
            break;

        case afxcmd_end:
            AmbSfxTics = 6 * TICSPERSEC + P_Random();
            AmbSfxPtr = LevelAmbientSfx[P_Random() % AmbSfxCount];
            done = true;
            break;

        default:
            Con_Error("P_AmbientSound: Unknown afxcmd %d", cmd);
            break;
        }
    } while(done == false);
}

boolean P_UseSpecialLine2(mobj_t* mo, LineDef* line, int side)
{
    xline_t            *xline = P_ToXLine(line);

    // Switches that other things can activate.
    if(!mo->player)
    {
        // never DT_OPEN secret doors
        if(xline->flags & ML_SECRET)
            return false;
    }

    if(!mo->player)
    {
        switch(xline->special)
        {
        case 1: // MANUAL DOOR RAISE
        case 32: // MANUAL BLUE
        case 33: // MANUAL RED
        case 34: // MANUAL YELLOW
            break;

        default:
            return false;
        }
    }

    // Do something.
    switch(xline->special)
    {
    // MANUALS
    case 1: // Vertical Door
    case 26: // Blue Door/Locked
    case 27: // Yellow Door /Locked
    case 28: // Red Door /Locked

    case 31: // Manual door DT_OPEN
    case 32: // Blue locked door DT_OPEN
    case 33: // Red locked door DT_OPEN
    case 34: // Yellow locked door DT_OPEN
        EV_VerticalDoor(line, mo);
        break;

    // SWITCHES
    case 7: // Switch_Build_Stairs (8 pixel steps)
        if(EV_BuildStairs(line, build8))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 107: // Switch_Build_Stairs_16 (16 pixel steps)
        if(EV_BuildStairs(line, build16))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 9: // Change Donut.
        if(EV_DoDonut(line))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 11: // Exit level.
        if(cyclingMaps && mapCycleNoExit)
            break;

        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
        P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;

    case 14: // Raise Floor 32 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 32))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 15: // Raise Floor 24 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 24))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 18: // Raise Floor to next highest floor.
        if(EV_DoFloor(line, FT_RAISEFLOORTONEAREST))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 20: // Raise Plat next highest floor and change texture.
        if(EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 21: // PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAY, 0))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 23: // Lower Floor to Lowest.
        if(EV_DoFloor(line, FT_LOWERTOLOWEST))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 29: // Raise Door.
        if(EV_DoDoor(line, DT_NORMAL))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 41: // Lower Ceiling to Floor.
        if(EV_DoCeiling(line, CT_LOWERTOFLOOR))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 71: // Turbo Lower Floor.
        if(EV_DoFloor(line, FT_LOWERTURBO))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 49: // Lower Ceiling And Crush.
        if(EV_DoCeiling(line, CT_LOWERANDCRUSH))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 50: // Close Door.
        if(EV_DoDoor(line, DT_CLOSE))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 51: // Secret EXIT.
        if(cyclingMaps && mapCycleNoExit)
            break;

        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, true), 0, true);
        P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
        xline->special = 0;
        break;

    case 55: // Raise Floor Crush.
        if(EV_DoFloor(line, FT_RAISEFLOORCRUSH))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 101: // Raise Floor.
        if(EV_DoFloor(line, FT_RAISEFLOOR))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 102: // Lower Floor to Surrounding floor height.
        if(EV_DoFloor(line, FT_LOWER))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    case 103: // Open Door.
        if(EV_DoDoor(line, DT_OPEN))
        {
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, 0);
            xline->special = 0;
        }
        break;

    // BUTTONS
    case 42: // Close Door.
        if(EV_DoDoor(line, DT_CLOSE))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 43: // Lower Ceiling to Floor.
        if(EV_DoCeiling(line, CT_LOWERTOFLOOR))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 45: // Lower Floor to Surrounding floor height.
        if(EV_DoFloor(line, FT_LOWER))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 60: // Lower Floor to Lowest.
        if(EV_DoFloor(line, FT_LOWERTOLOWEST))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 61: // Open Door.
        if(EV_DoDoor(line, DT_OPEN))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 62: // PlatDownWaitUpStay.
        if(EV_DoPlat(line, PT_DOWNWAITUPSTAY, 1))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 63: // Raise Door.
        if(EV_DoDoor(line, DT_NORMAL))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 64: // Raise Floor to ceiling.
        if(EV_DoFloor(line, FT_RAISEFLOOR))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 66: // Raise Floor 24 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 24))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 67: // Raise Floor 32 and change texture.
        if(EV_DoPlat(line, PT_RAISEANDCHANGE, 32))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 65: // Raise Floor Crush.
        if(EV_DoFloor(line, FT_RAISEFLOORCRUSH))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 68: // Raise Plat to next highest floor and change texture.
        if(EV_DoPlat(line, PT_RAISETONEARESTANDCHANGE, 0))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 69: // Raise Floor to next highest floor.
        if(EV_DoFloor(line, FT_RAISEFLOORTONEAREST))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    case 70: // Turbo Lower Floor.
        if(EV_DoFloor(line, FT_LOWERTURBO))
            P_ToggleSwitch(P_GetPtrp(line, DMU_SIDEDEF0), SFX_NONE, false, BUTTONTIME);
        break;

    default:
        break;
    }

    return true;
}
