/**\file hu_stuff.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Heads-up displays, font handling, text drawing routines.
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

#include "hu_chat.h"
#include "hu_log.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_stuff.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "am_map.h"
#include "fi_lib.h"
#include "r_common.h"

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

fontid_t fonts[NUM_GAME_FONTS];

#if __JDOOM__ || __JDOOM64__
// Name graphics of each map.
patchid_t* pMapNames = NULL;
#endif

#if __JDOOM__ || __JDOOM64__
/// The end message strings.
char* endmsg[NUM_QUITMESSAGES + 1];
#endif

#if __JHERETIC__ || __JHEXEN__
patchid_t pInvItemBox;
patchid_t pInvSelectBox;
patchid_t pInvPageLeft[2];
patchid_t pInvPageRight[2];
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
static patchid_t m_pause; // Paused graphic.

// CODE -------------------------------------------------------------------

static short translatePatchToTextDrawFlags(short in)
{
    short out = 0;
    if(in & DPF_ALIGN_LEFT)
        out |= DTF_ALIGN_LEFT;
    if(in & DPF_ALIGN_RIGHT)
        out |= DTF_ALIGN_RIGHT;
    if(in & DPF_ALIGN_BOTTOM)
        out |= DTF_ALIGN_BOTTOM;
    if(in & DPF_ALIGN_TOP)
        out |= DTF_ALIGN_TOP;
    /**
     * \kludge
     * Correct behavior is no-merge but due to the way the state for this is
     * managed it means the menu strings do not have text effects applied
     * when they should. This should be addressed by redesigning the API for
     * patch replacement.
     */
    return MN_MergeMenuEffectWithDrawTextFlags(out);
}

/**
 * Loads the font patches and inits various strings
 * JHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void Hu_LoadData(void)
{
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
        lumpnum_t lumpNum = W_GetLumpNumForName("menufog");
        if(lumpNum >= 0)
        {
            const uint8_t* pixels = (const uint8_t*) W_CacheLump(lumpNum, PU_GAMESTATIC);
            int width = 64, height = 64;

            fogEffectData.texture = DGL_NewTextureWithParams(DGL_LUMINANCE, width, height,
                pixels, 0, DGL_NEAREST, DGL_LINEAR, -1 /*best anisotropy*/, DGL_REPEAT, DGL_REPEAT);

            W_CacheChangeTag(lumpNum, PU_CACHE);
        }
    }

    // Load the border patches
    for(i = 1; i < 9; ++i)
        R_PrecachePatch(borderGraphics[i], &borderPatches[i-1]);

#if __JDOOM__ || __JDOOM64__
    m_pause = R_PrecachePatch("M_PAUSE", NULL);
#elif __JHERETIC__ || __JHEXEN__
    m_pause = R_PrecachePatch("PAUSED", NULL);
#endif

#if __JDOOM__ || __JDOOM64__
    // Load the map name patches.
# if __JDOOM64__
    {
        int NUMCMAPS = 32;
        pMapNames = Z_Malloc(sizeof(patchid_t) * NUMCMAPS, PU_GAMESTATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "WILV%2.2d", i);
            pMapNames[i] = R_PrecachePatch(name, NULL);
        }
    }
# else
    if(gameModeBits & GM_ANY_DOOM2)
    {
        int NUMCMAPS = 32;
        pMapNames = Z_Malloc(sizeof(patchid_t) * NUMCMAPS, PU_GAMESTATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "CWILV%2.2d", i);
            pMapNames[i] = R_PrecachePatch(name, NULL);
        }
    }
    else
    {
        int j;

        // Don't waste space - patches are loaded back to back
        // ie no space in the array is left for E1M10
        pMapNames = Z_Malloc(sizeof(patchid_t) * (9*4), PU_GAMESTATIC, 0);
        for(i = 0; i < 4; ++i) // Number of episodes.
        {
            for(j = 0; j < 9; ++j) // Number of maps per episode.
            {
                sprintf(name, "WILV%2.2d", (i * 10) + j);
                pMapNames[(i* 9) + j] = R_PrecachePatch(name, NULL);
            }
        }
    }
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    pInvItemBox = R_PrecachePatch("ARTIBOX", NULL);
    pInvSelectBox = R_PrecachePatch("SELECTBO", NULL);
    pInvPageLeft[0] = R_PrecachePatch("INVGEML1", NULL);
    pInvPageLeft[1] = R_PrecachePatch("INVGEML2", NULL);
    pInvPageRight[0] = R_PrecachePatch("INVGEMR1", NULL);
    pInvPageRight[1] = R_PrecachePatch("INVGEMR2", NULL);
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    R_GetGammaMessageStrings();
#endif

#if __JDOOM__ || __JDOOM64__
    // Quit messages.
    endmsg[0] = GET_TXT(TXT_QUITMSG);
    { int i;
    for(i = 1; i <= NUM_QUITMESSAGES; ++i)
        endmsg[i] = GET_TXT(TXT_QUITMESSAGE1 + i - 1);
    }
#endif
}

void Hu_UnloadData(void)
{
#if __JDOOM__ || __JDOOM64__
    if(pMapNames)
        Z_Free(pMapNames);
    pMapNames = 0;
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

void HU_DrawText(const char* str, float x, float y, float scale,
    float r, float g, float b, float a, short flags)
{
    if(!str || !str[0])
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);
    DGL_Translatef(-x, -y, 0);

    DGL_Color4f(r, g, b, a);
    FR_DrawTextFragment2(str, x, y, flags);

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
        R_GetColorPaletteRGBf(0, plrColors[info->team], info->color, false);
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

    FR_SetFont(FID(GF_FONTA));
    lineHeight = height / (MAXPLAYERS + 1);
    fontHeight = FR_CharHeight('A');
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
            colW[n] = FR_TextFragmentWidth(columns[n].label) + CELL_PADDING * 2;
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
    DGL_Enable(DGL_TEXTURE_2D);
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

        FR_SetFont(FID(GF_FONTA));
        HU_DrawText(columns[n].label, cX, cY, fontScale, 1.f, 1.f, 1.f, alpha,
            DTF_ALIGN_TOP|DTF_ONLY_SHADOW|(columns[n].alignRight? DTF_ALIGN_RIGHT : 0));
    }
    ly += lineHeight;
    DGL_Disable(DGL_TEXTURE_2D);

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

            DGL_DrawRect(x, ly, width, lineHeight, val + .2f, val + .2f, val, .5f * alpha);
        }

        // Now draw the fields:
        DGL_Enable(DGL_TEXTURE_2D);

        for(n = 0; n < numCols; ++n)
        {
            if(columns[n].flags & CF_HIDE)
                continue;

            cX = colX[n];
            cY = ly;

/*#if _DEBUG
DGL_Disable(DGL_TEXTURE_2D);
GL_DrawRect(cX + CELL_PADDING, cY + CELL_PADDING,
            colW[n] - CELL_PADDING * 2,
            lineHeight - CELL_PADDING * 2,
            1, 1, 1, .1f * alpha);
DGL_Enable(DGL_TEXTURE_2D);
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

                    DGL_SetMaterialUI(sprInfo.material);

                    drawQuad(cX, cY, w, h, sprInfo.texCoord[0], sprInfo.texCoord[1], 1, 1, 1, alpha);
                }
#endif
                break;
                }

            case 1: // Name.
                FR_SetFont(FID(GF_FONTA));
                HU_DrawText(name, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, DTF_ALIGN_TOPLEFT|DTF_ONLY_SHADOW);
                break;

            case 2: // #Suicides.
                sprintf(buf, "%4i", info->suicides);
                FR_SetFont(FID(GF_FONTA));
                HU_DrawText(buf, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, DTF_ALIGN_TOPLEFT|DTF_ONLY_SHADOW);
                break;

            case 3: // #Kills.
                sprintf(buf, "%4i", info->kills);
                FR_SetFont(FID(GF_FONTA));
                HU_DrawText(buf, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, DTF_ALIGN_TOPLEFT|DTF_ONLY_SHADOW);
                break;
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
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

static void drawMapMetaData(float x, float y, int fontIdx, float alpha)
{
    static const char*  unnamed = "unnamed";
    const char* lname = P_GetMapNiceName();

    if(!lname)
        lname = unnamed;

    FR_SetFont(FID(fontIdx));
    DGL_Color4f(1, 1, 1, alpha);

    // Map name:
    FR_DrawTextFragment("map: ", x, y + 16);
    FR_DrawTextFragment(lname, x += FR_TextFragmentWidth("map: "), y + 16);

    x += 8;

    // Game mode:
    FR_DrawTextFragment("gamemode: ", x += FR_TextFragmentWidth(lname), y + 16);
    FR_DrawTextFragment(P_GetGameModeName(), x += FR_TextFragmentWidth("gamemode: "), y + 16);
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
    DGL_DrawRect(x, y, width, height, 0, 0, 0, .4f * hud->scoreAlpha);

    DGL_Enable(DGL_TEXTURE_2D);

    // Title:
    FR_SetFont(FID(GF_FONTB));
    DGL_Color4f(1, 0, 0, hud->scoreAlpha);
    FR_DrawTextFragment2("ranking", x + width / 2, y + LINE_BORDER, DTF_ALIGN_TOP|DTF_ONLY_SHADOW);

    drawMapMetaData(x, y + 16, GF_FONTA, hud->scoreAlpha);

    drawTable(x, y + 20, width, height - 20, columns, scoreBoard, inCount, hud->scoreAlpha, player);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Hu_Ticker(void)
{
    int i;

    // Do not tick the message logs if the game is paused.
    if(!P_IsPaused())
    {
        Hu_LogTicker();
    }

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

void M_DrawTextFragmentShadowed(const char* string, int x, int y, short flags, int tracking, float r, float g, float b, float a)
{
    DGL_Color4f(0, 0, 0, a * .4f);
    FR_DrawTextFragment3(string, x+2, y+2, flags, tracking);

    DGL_Color4f(r, g, b, a);
    FR_DrawTextFragment3(string, x, y, flags, tracking);
}

const char* Hu_FindPatchReplacementString(patchid_t patchId, int flags)
{
    const ddstring_t* name = R_GetPatchName(patchId);
    ddstring_t valueStr;
    char* replacement;
    int result;

    if(name == NULL) // Invalid id?
    {
        return NULL;
    }

    Str_Init(&valueStr); 
    Str_Appendf(&valueStr, "Patch Replacement|%s", Str_Text(name));
    result = Def_Get(DD_DEF_VALUE, Str_Text(&valueStr), (void*)&replacement);
    Str_Free(&valueStr);
    if(result == 0) // Not found.
    {
        return NULL;
    }

    if(flags & (PRF_NO_IWAD|PRF_NO_PWAD))
    {
        patchinfo_t info;
        R_GetPatchInfo(patchId, &info);
        if(!info.isCustom)
        {
            if(flags & PRF_NO_IWAD)
                return NULL;
        }
        else
        {
            if(flags & PRF_NO_PWAD)
                return NULL;
        }
    }

    return replacement;
}

const char* Hu_ChoosePatchReplacement2(patchid_t patchId, const char* altString,
    boolean builtin)
{
    const char* replacement = NULL; // No replacement possible/wanted.

    if(altString && altString[0] && !builtin)
    {   // We have already determined a string to replace this with.
        patchinfo_t info;
        R_GetPatchInfo(patchId, &info);
        if(!info.isCustom)
        {
            replacement = altString;
        }
    }
    else if(cfg.usePatchReplacement)
    {   // We might be able to replace the patch with a string replacement.
        // A user replacement?
        replacement = Hu_FindPatchReplacementString(patchId, PRF_NO_PWAD);
        if(NULL == replacement)
        {
            // Perhaps a built-in replacement?
            patchinfo_t info;
            if(cfg.usePatchReplacement == 2 && altString && altString[0] &&
               R_GetPatchInfo(patchId, &info) && !info.isCustom)
            {
                replacement = altString;
            }
        }
    }

    return replacement;
}

const char* Hu_ChoosePatchReplacement(patchid_t patchId)
{
    return Hu_ChoosePatchReplacement2(patchId, NULL, false);
}

void WI_DrawPatch5(patchid_t patchId, const char* replacement, int x, int y, short flags,
    fontid_t fontId, float r, float g, float b, float a, float glitter, float shadow)
{
    if(patchId == 0)
        return;

    if(NULL != replacement && replacement[0])
    {
        // Use the replacement string.
        short textFlags = translatePatchToTextDrawFlags(flags);
        FR_DrawText(replacement, x, y, fontId, textFlags, .5f, 0, r, g, b, a, glitter, shadow, false);
        return;
    }

    // Use the original patch.
    DGL_Color4f(1, 1, 1, a);
    GL_DrawPatch2(patchId, x, y, flags);
}

void WI_DrawPatch4(patchid_t patchId, const char* replacement, int x, int y, short flags,
    fontid_t fontId, float r, float g, float b, float a)
{
    WI_DrawPatch5(patchId, replacement, x, y, flags, fontId, r, g, b, a, 0, 0);
}

void WI_DrawPatch3(patchid_t patchId, const char* replacement, int x, int y, short flags, fontid_t fontId)
{
    WI_DrawPatch4(patchId, replacement, x, y, flags, fontId, 1, 1, 1, 1);
}

void WI_DrawPatch2(patchid_t patchId, const char* replacement, int x, int y, short flags)
{
    WI_DrawPatch3(patchId, replacement, x, y, flags, FID(GF_FONTB));
}

void WI_DrawPatch(patchid_t patchId, const char* replacement, int x, int y)
{
    WI_DrawPatch2(patchId, replacement, x, y, DPF_ALIGN_TOPLEFT);
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
        DGL_SetMaterialUI(P_ToPtr(DMU_MATERIAL, Materials_IndexForName(borderGraphics[0])));
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
        DGL_Color4f(alpha, alpha / 2, 0, alpha / 3);
        DGL_BlendMode(BM_INVERSE_MUL);
        DGL_DrawRectTiled(0, 0, 320, 200, 1, 1);
    }

    DGL_Bind(tex);
    if(tex)
        DGL_Enable(DGL_TEXTURE_2D);

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

    if(tex)
        DGL_Disable(DGL_TEXTURE_2D);
    DGL_BlendMode(BM_NORMAL);
}

static void drawFogEffect(void)
{
#define mfd                 (&fogEffectData)

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

#undef mfd
}

void Hu_Drawer(void)
{
    boolean menuOrMessageVisible = (Hu_MenuIsVisible() || Hu_IsMessageActive());
    boolean pauseGraphicVisible = paused && !FI_StackActive();

    if(!menuOrMessageVisible && !pauseGraphicVisible)
        return;

    if(pauseGraphicVisible)
    {
        const int winWidth  = Get(DD_WINDOW_WIDTH);
        const int winHeight = Get(DD_WINDOW_HEIGHT);

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

        DGL_Enable(DGL_TEXTURE_2D);

        WI_DrawPatch3(m_pause, Hu_ChoosePatchReplacement(m_pause), 0, 0, DPF_ALIGN_TOP|DPF_NO_OFFSET, FID(GF_FONTB));

        DGL_Disable(DGL_TEXTURE_2D);

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PopMatrix();
    }

    if(!menuOrMessageVisible)
        return;

    // Draw the fog effect?
    if(fogEffectData.alpha > 0 && cfg.hudFog)
        drawFogEffect();

    if(Hu_IsMessageActive())
    {
        Hu_MsgDrawer();
    }
    else
    {
        Hu_MenuDrawer();
    }
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
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        mapnum = gameMap;
    else
        mapnum = (gameEpisode * 9) + gameMap;
# else // __JDOOM64__
    mapnum = gameMap;
# endif

    DGL_Enable(DGL_TEXTURE_2D);

    WI_DrawPatch4(pMapNames[mapnum], Hu_ChoosePatchReplacement2(pMapNames[mapnum], lname, false), 0, 0, DPF_ALIGN_TOP, FID(GF_FONTB), 1, 1, 1, alpha);

    DGL_Disable(DGL_TEXTURE_2D);

    y += 14;
#elif __JHERETIC__ || __JHEXEN__
    if(lname)
    {
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTB));
        DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], alpha);
        FR_DrawTextFragment2(lname, 0, 0, DTF_ALIGN_TOP|DTF_ONLY_SHADOW);

        DGL_Disable(DGL_TEXTURE_2D);
        y += 20;
    }
#endif

    if(lauthor)
    {
        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(FID(GF_FONTA));
        DGL_Color4f(.5f, .5f, .5f, alpha);
        FR_DrawTextFragment2(lauthor, 0, y, DTF_ALIGN_TOP|DTF_ONLY_SHADOW);

        DGL_Disable(DGL_TEXTURE_2D);
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
