/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * hu_stuff.c: Heads-up displays, font handling, text drawing routines
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_stuff.h"
#include "hu_log.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "f_infine.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         active;
    int             scoreHideTics;
    float           scoreAlpha;
} hudstate_t;

// Column flags
#define CF_HIDE                 0x0001
#define CF_FIXEDWIDTH           0x0002

typedef struct {
    const char*     label;
    int             type;
    short           flags; // CF_* flags.
    boolean         alignRight;
} column_t;

typedef struct fogeffectlayer_s {
    float           texOffset[2];
    float           texAngle;
    float           posAngle;
} fogeffectlayer_t;

typedef struct fogeffectdata_s {
    DGLuint         texture;
    float           alpha, targetAlpha;
    fogeffectlayer_t layers[2];
    float           joinY;
    boolean         scrollDir;
} fogeffectdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if __JDOOM__ || __JDOOM64__
// Name graphics of each map.
patchid_t* mapNamePatches = NULL;
// Name graphics of each skill mode.
patchid_t skillModeNames[NUM_SKILL_MODES];
#endif

#if __JDOOM__
// Name graphics of each episode.
patchid_t* episodeNamePatches = NULL;
#endif

#if __JHERETIC__ || __JHEXEN__
patchid_t dpInvItemBox;
patchid_t dpInvSelectBox;
patchid_t dpInvPageLeft[2];
patchid_t dpInvPageRight[2];
#endif

boolean shiftdown = false;
const char shiftXForm[] = {
    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"',                        // shift-'
    '(', ')', '*', '+',
    '<',                        // shift-,
    '_',                        // shift--
    '>',                        // shift-.
    '?',                        // shift-/
    ')',                        // shift-0
    '!',                        // shift-1
    '@',                        // shift-2
    '#',                        // shift-3
    '$',                        // shift-4
    '%',                        // shift-5
    '^',                        // shift-6
    '&',                        // shift-7
    '*',                        // shift-8
    '(',                        // shift-9
    ':',
    ':',                        // shift-;
    '<',
    '+',                        // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[',                        // shift-[
    '!',                        // shift-backslash
    ']',                        // shift-]
    '"', '_',
    '\'',                       // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];
static fogeffectdata_t fogEffectData;

static patchinfo_t borderPatches[8];
static patchid_t dpSliderLeft;
static patchid_t dpSliderMiddle;
static patchid_t dpSliderRight;
static patchid_t dpSliderHandle;
static patchid_t m_pause; // Paused graphic.

// CODE -------------------------------------------------------------------

static __inline short translatePatchToTextDrawFlags(short in)
{
    short out = DTF_NO_EFFECTS;
    if(in & DPF_ALIGN_LEFT)
        out |= DTF_ALIGN_LEFT;
    if(in & DPF_ALIGN_RIGHT)
        out |= DTF_ALIGN_RIGHT;
    if(in & DPF_ALIGN_BOTTOM)
        out |= DTF_ALIGN_BOTTOM;
    if(in & DPF_ALIGN_TOP)
        out |= DTF_ALIGN_TOP;
    return out;
}

/**
 * Loads the font patches and inits various strings
 * JHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void Hu_LoadData(void)
{
#if __JDOOM__ || __JDOOM64__
    static const char*  skillModePatchNames[] =
    {
        "M_JKILL",
        "M_ROUGH",
        "M_HURT",
        "M_ULTRA",
# if __JDOOM__
        "M_NMARE"
# endif
    };
#endif
#if __JDOOM__
    static const char*  episodePatchNames[] =
    {
        "M_EPI1",
        "M_EPI2",
        "M_EPI3",
        "M_EPI4"
    };
#endif
#if __JDOOM__ || __JDOOM64__
    char name[9];
#endif
    int i;

    // Intialize the background fog effect.
    fogEffectData.texture = 0;
    fogEffectData.alpha = fogEffectData.targetAlpha = 0;
    fogEffectData.joinY = 0.5f;
    fogEffectData.scrollDir = true;
    fogEffectData.layers[0].texOffset[VX] =
        fogEffectData.layers[0].texOffset[VY] = 0;
    fogEffectData.layers[0].texAngle = 93;
    fogEffectData.layers[0].posAngle = 35;
    fogEffectData.layers[1].texOffset[VX] =
        fogEffectData.layers[1].texOffset[VY] = 0;
    fogEffectData.layers[1].texAngle = 12;
    fogEffectData.layers[1].posAngle = 77;

    // Load the background fog texture.
    if(!fogEffectData.texture && !Get(DD_NOVIDEO))
    {
        fogEffectData.texture =
            GL_NewTextureWithParams3(DGL_LUMINANCE, 64, 64,
                                     W_CacheLumpName("menufog", PU_CACHE),
                                     0, DGL_NEAREST, DGL_LINEAR,
                                     -1 /*best anisotropy*/,
                                     DGL_REPEAT, DGL_REPEAT);
    }

    // Load the border patches
    for(i = 1; i < 9; ++i)
        R_PrecachePatch(borderLumps[i], &borderPatches[i-1]);

#if __JDOOM__ || __JDOOM64__
    dpSliderLeft = R_PrecachePatch("M_THERML", NULL);
    dpSliderMiddle = R_PrecachePatch("M_THERM2", NULL);
    dpSliderRight = R_PrecachePatch("M_THERMR", NULL);
    dpSliderHandle = R_PrecachePatch("M_THERMO", NULL);
    m_pause = R_PrecachePatch("M_PAUSE", NULL);
#elif __JHERETIC__ || __JHEXEN__
    dpSliderLeft = R_PrecachePatch("M_SLDLT", NULL);
    dpSliderMiddle = R_PrecachePatch("M_SLDMD1", NULL);
    dpSliderRight = R_PrecachePatch("M_SLDRT", NULL);
    dpSliderHandle = R_PrecachePatch("M_SLDKB", NULL);
    m_pause = R_PrecachePatch("PAUSED", NULL);
#endif

#if __JDOOM__ || __JDOOM64__
    for(i = 0; i < NUM_SKILL_MODES; ++i)
    {
        skillModeNames[i] = R_PrecachePatch(skillModePatchNames[i], NULL);
    }

    // Load the map name patches.
# if __JDOOM64__
    {
        int NUMCMAPS = 32;
        mapNamePatches = Z_Malloc(sizeof(patchid_t) * NUMCMAPS, PU_STATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "WILV%2.2d", i);
            mapNamePatches[i] = R_PrecachePatch(name, NULL);
        }
    }
# else
    if(gameMode == commercial)
    {
        int NUMCMAPS = 32;
        mapNamePatches = Z_Malloc(sizeof(patchid_t) * NUMCMAPS, PU_STATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "CWILV%2.2d", i);
            mapNamePatches[i] = R_PrecachePatch(name, NULL);
        }
    }
    else
    {
        int j;

        // Don't waste space - patches are loaded back to back
        // ie no space in the array is left for E1M10
        mapNamePatches = Z_Malloc(sizeof(patchid_t) * (9*4), PU_STATIC, 0);
        for(i = 0; i < 4; ++i) // Number of episodes.
        {
            for(j = 0; j < 9; ++j) // Number of maps per episode.
            {
                sprintf(name, "WILV%2.2d", (i * 10) + j);
                mapNamePatches[(i* 9) + j] = R_PrecachePatch(name, NULL);
            }
        }

        episodeNamePatches = Z_Malloc(sizeof(patchid_t) * 4, PU_STATIC, 0);
        for(i = 0; i < 4; ++i)
            episodeNamePatches[i] = R_PrecachePatch(episodePatchNames[i], NULL);
    }
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    dpInvItemBox = R_PrecachePatch("ARTIBOX", NULL);
    dpInvSelectBox = R_PrecachePatch("SELECTBO", NULL);
    dpInvPageLeft[0] = R_PrecachePatch("INVGEML1", NULL);
    dpInvPageLeft[1] = R_PrecachePatch("INVGEML2", NULL);
    dpInvPageRight[0] = R_PrecachePatch("INVGEMR1", NULL);
    dpInvPageRight[1] = R_PrecachePatch("INVGEMR2", NULL);
#endif

    Chat_Init();
}

void Hu_UnloadData(void)
{
#if __JDOOM__ || __JDOOM64__
    if(mapNamePatches)
        Z_Free(mapNamePatches);
#endif

    if(!Get(DD_NOVIDEO))
    {
        if(fogEffectData.texture)
            DGL_DeleteTextures(1, (DGLuint*) &fogEffectData.texture);
        fogEffectData.texture = 0;
    }
}

void HU_Stop(int player)
{
    assert(player >= 0 && player < MAXPLAYERS);
    {
    hudstate_t* hud = &hudStates[player];
    hud->active = false;
    }
}

void HU_Start(int player)
{
    assert(player >= 0 && player < MAXPLAYERS);
    {
    hudstate_t* hud = &hudStates[player];
    Chat_Start();
    Hu_LogStart(player);

    hud = &hudStates[player];
    if(hud->active)
        HU_Stop(player);

    hud->active = true;
    }
}

static void drawQuad(float x, float y, float w, float h, float s, float t,
    float r, float g, float b, float a)
{
    DGL_Color4f(r, g, b, a);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(x, y + h);
    DGL_End();
}

void HU_DrawText(const char* str, compositefontid_t font, float x, float y,
    float scale, float r, float g, float b, float a, short flags)
{
    if(!str || !str[0])
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);
    DGL_Translatef(-x, -y, 0);

    DGL_Color4f(r, g, b, a);
    GL_DrawTextFragment3(str, x, y, font, flags);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

typedef struct {
    int player, pClass, team;
    int kills, suicides;
    float color[3];
} scoreinfo_t;

int scoreInfoCompare(const void* a, const void* b)
{
    const scoreinfo_t* infoA = (scoreinfo_t*) a;
    const scoreinfo_t* infoB = (scoreinfo_t*) b;

    if(infoA->kills > infoB->kills)
        return -1;

    if(infoB->kills > infoA->kills)
        return 1;

    if(deathmatch)
    {   // In deathmatch, suicides affect your place on the scoreboard.
        if(infoA->suicides < infoB->suicides)
            return -1;

        if(infoB->suicides < infoA->suicides)
            return 1;
    }

    return 0;
}

static void sortScoreInfo(scoreinfo_t* vec, size_t size)
{
    qsort(vec, size, sizeof(scoreinfo_t), scoreInfoCompare);
}

static int buildScoreBoard(scoreinfo_t* scoreBoard, int maxPlayers, int player)
{
#if __JHEXEN__
    static const int plrColors[] = {
        AM_PLR1_COLOR,
        AM_PLR2_COLOR,
        AM_PLR3_COLOR,
        AM_PLR4_COLOR,
        AM_PLR5_COLOR,
        AM_PLR6_COLOR,
        AM_PLR7_COLOR,
        AM_PLR8_COLOR
    };
#else
    static const float green[3] = { 0.f,    .8f,  0.f   };
    static const float gray[3]  = {  .45f,  .45f,  .45f };
    static const float brown[3] = {  .7f,   .5f,   .4f  };
    static const float red[3]   = { 1.f,   0.f,   0.f   };
#endif
    int i, j, n, inCount;

    memset(scoreBoard, 0, sizeof(*scoreBoard) * maxPlayers);
    inCount = 0;
    for(i = 0, n = 0; i < maxPlayers; ++i)
    {
        player_t* plr = &players[i];
        scoreinfo_t* info;

        if(!plr->plr->inGame)
            continue;

        inCount++;
        info = &scoreBoard[n++];
        info->player = i;
#if __JHERETIC__
        info->pClass = (plr->morphTics > 0? PCLASS_CHICKEN : PCLASS_PLAYER);
#elif __JHEXEN__
        if(plr->morphTics > 0)
            info->pClass = PCLASS_PIG;
        else
            info->pClass = plr->class;
#else
        info->pClass = PCLASS_PLAYER;
#endif
        info->team = cfg.playerColor[i];

        // Pick team color:
#if __JHEXEN__
        R_GetColorPaletteRGBf(0, info->color, plrColors[info->team], false);
#else
        switch(info->team)
        {
        case 0: memcpy(info->color, green, sizeof(float)*3); break;
        case 1: memcpy(info->color, gray, sizeof(float)*3); break;
        case 2: memcpy(info->color, brown, sizeof(float)*3); break;
        case 3: memcpy(info->color, red, sizeof(float)*3); break;
        }
#endif

        if(deathmatch)
        {
            for(j = 0; j < maxPlayers; ++j)
            {
                if(j != i)
                {
                    info->kills += plr->frags[j];
                }
                else
                {
#if __JHEXEN__
                    info->suicides += -plr->frags[j];
#else
                    info->suicides += plr->frags[j];
#endif
                }
            }
        }
        else
        {
            info->kills = plr->killCount;
            info->suicides = 0; // We don't care anyway.
        }
    }

    sortScoreInfo(scoreBoard, n);

    return inCount;
}

void HU_ScoreBoardUnHide(int player)
{
    hudstate_t* hud;
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];
    hud->scoreAlpha = 1;
    hud->scoreHideTics = 35;
}

static void drawTable(float x, float ly, float width, float height,
    column_t* columns, scoreinfo_t* scoreBoard, int inCount, float alpha,
    int player)
{
#define CELL_PADDING    (1)

    int i, n, numCols, numStretchCols;
    float cX, cY, fixedWidth, lineHeight, fontScale, fontHeight, fontOffsetY;
    float* colX, *colW;

    if(!columns)
        return;

    if(!(alpha > 0))
        return;

    numStretchCols = 0;
    numCols = 0;
    for(n = 0; columns[n].label; n++)
    {
        numCols++;

        if(columns[n].flags & CF_HIDE)
            continue;

        if(!(columns[n].flags & CF_FIXEDWIDTH))
            numStretchCols++;
    }

    if(!numCols)
        return;

    colX = calloc(1, sizeof(*colX) * numCols);
    colW = calloc(1, sizeof(*colW) * numCols);

    lineHeight = height / (MAXPLAYERS + 1);
    fontHeight = GL_CharHeight('A', GF_FONTA);
    fontScale = (lineHeight - CELL_PADDING * 2) / fontHeight;
    fontOffsetY = 0;
    if(fontScale > 1)
    {
        fontScale = 1;
        fontOffsetY = (lineHeight - CELL_PADDING * 2 - fontHeight) / 2;
    }

    fixedWidth = 0;
    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE)
            continue;

        if(columns[n].flags & CF_FIXEDWIDTH)
        {
            colW[n] = GL_TextWidth(columns[n].label, GF_FONTA) + CELL_PADDING * 2;
            fixedWidth += colW[n];
        }
    }

    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE)
            continue;

        if(!(columns[n].flags & CF_FIXEDWIDTH))
            colW[n] = (width - fixedWidth) / numStretchCols;
    }

    colX[0] = x;
    for(n = 1; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE)
            colX[n] = colX[n-1];
        else
            colX[n] = colX[n-1] + colW[n-1];
    }

    // Draw the table header:
    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE)
            continue;

        cX = colX[n];
        cY = ly + fontOffsetY;

        cY += CELL_PADDING;
        if(columns[n].alignRight)
            cX += colW[n] - CELL_PADDING;
        else
            cX += CELL_PADDING;

        HU_DrawText(columns[n].label, GF_FONTA, cX, cY,
            fontScale, 1.f, 1.f, 1.f, alpha, DTF_ALIGN_TOP|DTF_NO_TYPEIN|(columns[n].alignRight? DTF_ALIGN_RIGHT : 0));
    }
    ly += lineHeight;

    // Draw the table from left to right, top to bottom:
    for(i = 0; i < inCount; ++i, ly += lineHeight)
    {
        scoreinfo_t* info = &scoreBoard[i];
        const char* name = Net_GetPlayerName(info->player);
        char buf[5];

        if(info->player == player)
        {   // Draw a background to make *me* stand out.
            float val = (info->color[0] + info->color[1] + info->color[2]) / 3;

            if(val < .5f)
                val = .2f;
            else
                val = .8f;

            DGL_Disable(DGL_TEXTURING);
            DGL_DrawRect(x, ly, width, lineHeight, val + .2f, val + .2f, val, .5f * alpha);
            DGL_Enable(DGL_TEXTURING);
        }

        // Now draw the fields:
        for(n = 0; n < numCols; ++n)
        {
            if(columns[n].flags & CF_HIDE)
                continue;

            cX = colX[n];
            cY = ly;

/*#if _DEBUG
DGL_Disable(DGL_TEXTURING);
GL_DrawRect(cX + CELL_PADDING, cY + CELL_PADDING,
            colW[n] - CELL_PADDING * 2,
            lineHeight - CELL_PADDING * 2,
            1, 1, 1, .1f * alpha);
DGL_Enable(DGL_TEXTURING);
#endif*/

            cY += CELL_PADDING;
            if(columns[n].alignRight)
                cX += colW[n] - CELL_PADDING;
            else
                cX += CELL_PADDING;

            switch(columns[n].type)
            {
            case 0: // Class icon.
                {
#if __JHERETIC__ || __JHEXEN__
                int spr = 0;
# if __JHERETIC__
                if(info->pClass == PCLASS_CHICKEN)
                    spr = SPR_CHKN;
# else
                switch(info->pClass)
                {
                case PCLASS_FIGHTER: spr = SPR_PLAY; break;
                case PCLASS_CLERIC:  spr = SPR_CLER; break;
                case PCLASS_MAGE:    spr = SPR_MAGE; break;
                case PCLASS_PIG:     spr = SPR_PIGY; break;
                }
# endif
                if(spr)
                {
                    spriteinfo_t sprInfo;
                    int w, h;
                    float scale;

                    R_GetSpriteInfo(spr, 0, &sprInfo);
                    w = sprInfo.width;
                    h = sprInfo.height;

                    if(h > w)
                        scale = (lineHeight - CELL_PADDING * 2) / h;
                    else
                        scale = (colW[n] - CELL_PADDING * 2) / w;

                    w *= scale;
                    h *= scale;

                    // Align to center on both X+Y axes.
                    cX += ((colW[n] - CELL_PADDING * 2) - w) / 2;
                    cY += ((lineHeight - CELL_PADDING * 2) - h) / 2;

                    DGL_SetMaterial(sprInfo.material);

                    drawQuad(cX, cY, w, h, sprInfo.texCoord[0], sprInfo.texCoord[1], 1, 1, 1, alpha);
                }
#endif
                break;
                }

            case 1: // Name.
                HU_DrawText(name, GF_FONTA, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, DTF_ALIGN_TOPLEFT|DTF_NO_TYPEIN);
                break;

            case 2: // #Suicides.
                sprintf(buf, "%4i", info->suicides);
                HU_DrawText(buf, GF_FONTA, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, DTF_ALIGN_TOPLEFT|DTF_NO_TYPEIN);
                break;

            case 3: // #Kills.
                sprintf(buf, "%4i", info->kills);
                HU_DrawText(buf, GF_FONTA, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, DTF_ALIGN_TOPLEFT|DTF_NO_TYPEIN);
                break;
            }
        }
    }

    free(colX);
    free(colW);

#undef CELL_PADDING
}

const char* P_GetGameModeName(void)
{
    static const char* dm = "deathmatch";
    static const char* coop = "cooperative";
    static const char* sp = "singleplayer";

    if(IS_NETGAME)
    {
        if(deathmatch)
            return dm;

        return coop;
    }

    return sp;
}

static void drawMapMetaData(float x, float y, compositefontid_t font, float alpha)
{
    static const char*  unnamed = "unnamed";
    const char* lname = P_GetMapNiceName();

    if(!lname)
        lname = unnamed;

    DGL_Color4f(1, 1, 1, alpha);
    // Map name:
    GL_DrawTextFragment2("map: ", x, y + 16, font);
    GL_DrawTextFragment2(lname, x += GL_TextWidth("map: ", font), y + 16, font);

    x += 8;

    // Game mode:
    GL_DrawTextFragment2("gamemode: ", x += GL_TextWidth(lname, font), y + 16, font);
    GL_DrawTextFragment2(P_GetGameModeName(), x += GL_TextWidth("gamemode: ", font), y + 16, font);
}

/**
 * Draws a sorted frags list in the lower right corner of the screen.
 */
void HU_DrawScoreBoard(int player)
{
#define LINE_BORDER     4

    column_t columns[] = {
        {"cl", 0, CF_FIXEDWIDTH, false},
        {"name", 1, 0, false},
        {"suicides", 2, CF_FIXEDWIDTH, true},
        {"frags", 3, CF_FIXEDWIDTH, true},
        {NULL, 0, 0}
    };

    scoreinfo_t scoreBoard[MAXPLAYERS];
    int x, y, width, height, inCount;
    hudstate_t* hud;

    if(!IS_NETGAME)
        return;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];

    if(!(hud->scoreAlpha > 0))
        return;

    // Set up the fixed 320x200 projection.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);

    // Determine the dimensions of the scoreboard:
    x = 0;
    y = 0;
    width = SCREENWIDTH - 32;
    height = SCREENHEIGHT - 32;

    // Build and sort the scoreboard according to game rules, type, etc.
    inCount = buildScoreBoard(scoreBoard, MAXPLAYERS, player);

    // Only display the player class column if more than one.
    if(NUM_PLAYER_CLASSES == 1)
        columns[0].flags |= CF_HIDE;

    // Scale by HUD scale.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(16, 16, 0);

    // Draw a background around the whole thing.
    DGL_Disable(DGL_TEXTURING);
    DGL_DrawRect(x, y, width, height, 0, 0, 0, .4f * hud->scoreAlpha);
    DGL_Enable(DGL_TEXTURING);

    // Title:
    DGL_Color4f(1, 0, 0, hud->scoreAlpha);
    GL_DrawTextFragment3("ranking", x + width / 2, y + LINE_BORDER, GF_FONTB, DTF_ALIGN_TOP|DTF_NO_EFFECTS);

    drawMapMetaData(x, y + 16, GF_FONTA, hud->scoreAlpha);

    drawTable(x, y + 20, width, height - 20, columns, scoreBoard, inCount, hud->scoreAlpha, player);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Hu_Ticker(void)
{
    int i;

    Hu_LogTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t* hud = &hudStates[i];
        player_t* plr = &players[i];

        if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
            continue;

        if(hud->scoreHideTics > 0)
        {
            hud->scoreHideTics--;
        }
        else
        {
            if(hud->scoreAlpha > 0)
                hud->scoreAlpha -= .05f;
        }
    }
}

/**
 * Updates on Game Tick.
 */
void Hu_FogEffectTicker(timespan_t ticLength)
{
#define fog                 (&fogEffectData)
#define FOGALPHA_FADE_STEP (.07f)

    static const float MENUFOGSPEED[2] = {.03f, -.085f};
    int i;

    if(cfg.hudFog == 0)
        return;

    // Move towards the target alpha
    if(fog->alpha != fog->targetAlpha)
    {
        float diff = fog->targetAlpha - fog->alpha;

        if(fabs(diff) > FOGALPHA_FADE_STEP)
        {
            fog->alpha += FOGALPHA_FADE_STEP * ticLength * TICRATE * (diff > 0? 1 : -1);
        }
        else
        {
            fog->alpha = fog->targetAlpha;
        }
    }

    if(!(fog->alpha > 0))
        return;

    for(i = 0; i < 2; ++i)
    {
        if(cfg.hudFog == 2)
        {
            fog->layers[i].texAngle += ((MENUFOGSPEED[i]/4) * ticLength * TICRATE);
            fog->layers[i].posAngle -= (MENUFOGSPEED[!i]    * ticLength * TICRATE);
            fog->layers[i].texOffset[VX] = 160 + 120 * cos(fog->layers[i].posAngle / 180 * PI);
            fog->layers[i].texOffset[VY] = 100 + 100 * sin(fog->layers[i].posAngle / 180 * PI);
        }
        else
        {
            fog->layers[i].texAngle += ((MENUFOGSPEED[i]/4)     * ticLength * TICRATE);
            fog->layers[i].posAngle -= ((MENUFOGSPEED[!i]*1.5f) * ticLength * TICRATE);
            fog->layers[i].texOffset[VX] = 320 + 320 * cos(fog->layers[i].posAngle / 180 * PI);
            fog->layers[i].texOffset[VY] = 240 + 240 * sin(fog->layers[i].posAngle / 180 * PI);
        }
    }

    // Calculate the height of the menuFog 3 Y join
    if(cfg.hudFog == 4)
    {
        if(fog->scrollDir && fog->joinY > 0.46f)
            fog->joinY = fog->joinY / 1.002f;
        else if(!fog->scrollDir && fog->joinY < 0.54f )
            fog->joinY = fog->joinY * 1.002f;

        if((fog->joinY < 0.46f || fog->joinY > 0.54f))
            fog->scrollDir = !fog->scrollDir;
    }
#undef fog
#undef FOGALPHA_FADE_STEP
}

void M_DrawGlowBar(const float a[2], const float b[2], float thickness,
    boolean left, boolean right, boolean caps, float red, float green,
    float blue, float alpha)
{
    float length, delta[2], normal[2], unit[2];
    DGLuint tex;

    if(!left && !right && !caps)
        return;
    if(!(alpha > 0))
        return;

    delta[0] = b[0] - a[0];
    delta[1] = b[1] - a[1];
    length = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
    if(length <= 0)
        return;

    unit[0] = delta[0] / length;
    unit[1] = delta[1] / length;
    normal[0] = unit[1];
    normal[1] = -unit[0];

    tex = Get(DD_DYNLIGHT_TEXTURE);

    if(caps)
    {   // Draw a "cap" at the start of the line.
        float v1[2], v2[2], v3[2], v4[2];

        v1[0] = a[0] - unit[0] * thickness + normal[0] * thickness;
        v1[1] = a[1] - unit[1] * thickness + normal[1] * thickness;
        v2[0] = a[0] + normal[0] * thickness;
        v2[1] = a[1] + normal[1] * thickness;
        v3[0] = a[0] - normal[0] * thickness;
        v3[1] = a[1] - normal[1] * thickness;
        v4[0] = a[0] - unit[0] * thickness - normal[0] * thickness;
        v4[1] = a[1] - unit[1] * thickness - normal[1] * thickness;

        DGL_Bind(tex);
        DGL_Color4f(red, green, blue, alpha);

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(v1[0], v1[1]);

            DGL_TexCoord2f(0, .5f, 0);
            DGL_Vertex2f(v2[0], v2[1]);

            DGL_TexCoord2f(0, .5f, 1);
            DGL_Vertex2f(v3[0], v3[1]);

            DGL_TexCoord2f(0, 0, 1);
            DGL_Vertex2f(v4[0], v4[1]);
        DGL_End();
    }

    // The middle part of the line.
    if(left && right)
    {
        float v1[2], v2[2], v3[2], v4[2];

        v1[0] = a[0] + normal[0] * thickness;
        v1[1] = a[1] + normal[1] * thickness;
        v2[0] = b[0] + normal[0] * thickness;
        v2[1] = b[1] + normal[1] * thickness;
        v3[0] = b[0] - normal[0] * thickness;
        v3[1] = b[1] - normal[1] * thickness;
        v4[0] = a[0] - normal[0] * thickness;
        v4[1] = a[1] - normal[1] * thickness;

        DGL_Bind(tex);
        DGL_Color4f(red, green, blue, alpha);

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, .5f, 0);
            DGL_Vertex2f(v1[0], v1[1]);

            DGL_TexCoord2f(0, .5f, 0);
            DGL_Vertex2f(v2[0], v2[1]);

            DGL_TexCoord2f(0, .5f, 1);
            DGL_Vertex2f(v3[0], v3[1]);

            DGL_TexCoord2f(0, .5f, 1);
            DGL_Vertex2f(v4[0], v4[1]);
        DGL_End();
    }
    else if(left)
    {
        float v1[2], v2[2], v3[2], v4[2];

        v1[0] = a[0] + normal[0] * thickness;
        v1[1] = a[1] + normal[1] * thickness;
        v2[0] = b[0] + normal[0] * thickness;
        v2[1] = b[1] + normal[1] * thickness;
        v3[0] = b[0];
        v3[1] = b[1];
        v4[0] = a[0];
        v4[1] = a[1];

        DGL_Bind(tex);
        DGL_Color4f(red, green, blue, alpha);

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, .25f);
            DGL_Vertex2f(v1[0], v1[1]);

            DGL_TexCoord2f(0, 0, .25f);
            DGL_Vertex2f(v2[0], v2[1]);

            DGL_TexCoord2f(0, .5f, .25f);
            DGL_Vertex2f(v3[0], v3[1]);

            DGL_TexCoord2f(0, .5f, .25f);
            DGL_Vertex2f(v4[0], v4[1]);
        DGL_End();
    }
    else // right
    {
        float v1[2], v2[2], v3[2], v4[2];

        v1[0] = a[0];
        v1[1] = a[1];
        v2[0] = b[0];
        v2[1] = b[1];
        v3[0] = b[0] - normal[0] * thickness;
        v3[1] = b[1] - normal[1] * thickness;
        v4[0] = a[0] - normal[0] * thickness;
        v4[1] = a[1] - normal[1] * thickness;

        DGL_Bind(tex);
        DGL_Color4f(red, green, blue, alpha);

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, .75f, .5f);
            DGL_Vertex2f(v1[0], v1[1]);

            DGL_TexCoord2f(0, .75f, .5f);
            DGL_Vertex2f(v2[0], v2[1]);

            DGL_TexCoord2f(0, .75f, 1);
            DGL_Vertex2f(v3[0], v3[1]);

            DGL_TexCoord2f(0, .75f, 1);
            DGL_Vertex2f(v4[0], v4[1]);
        DGL_End();
    }

    if(caps)
    {
        float v1[2], v2[2], v3[2], v4[2];

        v1[0] = b[0] + normal[0] * thickness;
        v1[1] = b[1] + normal[1] * thickness;
        v2[0] = b[0] + unit[0] * thickness + normal[0] * thickness;
        v2[1] = b[1] + unit[1] * thickness + normal[1] * thickness;
        v3[0] = b[0] + unit[0] * thickness - normal[0] * thickness;
        v3[1] = b[1] + unit[1] * thickness - normal[1] * thickness;
        v4[0] = b[0] - normal[0] * thickness;
        v4[1] = b[1] - normal[1] * thickness;

        DGL_Bind(tex);
        DGL_Color4f(red, green, blue, alpha);

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, .5f, 0);
            DGL_Vertex2f(v1[0], v1[1]);

            DGL_TexCoord2f(0, 1, 0);
            DGL_Vertex2f(v2[0], v2[1]);

            DGL_TexCoord2f(0, 1, 1);
            DGL_Vertex2f(v3[0], v3[1]);

            DGL_TexCoord2f(0, .5, 1);
            DGL_Vertex2f(v4[0], v4[1]);
        DGL_End();
    }
}

void M_DrawShadowedChar3(unsigned char ch, int x, int y, compositefontid_t font, short flags,
    float r, float g, float b, float a)
{
    DGL_Color4f(0, 0, 0, a * .4f);
    GL_DrawChar3(ch, x+2, y+2, font, flags);

    DGL_Color4f(r, g, b, a);
    GL_DrawChar3(ch, x, y, font, flags);
}

void M_DrawShadowedChar2(unsigned char ch, int x, int y, compositefontid_t font)
{
    M_DrawShadowedChar3(ch, x, y, font, DTF_ALIGN_TOPLEFT|DTF_NO_TYPEIN, 1, 1, 1, 1);
}

void M_DrawShadowedChar(unsigned char ch, int x, int y)
{
    M_DrawShadowedChar2(ch, x, y, GF_FONTA);
}

void M_DrawTextFragmentShadowed(const char* string, int x, int y, compositefontid_t font, short flags, int tracking, float r, float g, float b, float a)
{
    DGL_Color4f(0, 0, 0, a * .4f);
    GL_DrawTextFragment4(string, x+2, y+2, font, flags, tracking);

    DGL_Color4f(r, g, b, a);
    GL_DrawTextFragment4(string, x, y, font, flags, tracking);
}

/**
 * This routine tests for a string-replacement for the patch.
 * If one is found, it's used instead of the original graphic.
 *
 * "{fontb; r=0.5; g=1; b=0; x=2; y=-2}This is good!"
 *
 * If the patch is not in an IWAD, it won't be replaced!
 *
 * @param altstring     String to use instead of the patch if appropriate.
 * @param built-in      (True) if the altstring is a built-in replacement
 *                      (ie it does not originate from a DED definition).
 */
void WI_DrawPatch4(patchid_t patch, int x, int y, const char* altstring,
    compositefontid_t font, boolean builtin, short flags, float r, float g, float b, float a)
{
    int patchString = 0, posx = x;
    char def[80], *string;

    if(IS_DEDICATED)
        return;

    if(patch == -1)
        return;

    if(altstring && altstring[0] && !builtin)
    {   // We have already determined a string to replace this with.
        patchinfo_t info;
        R_GetPatchInfo(patch, &info);
        if(!info.isCustom)
        {
            GL_DrawText(altstring, x, y, font, translatePatchToTextDrawFlags(flags), .5f, 0, r, g, b, a, menu_glitter, menu_shadow, false);
            return;
        }
    }
    else if(cfg.usePatchReplacement)
    {   // We might be able to replace the patch with a string.
        patchinfo_t info;
        R_GetPatchInfo(patch, &info);

        strcpy(def, "Patch Replacement|");
        strcat(def, W_LumpName(patch));

        patchString = Def_Get(DD_DEF_VALUE, def, &string);

        if(!info.isCustom)
        {
            short textFlags = translatePatchToTextDrawFlags(flags);
            /**
             * \kludge Remove the DTF_NO_EFFECTS flag.
             * Correct behavior is off but due to the way the state for
             * this is managed it means the menu strings don't typein
             * and/or shadow when they should. This should be addressed
             * by redesigning the API for patch replacement.
             */
            if(cfg.menuEffects)
                textFlags &= ~DTF_NO_EFFECTS;

            // A user replacement?
            if(patchString)
            {
                GL_DrawText(string, x, y, font, textFlags, .5f, 0, r, g, b, a, menu_glitter, menu_shadow, false);
                return;
            }

            // A built-in replacement?
            if(cfg.usePatchReplacement == 2 && altstring && altstring[0])
            {
                GL_DrawText(altstring, x, y, font, textFlags, .5f, 0, r, g, b, a, menu_glitter, menu_shadow, false);
                return;
            }
        }
    }

    // No replacement possible/wanted - use the original patch.
    DGL_Color4f(1, 1, 1, a);
    GL_DrawPatch2(patch, posx, y, flags);
}

void WI_DrawPatch3(patchid_t id, int x, int y, const char* altstring, compositefontid_t font, boolean builtin, short flags)
{
    WI_DrawPatch4(id, x, y, altstring, font, builtin, flags, 1, 1, 1, 1);
}

void WI_DrawPatch2(patchid_t patch, int x, int y, const char* altstring, compositefontid_t font, boolean builtin)
{
    WI_DrawPatch3(patch, x, y, altstring, font, builtin, DPF_ALIGN_TOPLEFT);
}

void WI_DrawPatch(patchid_t patch, int x, int y)
{
    WI_DrawPatch2(patch, x, y, NULL, GF_FONTB, false);
}

/**
 * Draws a box using the border patches, a border is drawn outside.
 */
void M_DrawBackgroundBox(float x, float y, float w, float h, boolean background,
    int border, float red, float green, float blue, float alpha)
{
    patchinfo_t* t = 0, *b = 0, *l = 0, *r = 0, *tl = 0, *tr = 0, *br = 0, *bl = 0;
    int up = -1;

    switch(border)
    {
    case BORDERUP:
        t = &borderPatches[2];
        b = &borderPatches[0];
        l = &borderPatches[1];
        r = &borderPatches[3];
        tl = &borderPatches[6];
        tr = &borderPatches[7];
        br = &borderPatches[4];
        bl = &borderPatches[5];

        up = -1;
        break;

    case BORDERDOWN:
        t = &borderPatches[0];
        b = &borderPatches[2];
        l = &borderPatches[3];
        r = &borderPatches[1];
        tl = &borderPatches[4];
        tr = &borderPatches[5];
        br = &borderPatches[6];
        bl = &borderPatches[7];

        up = 1;
        break;

    default:
        break;
    }

    DGL_Color4f(red, green, blue, alpha);

    if(background)
    {
        DGL_SetMaterial(P_ToPtr(DMU_MATERIAL, P_MaterialNumForName(borderLumps[0], MN_FLATS)));
        DGL_DrawRectTiled(x, y, w, h, 64, 64);
    }

    if(border)
    {
        // Top
        DGL_SetPatch(t->id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x, y - t->height, w, t->height, up * t->width, up * t->height);
        // Bottom
        DGL_SetPatch(b->id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x, y + h, w, b->height, up * b->width, up * b->height);
        // Left
        DGL_SetPatch(l->id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x - l->width, y, l->width, h, up * l->width, up * l->height);
        // Right
        DGL_SetPatch(r->id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x + w, y, r->width, h, up * r->width, up * r->height);

        // Top Left
        DGL_SetPatch(tl->id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x - tl->width, y - tl->height, tl->width, tl->height, red, green, blue, alpha);
        // Top Right
        DGL_SetPatch(tr->id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x + w, y - tr->height, tr->width, tr->height, red, green, blue, alpha);
        // Bottom Right
        DGL_SetPatch(br->id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x + w, y + h, br->width, br->height, red, green, blue, alpha);
        // Bottom Left
        DGL_SetPatch(bl->id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x - bl->width, y + h, bl->width, bl->height, red, green, blue, alpha);
    }
}

/**
 * Draws a menu slider widget control.
 */
void M_DrawSlider(int x, int y, int height, int range, int slot, float alpha)
{
#define WIDTH           (middleInfo.width)
#define HEIGHT          (middleInfo.height)

    patchinfo_t middleInfo;
    float pos, scale;

    if(range <= 1 || height <= 0)
        return;
    if(!R_GetPatchInfo(dpSliderMiddle, &middleInfo))
        return;
    if(HEIGHT <= 0)
        return;

    pos = (float)slot / (range-1);
    scale = (float) height / HEIGHT;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);

    if(cfg.menuShadow > 0)
    {
        float from[2], to[2];
        from[0] = -3;
        from[1] = 1+HEIGHT/2;
        to[0] = ((range+1) * WIDTH) - 6;
        to[1] = 1+HEIGHT/2;
        M_DrawGlowBar(from, to, HEIGHT*1.1f, true, true, true, 0, 0, 0, alpha * cfg.menuShadow);
    }

    DGL_Color4f( 1, 1, 1, alpha);

    GL_DrawPatch2(dpSliderLeft, 0, 0, DPF_ALIGN_RIGHT|DPF_ALIGN_TOP|DPF_NO_OFFSETX);
    GL_DrawPatch(dpSliderRight, range * WIDTH, 0);

    DGL_SetPatch(dpSliderMiddle, DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectTiled(0, middleInfo.topOffset, range * WIDTH, HEIGHT, middleInfo.width, middleInfo.height);

    DGL_Color4f(1, 1, 1, alpha);
    GL_DrawPatch2(dpSliderHandle, range * WIDTH * pos, 1, DPF_ALIGN_TOP|DPF_NO_OFFSET);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef WIDTH
#undef HEIGHT
}

void Draw_BeginZoom(float s, float originX, float originY)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(originX, originY, 0);
    DGL_Scalef(s, s, 1);
    DGL_Translatef(-originX, -originY, 0);
}

void Draw_EndZoom(void)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Draws a 'fancy' fullscreen fog effect. Used as a background to various
 * HUD displays.
 */
void Hu_DrawFogEffect(int effectID, DGLuint tex, float texOffset[2],
                      float texAngle, float alpha, float arg1)
{
    const float         xscale = 2.0f;
    const float         yscale = 1.0f;

    if(!(alpha > 0))
        return;

    if(effectID == 4)
    {
        DGL_SetNoMaterial();
        DGL_DrawRect(0, 0, 320, 200, 0.0f, 0.0f, 0.0f, MIN_OF(alpha, .5f));
        return;
    }

    if(effectID == 2)
    {
        DGL_Disable(DGL_TEXTURING);
        DGL_Color4f(alpha, alpha / 2, 0, alpha / 3);
        DGL_BlendMode(BM_INVERSE_MUL);
        DGL_DrawRectTiled(0, 0, 320, 200, 1, 1);
        DGL_Enable(DGL_TEXTURING);
    }

    DGL_Bind(tex);
    DGL_Color3f(alpha, alpha, alpha);
    DGL_MatrixMode(DGL_TEXTURE);
    DGL_LoadIdentity();
    DGL_PushMatrix();

    if(effectID == 1)
    {
        DGL_Color3f(alpha / 3, alpha / 2, alpha / 2);
        DGL_BlendMode(BM_INVERSE_MUL);
    }
    else if(effectID == 2)
    {
        DGL_Color3f(alpha / 5, alpha / 3, alpha / 2);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_SRC_ALPHA);
    }
    else if(effectID == 0)
    {
        DGL_Color3f(alpha * 0.15, alpha * 0.2, alpha * 0.3);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_SRC_ALPHA);
    }

    if(effectID == 3)
    {   // The fancy one.
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_SRC_ALPHA);

        DGL_Translatef(texOffset[VX] / 320, texOffset[VY] / 200, 0);
        DGL_Rotatef(texAngle * 1, 0, 0, 1);
        DGL_Translatef(-texOffset[VX] / 320, -texOffset[VY] / 200, 0);

        DGL_Begin(DGL_QUADS);
            // Top Half
            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f(0, 0, 0);
            DGL_Vertex2f(0, 0);

            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f(0, xscale, 0);
            DGL_Vertex2f(320, 0);

            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f(0, xscale, yscale * arg1);
            DGL_Vertex2f(320, 200 * arg1);

            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f(0, 0, yscale * arg1);
            DGL_Vertex2f(0, 200 * arg1);

            // Bottom Half
            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f(0, 0, yscale * arg1);
            DGL_Vertex2f(0, 200 * arg1);

            DGL_Color4f(alpha * 0.7, alpha * 0.7, alpha * 0.8, 1 - (0-(alpha * 0.9)));
            DGL_TexCoord2f(0, xscale, yscale * arg1);
            DGL_Vertex2f(320, 200 * arg1);

            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f(0, xscale, yscale);
            DGL_Vertex2f(320, 200);

            DGL_Color4f(alpha * 0.25, alpha * 0.3, alpha * 0.4, 1 - (alpha * 0.8) );
            DGL_TexCoord2f(0, 0, yscale);
            DGL_Vertex2f(0, 200);
        DGL_End();
    }
    else
    {
        DGL_Translatef(texOffset[VX] / 320, texOffset[VY] / 200, 0);
        DGL_Rotatef(texAngle * (effectID == 0 ? 0.5 : 1), 0, 0, 1);
        DGL_Translatef(-texOffset[VX] / 320, -texOffset[VY] / 200, 0);
        if(effectID == 2)
            DGL_DrawRectTiled(0, 0, 320, 200, 270 / 8, 4 * 225);
        else if(effectID == 0)
            DGL_DrawRectTiled(0, 0, 320, 200, 270 / 4, 8 * 225);
        else
            DGL_DrawRectTiled(0, 0, 320, 200, 270, 225);
    }

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();

    DGL_BlendMode(BM_NORMAL);
}

static void drawFogEffect(void)
{
#define mfd                 (&fogEffectData)

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    // Two layers.
    Hu_DrawFogEffect(cfg.hudFog - 1, mfd->texture,
                     mfd->layers[0].texOffset, mfd->layers[0].texAngle,
                     mfd->alpha, fogEffectData.joinY);
    Hu_DrawFogEffect(cfg.hudFog - 1, mfd->texture,
                     mfd->layers[1].texOffset, mfd->layers[1].texAngle,
                     mfd->alpha, fogEffectData.joinY);

    // Restore original matrices.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

#undef mfd
}

/**
 * Decide our scaling strategy by comparing the aspect ratios of the
 * window dimensions to the original fixed-size window.
 *
 * @return              @c true if decided to stretch, else scale to fit.
 */
static boolean __inline pickScalingStrategy(int winWidth, int winHeight)
{
    float a = (float)winWidth/winHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;

    if(INRANGE_OF(a, b, .001f))
        return true; // The same, so stretch.
    if(cfg.menuNoStretch || !INRANGE_OF(a, b, .38f))
        return false; // No stretch; translate and scale to fit.
    // Otherwise stretch.
    return true;
}

void Hu_Drawer(void)
{
    boolean menuOrMessageVisible = (Hu_MenuIsActive() || Hu_MenuAlpha() > 0) || Hu_IsMessageActive();
    boolean pauseGraphicVisible = paused && !fiActive;
    int winWidth, winHeight;

    if(!menuOrMessageVisible && !pauseGraphicVisible)
        return;

    winWidth = Get(DD_WINDOW_WIDTH);
    winHeight = Get(DD_WINDOW_HEIGHT);

    if(pauseGraphicVisible)
    {
        /**
         * Use an orthographic projection in native screenspace. Then
         * translate and scale the projection to produce an aspect
         * corrected coordinate space at 4:3, aligned vertically to
         * the top and centered horizontally in the window.
         */
        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PushMatrix();
        DGL_LoadIdentity();
        DGL_Ortho(0, 0, winWidth, winHeight, -1, 1);

        DGL_Translatef((float)winWidth/2, (float)winHeight/SCREENHEIGHT * 4, 0);
        if(winWidth >= winHeight)
            DGL_Scalef((float)winHeight/SCREENHEIGHT, (float)winHeight/SCREENHEIGHT, 1);
        else
            DGL_Scalef((float)winWidth/SCREENWIDTH, (float)winWidth/SCREENWIDTH, 1);

        WI_DrawPatch3(m_pause, 0, 0, NULL, GF_FONTB, false, DPF_ALIGN_TOP|DPF_NO_OFFSET);

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PopMatrix();
    }

    if(!menuOrMessageVisible)
        return;

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();

    if(pickScalingStrategy(winWidth, winHeight))
    {
        // Use an orthographic projection in a fixed 320x200 space.
        DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);
    }
    else
    {
        /**
         * Use an orthographic projection in native screenspace. Then
         * translate and scale the projection to produce an aspect
         * corrected coordinate space of 320x200 and centered on the
         * larger of the horizontal and vertical axes.
         */
        DGL_Ortho(0, 0, winWidth, winHeight, -1, 1);

        if(winWidth >= winHeight)
        {
            DGL_Translatef((float)winWidth/2, 0, 0);
            DGL_Scalef(1/1.2f, 1, 1); // Aspect correction.
            DGL_Scalef((float)winHeight/SCREENHEIGHT, (float)winHeight/SCREENHEIGHT, 1);
            DGL_Translatef(-(SCREENWIDTH/2), 0, 0);
        }
        else
        {
            DGL_Translatef(0, (float)winHeight/2, 0);
            DGL_Scalef(1, 1.2f, 1); // Aspect correction.
            DGL_Scalef((float)winWidth/SCREENWIDTH, (float)winWidth/SCREENWIDTH, 1);
            DGL_Translatef(0, -(SCREENHEIGHT/2), 0);
        }
    }

    // Draw the fog effect?
    if(fogEffectData.alpha > 0 && cfg.hudFog)
        drawFogEffect();

    if(Hu_IsMessageActive())
        Hu_MsgDrawer();
    else
        Hu_MenuDrawer();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}

void Hu_FogEffectSetAlphaTarget(float alpha)
{
    fogEffectData.targetAlpha = MINMAX_OF(0, alpha, 1);
}

static void drawMapTitle(void)
{
    const char* lname, *lauthor;
    float y = 0, alpha = 1;
#if __JDOOM__ || __JDOOM64__
    int mapnum;
#endif

    if(actualMapTime < 35)
        alpha = actualMapTime / 35.0f;
    if(actualMapTime > 5 * 35)
        alpha = 1 - (actualMapTime - 5 * 35) / 35.0f;

    // Get the strings from Doomsday.
    lname = P_GetMapNiceName();
    lauthor = P_GetMapAuthor(cfg.hideIWADAuthor);
#if __JHEXEN__
    // Use stardard map name if DED didn't define it.
    if(!lname)
        lname = P_GetMapName(gameMap);
#endif

#if __JDOOM__ || __JDOOM64__
    // Compose the mapnumber used to check the map name patches array.
# if __JDOOM__
    if(gameMode == commercial)
        mapnum = gameMap;
    else
        mapnum = (gameEpisode * 9) + gameMap;
# else // __JDOOM64__
    mapnum = gameMap;
# endif

    WI_DrawPatch4(mapNamePatches[mapnum], 0, 0, lname, GF_FONTB, false, DPF_ALIGN_TOP, 1, 1, 1, alpha);
    y += 14;

#elif __JHERETIC__ || __JHEXEN__
    if(lname)
    {
        DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], alpha);
        GL_DrawTextFragment3(lname, 0, 0, GF_FONTB, DTF_ALIGN_TOP|DTF_NO_TYPEIN);
        y += 20;
    }
#endif

    if(lauthor)
    {
        DGL_Color4f(.5f, .5f, .5f, alpha);
        GL_DrawTextFragment3(lauthor, 0, y, GF_FONTA, DTF_ALIGN_TOP|DTF_NO_TYPEIN);
    }
}

void Hu_DrawMapTitle(int x, int y, float scale)
{
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);

    drawMapTitle();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

enum {
    STRETCH = 0,
    PILLARBOX,
    LETTERBOX,
};

static int pickBorderedScalingStrategy(int winWidth, int winHeight, float* outScale)
{
    float scale = (winWidth >= winHeight? (float)winHeight/SCREENHEIGHT : (float)winWidth/SCREENWIDTH);
    float a = (float)winWidth/winHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;
    int displayMode = STRETCH;

    if(!INRANGE_OF(a, b, .001f) && (cfg.fiNoStretch || !INRANGE_OF(a, b, .38f)))
    {
        if(SCREENWIDTH * scale > winWidth || SCREENHEIGHT * scale < winHeight)
        {
            scale *= (float)winWidth/(SCREENWIDTH*scale);
            displayMode = LETTERBOX;
        }
        else if(SCREENWIDTH * scale < winWidth)
        {
            displayMode = PILLARBOX;
        }
    }
    if(outScale)
        *outScale = scale;
    return displayMode;
}

void Hu_ConfigureBorderedProjection(borderedprojectionstate_t* s)
{
    s->winWidth = Get(DD_WINDOW_WIDTH);
    s->winHeight = Get(DD_WINDOW_HEIGHT);
    s->displayMode = pickBorderedScalingStrategy(s->winWidth, s->winHeight, &s->scale);
    memset(s->scissorState, 0, sizeof(s->scissorState));
}

void Hu_BeginBorderedProjection(borderedprojectionstate_t* s)
{
    assert(s);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    switch(s->displayMode)
    {
    case PILLARBOX:
        {
        /**
         * Use an orthographic projection in native screenspace. Then
         * translate and scale the projection to produce an aspect
         * corrected coordinate space at 4:3, centered horizontally
         * in the window.
         */
        int w = (s->winWidth-SCREENWIDTH*s->scale)/2;
        DGL_Ortho(0, 0, s->winWidth, s->winHeight, -1, 1);

        DGL_GetIntegerv(DGL_SCISSOR_TEST, s->scissorState);
        DGL_GetIntegerv(DGL_SCISSOR_BOX, s->scissorState + 1);
        DGL_Scissor(w, 0, SCREENWIDTH*s->scale, s->winHeight);
        DGL_Enable(DGL_SCISSOR_TEST);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef((float)s->winWidth/2, (float)s->winHeight/2, 0);
        DGL_Scalef(s->scale, s->scale, 1);
        DGL_Translatef(-SCREENWIDTH/2, -SCREENHEIGHT/2, 0);
        break;
        }
    case LETTERBOX:
        {
        /**
         * Use an orthographic projection in native screenspace. Then
         * translate and scale the projection to produce an aspect
         * corrected coordinate space at 4:3, centered vertically in
         * the window.
         */
        int h = (s->winHeight-SCREENHEIGHT*s->scale)/2;
        DGL_Ortho(0, 0, s->winWidth, s->winHeight, -1, 1);

        DGL_GetIntegerv(DGL_SCISSOR_TEST, s->scissorState);
        DGL_GetIntegerv(DGL_SCISSOR_BOX, s->scissorState + 1);
        DGL_Scissor(0, h, s->winWidth, SCREENHEIGHT*s->scale);
        DGL_Enable(DGL_SCISSOR_TEST);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef((float)s->winWidth/2, (float)s->winHeight/2, 0);
        DGL_Scalef(s->scale, s->scale, 1);
        DGL_Translatef(-SCREENWIDTH/2, -SCREENHEIGHT/2, 0);
        }
        break;

    default: // STRETCH
        DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);
        break;
    }
}

void Hu_EndBorderedProjection(borderedprojectionstate_t* s)
{
    if(s->displayMode != STRETCH)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();

        if(!s->scissorState[0])
            DGL_Disable(DGL_SCISSOR_TEST);
        DGL_Scissor(s->scissorState[1], s->scissorState[2], s->scissorState[3], s->scissorState[4]);

        switch(s->displayMode)
        {
        case PILLARBOX:
            {
            int w = (s->winWidth-SCREENWIDTH*s->scale)/2;

            DGL_SetNoMaterial();
            DGL_DrawRect(0, 0, w, s->winHeight, 0, 0, 0, 1);
            DGL_DrawRect(s->winWidth - w, 0, w, s->winHeight, 0, 0, 0, 1);
            break;
            }
        case LETTERBOX:
            {
            int h = (s->winHeight-SCREENHEIGHT*s->scale)/2;

            DGL_SetNoMaterial();
            DGL_DrawRect(0, 0, s->winWidth, h, 0, 0, 0, 1);
            DGL_DrawRect(0, s->winHeight - h, s->winWidth, h, 0, 0, 0, 1);
            }
            break;

        default: // STRETCH
            break;
        }
    }
    
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}

void M_DrawShadowedPatch3(patchid_t id, int x, int y, short flags, float r, float g,
    float b, float a)
{
    if(id < 0)
        return;
    DGL_Color4f(0, 0, 0, a * .4f);
    GL_DrawPatch2(id, x+2, y+2, flags);

    DGL_Color4f(r, g, b, a);
    GL_DrawPatch2(id, x, y, flags);
}

void M_DrawShadowedPatch2(patchid_t id, int x, int y, short flags)
{
    M_DrawShadowedPatch3(id, x, y, flags, 1, 1, 1, 1);
}

void M_DrawShadowedPatch(patchid_t id, int x, int y)
{
    M_DrawShadowedPatch2(id, x, y, DPF_ALIGN_TOPLEFT);
}
