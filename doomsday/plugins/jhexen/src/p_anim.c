/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

#define ANIM_SCRIPT_NAME "ANIMDEFS"
#define MAX_ANIM_DEFS 20
#define MAX_FRAME_DEFS 96
#define ANIM_FLAT 0
#define ANIM_TEXTURE 1
#define SCI_FLAT "flat"
#define SCI_TEXTURE "texture"
#define SCI_PIC "pic"
#define SCI_TICS "tics"
#define SCI_RAND "rand"

#define LIGHTNING_SPECIAL   198
#define LIGHTNING_SPECIAL2  199
#define SKYCHANGE_SPECIAL   200

// TYPES -------------------------------------------------------------------

typedef struct framedef_s {
    int     index;
    int     tics;
} framedef_t;

typedef struct animdef_s {
    int     type;
    int     index;
    int     tics;
    int     currentFrameDef;
    int     startFrameDef;
    int     endFrameDef;
} animdef_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void P_LightningFlash(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t Sky1ColumnOffset;
extern fixed_t Sky2ColumnOffset;
extern int Sky1Texture;
extern boolean DoubleSky;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     Sky1Texture;
int     Sky2Texture;
fixed_t Sky1ColumnOffset;
fixed_t Sky2ColumnOffset;
fixed_t Sky1ScrollDelta;
fixed_t Sky2ScrollDelta;
boolean DoubleSky;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean LevelHasLightning;
static int NextLightningFlash;
static int LightningFlash;
static int *LightningLightLevels;

// CODE --------------------------------------------------------------------

void P_AnimateSurfaces(void)
{
    int         i;
    line_t     *line;

    // Update scrolling textures
    if(P_IterListSize(linespecials))
    {
        P_IterListResetIterator(linespecials, false);
        while((line = P_IterListIterator(linespecials)) != NULL)
        {
            side_t* side = 0;
            fixed_t texOff[2];

            side = P_GetPtrp(line, DMU_SIDE0);
            for(i =0; i < 3; ++i)
            {

                P_GetFixedpv(side,
                             (i==0? DMU_TOP_TEXTURE_OFFSET_XY :
                              i==1? DMU_MIDDLE_TEXTURE_OFFSET_XY :
                              DMU_BOTTOM_TEXTURE_OFFSET_XY), texOff);

                switch (P_XLine(line)->special)
                {
                case 100:               // Scroll_Texture_Left
                    texOff[0] += P_XLine(line)->arg1 << 10;
                    break;
                case 101:               // Scroll_Texture_Right
                    texOff[0] -= P_XLine(line)->arg1 << 10;
                    break;
                case 102:               // Scroll_Texture_Up
                    texOff[1] += P_XLine(line)->arg1 << 10;
                    break;
                case 103:               // Scroll_Texture_Down
                    texOff[1] -= P_XLine(line)->arg1 << 10;
                    break;
                }

                P_SetFixedpv(side,
                             (i==0? DMU_TOP_TEXTURE_OFFSET_XY :
                              i==1? DMU_MIDDLE_TEXTURE_OFFSET_XY :
                              DMU_BOTTOM_TEXTURE_OFFSET_XY), texOff);
            }
        }
    }

    // Update sky column offsets
    Sky1ColumnOffset += Sky1ScrollDelta;
    Sky2ColumnOffset += Sky2ScrollDelta;
    Rend_SkyParams(1, DD_OFFSET, FIX2FLT(Sky1ColumnOffset));
    Rend_SkyParams(0, DD_OFFSET, FIX2FLT(Sky2ColumnOffset));

    if(LevelHasLightning)
    {
        if(!NextLightningFlash || LightningFlash)
        {
            P_LightningFlash();
        }
        else
        {
            NextLightningFlash--;
        }
    }
}

static void P_LightningFlash(void)
{
    uint        i;
    sector_t   *tempSec;
    int        *tempLight;
    boolean     foundSec;
    int         flashLight;

    if(LightningFlash)
    {
        LightningFlash--;
        if(LightningFlash)
        {
            tempLight = LightningLightLevels;
            for(i = 0; i < numsectors; ++i)
            {
                tempSec = P_ToPtr(DMU_SECTOR, i);
                if(P_GetIntp(tempSec, DMU_CEILING_TEXTURE) == skyflatnum ||
                   P_XSector(tempSec)->special == LIGHTNING_SPECIAL ||
                   P_XSector(tempSec)->special == LIGHTNING_SPECIAL2)
                {
                    if(*tempLight < 255.0f * P_GetFloatp(tempSec, DMU_LIGHT_LEVEL) - 4)
                    {
                        P_SetFloatp(tempSec, DMU_LIGHT_LEVEL,
                                    P_GetFloatp(tempSec, DMU_LIGHT_LEVEL) - ((1.0f / 255.0f) * 4));
                    }
                    tempLight++;
                }
            }
        }
        else
        {   // remove the alternate lightning flash special
            tempLight = LightningLightLevels;
            for(i = 0; i < numsectors; ++i)
            {
                tempSec = P_ToPtr(DMU_SECTOR, i);
                if(P_GetIntp(tempSec, DMU_CEILING_TEXTURE) == skyflatnum ||
                   P_XSector(tempSec)->special == LIGHTNING_SPECIAL ||
                   P_XSector(tempSec)->special == LIGHTNING_SPECIAL2)
                {
                    P_SetFloatp(tempSec, DMU_LIGHT_LEVEL, (float) (*tempLight) / 255.0f);
                    tempLight++;
                }
            }

            Rend_SkyParams(1, DD_DISABLE, 0);
            Rend_SkyParams(0, DD_ENABLE, 0);
        }
        return;
    }

    LightningFlash = (P_Random() & 7) + 8;
    flashLight = 200 + (P_Random() & 31);
    tempLight = LightningLightLevels;
    foundSec = false;
    for(i = 0; i < numsectors; ++i)
    {
        tempSec = P_ToPtr(DMU_SECTOR, i);
        if(P_GetIntp(tempSec, DMU_CEILING_TEXTURE) == skyflatnum ||
           P_XSector(tempSec)->special == LIGHTNING_SPECIAL ||
           P_XSector(tempSec)->special == LIGHTNING_SPECIAL2)
        {
            int newLevel = *tempLight = 255.0f * P_GetFloatp(tempSec, DMU_LIGHT_LEVEL);

            if(P_XSector(tempSec)->special == LIGHTNING_SPECIAL)
            {
                newLevel += 64;
                if(newLevel > flashLight)
                {
                    newLevel = flashLight;
                }
            }
            else if(P_XSector(tempSec)->special == LIGHTNING_SPECIAL2)
            {
                newLevel += 32;
                if(newLevel > flashLight)
                {
                    newLevel = flashLight;
                }
            }
            else
            {
                newLevel = flashLight;
            }

            if(newLevel < *tempLight)
            {
                newLevel = *tempLight;
            }
            P_SetFloatp(tempSec, DMU_LIGHT_LEVEL, (float) newLevel / 255.0f);
            tempLight++;
            foundSec = true;
        }
    }

    if(foundSec)
    {
        mobj_t *plrmo = players[displayplayer].plr->mo;
        mobj_t *crashorigin = NULL;

        // Set the alternate (lightning) sky.
        Rend_SkyParams(0, DD_DISABLE, 0);
        Rend_SkyParams(1, DD_ENABLE, 0);

        // If 3D sounds are active, position the clap somewhere above
        // the player.
        if(cfg.snd_3D && plrmo)
        {
            // SpawnMobj calls P_Random, and we don't want that the
            // random number generator gets out of sync.
            //P_SaveRandom();
            crashorigin =
                P_SpawnMobj(plrmo->pos[VX] + (16 * (M_Random() - 127) << FRACBITS),
                            plrmo->pos[VY] + (16 * (M_Random() - 127) << FRACBITS),
                            plrmo->pos[VZ] + (4000 << FRACBITS), MT_CAMERA);
            //P_RestoreRandom();
            crashorigin->tics = 5 * 35; // Five seconds will do.
        }
        // Make it loud!
        S_StartSound(SFX_THUNDER_CRASH | DDSF_NO_ATTENUATION, crashorigin);
    }

    // Calculate the next lighting flash
    if(!NextLightningFlash)
    {
        if(P_Random() < 50)
        {                       // Immediate Quick flash
            NextLightningFlash = (P_Random() & 15) + 16;
        }
        else
        {
            if(P_Random() < 128 && !(leveltime & 32))
            {
                NextLightningFlash = ((P_Random() & 7) + 2) * 35;
            }
            else
            {
                NextLightningFlash = ((P_Random() & 15) + 5) * 35;
            }
        }
    }
}

void P_ForceLightning(void)
{
    NextLightningFlash = 0;
}

void P_InitLightning(void)
{
    uint        i, secCount;
    sector_t   *sec;
    xsector_t  *xsec;

    if(!P_GetMapLightning(gamemap))
    {
        LevelHasLightning = false;
        LightningFlash = 0;
        return;
    }

    LightningFlash = 0;
    secCount = 0;
    for(i = 0; i < numsectors; ++i)
    {
        sec = P_ToPtr(DMU_SECTOR, i);
        xsec = P_XSector(sec);
        if(P_GetIntp(sec, DMU_CEILING_TEXTURE) == skyflatnum ||
           xsec->special == LIGHTNING_SPECIAL ||
           xsec->special == LIGHTNING_SPECIAL2)
        {
            secCount++;
        }
    }

    if(secCount > 0)
    {
        LevelHasLightning = true;
    }
    else
    {
        LevelHasLightning = false;
        return;
    }

    LightningLightLevels = Z_Malloc(secCount * sizeof(int), PU_LEVEL, NULL);
    NextLightningFlash = ((P_Random() & 15) + 5) * 35;  // don't flash at level start
}

/**
 * Initialize flat and texture animation lists.
 */
void P_InitPicAnims(void)
{
    int         base;
    boolean     ignore;
    boolean     done;
    int         AnimDefCount = 0;
    int         groupNumber = 0, picBase = 0;
    int         type = 0, index = 0;

    SC_Open(ANIM_SCRIPT_NAME);
    while(SC_GetString())
    {
        if(AnimDefCount == MAX_ANIM_DEFS)
        {
            Con_Error("P_InitPicAnims: too many AnimDefs.");
        }
        if(SC_Compare(SCI_FLAT))
        {
            type = ANIM_FLAT;
        }
        else if(SC_Compare(SCI_TEXTURE))
        {
            type = ANIM_TEXTURE;
        }
        else
        {
            SC_ScriptError(NULL);
        }
        SC_MustGetString();     // Name

        ignore = false;
        if(type == ANIM_FLAT)
        {
            if(W_CheckNumForName(sc_String) == -1)
            {
                ignore = true;
            }
            else
            {
                picBase = R_FlatNumForName(sc_String);
                groupNumber =
                    R_CreateAnimGroup(DD_FLAT, AGF_SMOOTH | AGF_FIRST_ONLY);
            }
        }
        else
        {                       // Texture
            if(R_CheckTextureNumForName(sc_String) == -1)
            {
                ignore = true;
            }
            else
            {
                picBase = R_TextureNumForName(sc_String);
                groupNumber =
                    R_CreateAnimGroup(DD_TEXTURE, AGF_SMOOTH | AGF_FIRST_ONLY);
            }
        }

        done = false;
        while(done == false)
        {
            if(SC_GetString())
            {
                if(SC_Compare(SCI_PIC))
                {
                    SC_MustGetNumber();
                    if(ignore == false)
                    {
                        index = picBase + sc_Number - 1;
                    }
                    SC_MustGetString();
                    if(SC_Compare(SCI_TICS))
                    {
                        SC_MustGetNumber();
                        if(ignore == false)
                        {
                            R_AddToAnimGroup(groupNumber, index, sc_Number, 0);
                        }
                    }
                    else if(SC_Compare(SCI_RAND))
                    {
                        SC_MustGetNumber();
                        base = sc_Number;
                        SC_MustGetNumber();
                        if(ignore == false)
                        {
                            R_AddToAnimGroup(groupNumber, index, base,
                                             sc_Number - base);
                        }
                    }
                    else
                    {
                        SC_ScriptError(NULL);
                    }
                }
                else
                {
                    SC_UnGet();
                    done = true;
                }
            }
            else
            {
                done = true;
            }
        }
    }
    SC_Close();
}

void P_InitSky(int map)
{
    Sky1Texture = P_GetMapSky1Texture(map);
    Sky2Texture = P_GetMapSky2Texture(map);
    Sky1ScrollDelta = P_GetMapSky1ScrollDelta(map);
    Sky2ScrollDelta = P_GetMapSky2ScrollDelta(map);
    Sky1ColumnOffset = 0;
    Sky2ColumnOffset = 0;
    DoubleSky = P_GetMapDoubleSky(map);

    // First disable all sky layers.
    Rend_SkyParams(DD_SKY, DD_DISABLE, 0);

    // Sky2 is layer zero and Sky1 is layer one.
    Rend_SkyParams(0, DD_OFFSET, 0);
    Rend_SkyParams(1, DD_OFFSET, 0);
    if(DoubleSky)
    {
        Rend_SkyParams(0, DD_ENABLE, 0);
        Rend_SkyParams(0, DD_MASK, DD_NO);
        Rend_SkyParams(0, DD_TEXTURE, Sky2Texture);

        Rend_SkyParams(1, DD_ENABLE, 0);
        Rend_SkyParams(1, DD_MASK, DD_YES);
        Rend_SkyParams(1, DD_TEXTURE, Sky1Texture);
    }
    else
    {
        Rend_SkyParams(0, DD_ENABLE, 0);
        Rend_SkyParams(0, DD_MASK, DD_NO);
        Rend_SkyParams(0, DD_TEXTURE, Sky1Texture);

        Rend_SkyParams(1, DD_DISABLE, 0);
        Rend_SkyParams(1, DD_MASK, DD_NO);
        Rend_SkyParams(1, DD_TEXTURE, Sky2Texture);
    }
}
