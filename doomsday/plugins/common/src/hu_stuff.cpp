/**
 * @file hu_stuff.cpp
 * Miscellaneous routines for heads-up displays and UI.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 1993-1996 by id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

#include "common.h"

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

typedef struct {
    int             hideTics;
    float           alpha;
} scoreboardstate_t;

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

fontid_t fonts[NUM_GAME_FONTS];

#if __JDOOM__ || __JDOOM64__
// Name graphics of each map.
patchid_t* pMapNames;
uint pMapNamesSize;
#endif

#if __JDOOM__ || __JDOOM64__
/// The end message strings.
const char* endmsg[NUM_QUITMESSAGES + 1];
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

patchid_t borderPatches[8];

static scoreboardstate_t scoreStates[MAXPLAYERS];
static fogeffectdata_t fogEffectData;

static patchid_t m_pause; // Paused graphic.

typedef std::map<patchid_t, int> PatchReplacementValues;
PatchReplacementValues patchReplacements;

static int patchReplacementValueIndex(patchid_t patchId, bool canCreate = true)
{
    // Have we previously encountered this?
    PatchReplacementValues::const_iterator found = patchReplacements.find(patchId);
    if(found != patchReplacements.end()) return found->second;

    // No. Look it up.
    int valueIndex = -1;
    AutoStr* patchPath = R_ComposePatchPath(patchId);
    if(!Str_IsEmpty(patchPath))
    {
        AutoStr* valueStr = Str_Appendf(AutoStr_New(), "Patch Replacement|%s", Str_Text(patchPath));
        valueIndex = Def_Get(DD_DEF_VALUE, Str_Text(valueStr), 0);
    }

    if(canCreate)
    {
        patchReplacements.insert(std::pair<patchid_t, int>(patchId, valueIndex));
    }

    return valueIndex;
}

/**
 * Loads the font patches and inits various strings
 * JHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void Hu_LoadData(void)
{
    // Clear the patch replacement value map (definitions have been re-read).
    patchReplacements.clear();

#if __JDOOM__ || __JDOOM64__
    char name[9];
#endif
    uint i;

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
    if(!fogEffectData.texture && !(Get(DD_NOVIDEO) || Get(DD_DEDICATED)))
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
        borderPatches[i-1] = R_DeclarePatch(borderGraphics[i]);

#if __JDOOM__ || __JDOOM64__
    m_pause = R_DeclarePatch("M_PAUSE");
#elif __JHERETIC__ || __JHEXEN__
    m_pause = R_DeclarePatch("PAUSED");
#endif

#if __JDOOM__ || __JDOOM64__
    // Load the map name patches.
# if __JDOOM64__
    {
        pMapNamesSize = 32;
        pMapNames = (patchid_t*) Z_Malloc(sizeof(patchid_t) * pMapNamesSize, PU_GAMESTATIC, 0);
        for(i = 0; i < pMapNamesSize; ++i)
        {
            sprintf(name, "WILV%2.2u", i);
            pMapNames[i] = R_DeclarePatch(name);
        }
    }
# else
    if(gameModeBits & GM_ANY_DOOM2)
    {
        pMapNamesSize = 32;
        pMapNames = (patchid_t*) Z_Malloc(sizeof(patchid_t) * pMapNamesSize, PU_GAMESTATIC, 0);
        for(i = 0; i < pMapNamesSize; ++i)
        {
            sprintf(name, "CWILV%2.2u", i);
            pMapNames[i] = R_DeclarePatch(name);
        }
    }
    else
    {
        uint numEpisodes = (gameMode == doom_shareware? 1 : gameMode == doom_ultimate? 4 : 3);
        uint j;

        // Don't waste space - patches are loaded back to back
        // ie no space in the array is left for E1M10
        pMapNamesSize = 9*4;
        pMapNames = (patchid_t*) Z_Malloc(sizeof(patchid_t) * pMapNamesSize, PU_GAMESTATIC, 0);
        for(i = 0; i < numEpisodes; ++i)
        {
            for(j = 0; j < 9; ++j) // Number of maps per episode.
            {
                sprintf(name, "WILV%2.2u", (i * 10) + j);
                pMapNames[(i* 9) + j] = R_DeclarePatch(name);
            }
        }
    }
# endif
#endif

#if __JHERETIC__ || __JHEXEN__
    pInvItemBox = R_DeclarePatch("ARTIBOX");
    pInvSelectBox = R_DeclarePatch("SELECTBO");
    pInvPageLeft[0] = R_DeclarePatch("INVGEML1");
    pInvPageLeft[1] = R_DeclarePatch("INVGEML2");
    pInvPageRight[0] = R_DeclarePatch("INVGEMR1");
    pInvPageRight[1] = R_DeclarePatch("INVGEMR2");
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    R_GetGammaMessageStrings();
#endif

#if __JDOOM__ || __JDOOM64__
    // Quit messages.
    endmsg[0] = GET_TXT(TXT_QUITMSG);
    for(i = 1; i <= NUM_QUITMESSAGES; ++i)
    {
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

void HU_WakeWidgets(int player)
{
    if(player < 0)
    {
        uint i;
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame) continue;
            HU_WakeWidgets(i);
        }
        return;
    }
    if(player < MAXPLAYERS)
    {
        if(!players[player].plr->inGame) return;
        ST_Start(player);
    }
}

#if __JHERETIC__ || __JHEXEN__
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
#endif

void HU_DrawText(const char* str, float x, float y, float scale,
    float r, float g, float b, float a, int alignFlags, short textFlags)
{
    const boolean applyScale = !FEQUAL(scale, 1.0f);

    if(!str || !str[0]) return;

    if(applyScale)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        DGL_Translatef(x, y, 0);
        DGL_Scalef(scale, scale, 1);
        DGL_Translatef(-x, -y, 0);
    }

    FR_SetColorAndAlpha(r, g, b, a);
    FR_DrawTextXY3(str, x, y, alignFlags, textFlags);

    if(applyScale)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
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
    DENG_UNUSED(player);

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
            info->pClass = plr->class_;
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
    scoreboardstate_t* ss;
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!plr->plr->inGame)
        return;

    ss = &scoreStates[player];
    ss->alpha = 1;
    ss->hideTics = 35;
}

static void drawTable(float x, float ly, float width, float height,
    column_t* columns, scoreinfo_t* scoreBoard, int inCount, float alpha,
    int player)
{
#define CELL_PADDING    (1)

    int i, n, numCols, numStretchCols;
    int cX, cY, fixedWidth, lineHeight, fontHeight, fontOffsetY;
    float fontScale, *colX, *colW;

    if(!columns) return;
    if(!(alpha > 0)) return;

    numStretchCols = 0;
    numCols = 0;
    for(n = 0; columns[n].label; n++)
    {
        numCols++;

        if(columns[n].flags & CF_HIDE) continue;

        if(!(columns[n].flags & CF_FIXEDWIDTH))
            numStretchCols++;
    }

    if(!numCols) return;

    /// @todo - Preprocess the table to avoid constant allocations.
    colX = (float*) calloc(1, sizeof(*colX) * numCols);
    if(!colX) Con_Error("drawTable: Failed on allocation of %lu bytes for colX data table.", (unsigned long) (sizeof(*colX) * numCols));

    colW = (float*) calloc(1, sizeof(*colW) * numCols);
    if(!colW) Con_Error("drawTable: Failed on allocation of %lu bytes for colW data table.", (unsigned long) (sizeof(*colW) * numCols));

    lineHeight = .5f + height / (MAXPLAYERS + 1);
    fontHeight = FR_CharHeight('A');
    fontScale = (float)lineHeight / (fontHeight + CELL_PADDING * 2);
    fontOffsetY = 0;

    fixedWidth = 0;
    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE) continue;

        if(columns[n].flags & CF_FIXEDWIDTH)
        {
            colW[n] = FR_TextWidth(columns[n].label) + CELL_PADDING * 2;
            fixedWidth += colW[n];
        }
    }

    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE) continue;

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
        if(columns[n].flags & CF_HIDE) continue;

        cX = colX[n];
        cY = ly + lineHeight - CELL_PADDING + fontOffsetY;

        if(columns[n].alignRight)
            cX += colW[n] - CELL_PADDING;
        else
            cX += CELL_PADDING;

        HU_DrawText(columns[n].label, cX, cY, fontScale, 1.f, 1.f, 1.f, alpha,
                    ALIGN_BOTTOM|(columns[n].alignRight? ALIGN_RIGHT : ALIGN_LEFT), DTF_ONLY_SHADOW);
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
        {
            // Draw a background to make *me* stand out.
            float val = (info->color[0] + info->color[1] + info->color[2]) / 3;

            if(val < .5f) val = .2f;
            else          val = .8f;

            DGL_DrawRectf2Color(x, ly, width, lineHeight, val + .2f, val + .2f, val, .5f * alpha);
        }

        // Now draw the fields:
        DGL_Enable(DGL_TEXTURE_2D);

        for(n = 0; n < numCols; ++n)
        {
            if(columns[n].flags & CF_HIDE) continue;

            cX = colX[n];
            cY = ly;

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
                    w = sprInfo.geometry.size.width;
                    h = sprInfo.geometry.size.height;

                    if(h > w)
                        scale = (float) (lineHeight - CELL_PADDING * 2) / h;
                    else
                        scale = (float) (colW[n] - CELL_PADDING * 2) / w;

                    w *= scale;
                    h *= scale;

                    // Align to center on both X+Y axes.
                    cX += ((colW[n] - CELL_PADDING * 2) - w) / 2.0f;
                    cY += ((lineHeight - CELL_PADDING * 2) - h) / 2.0f;

                    DGL_SetMaterialUI(sprInfo.material, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

                    drawQuad(cX, cY, w, h, sprInfo.texCoord[0], sprInfo.texCoord[1], 1, 1, 1, alpha);
                }
#endif
                break;
                }

            case 1: // Name.
                HU_DrawText(name, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, ALIGN_TOP|(columns[n].alignRight? ALIGN_RIGHT : ALIGN_LEFT),
                            DTF_ONLY_SHADOW);
                break;

            case 2: // #Suicides.
                sprintf(buf, "%4i", info->suicides);
                HU_DrawText(buf, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, ALIGN_TOP|(columns[n].alignRight? ALIGN_RIGHT : ALIGN_LEFT),
                            DTF_ONLY_SHADOW);
                break;

            case 3: // #Kills.
                sprintf(buf, "%4i", info->kills);
                HU_DrawText(buf, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, ALIGN_TOP|(columns[n].alignRight? ALIGN_RIGHT : ALIGN_LEFT),
                            DTF_ONLY_SHADOW);
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
        if(deathmatch) return dm;
        return coop;
    }
    return sp;
}

static void drawMapMetaData(float x, float y, float alpha)
{
#define BORDER              2

    static const char* unnamed = "Unnamed";

    const char* lname = P_GetMapNiceName();
    char buf[256];

    if(!lname)
        lname = unnamed;

    dd_snprintf(buf, 256, "map: %s gamemode: %s", lname, P_GetGameModeName());

    FR_SetColorAndAlpha(1, 1, 1, alpha);
    FR_DrawTextXY2(buf, x + BORDER, y - BORDER, ALIGN_BOTTOMLEFT);

#undef BORDER
}

/**
 * Draws a sorted frags list in the lower right corner of the screen.
 */
void HU_DrawScoreBoard(int player)
{
#define LINE_BORDER         4

    column_t columns[] = {
        { "cl",       0, CF_FIXEDWIDTH, false },
        { "name",     1, 0,             false },
        { "suicides", 2, CF_FIXEDWIDTH, true  },
        { "frags",    3, CF_FIXEDWIDTH, true  },
        { NULL,       0, 0,             false }
    };

    scoreinfo_t scoreBoard[MAXPLAYERS];
    int x, y, width, height, inCount;
    scoreboardstate_t* ss;

    if(!IS_NETGAME) return;

    if(player < 0 || player >= MAXPLAYERS) return;
    ss = &scoreStates[player];

    if(!(ss->alpha > 0)) return;

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
    DGL_PushMatrix();
    DGL_Translatef(16, 16, 0);

    // Draw a background around the whole thing.
    DGL_DrawRectf2Color(x, y, width, height, 0, 0, 0, .4f * ss->alpha);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetColorAndAlpha(1, 0, 0, ss->alpha);
    FR_DrawTextXY3("ranking", x + width / 2, y + LINE_BORDER, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    drawMapMetaData(x, y + height, ss->alpha);
    drawTable(x, y + 20, width, height - 20, columns, scoreBoard, inCount, ss->alpha, player);

    DGL_Disable(DGL_TEXTURE_2D);

    // Restore earlier matrices.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Hu_Ticker(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        scoreboardstate_t* ss = &scoreStates[i];
        player_t* plr = &players[i];

        if(!plr->plr->inGame)
            continue;

        if(ss->hideTics > 0)
        {
            --ss->hideTics;
        }
        else
        {
            if(ss->alpha > 0)
                ss->alpha -= .05f;
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

void M_DrawTextFragmentShadowed(const char* string, int x, int y, int alignFlags,
    short textFlags, float r, float g, float b, float a)
{
    FR_SetColorAndAlpha(0, 0, 0, a * .4f);
    FR_DrawTextXY3(string, x+2, y+2, alignFlags, textFlags);

    FR_SetColorAndAlpha(r, g, b, a);
    FR_DrawTextXY3(string, x, y, alignFlags, textFlags);
}

static const char* patchReplacement(patchid_t patchId)
{
    char* replacement = 0; // Not found.
    int valueIndex = patchReplacementValueIndex(patchId);
    if(valueIndex >= 0)
    {
        if(Def_Get(DD_DEF_VALUE_BY_INDEX, (char*)&valueIndex, (void*)&replacement) < 0)
            Con_Error("Hu_FindPatchReplacementString: Failed retrieving text value #%i.", valueIndex);
    }
    return replacement;
}

const char* Hu_FindPatchReplacementString(patchid_t patchId, int flags)
{
    const char* replacement = patchReplacement(patchId);
    if(flags & (PRF_NO_IWAD|PRF_NO_PWAD))
    {
        patchinfo_t info;
        R_GetPatchInfo(patchId, &info);
        if(!info.flags.isCustom)
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

const char* Hu_ChoosePatchReplacement2(patchreplacemode_t mode, patchid_t patchId, const char* text)
{
    const char* replacement = NULL; // No replacement possible/wanted.
    if(mode != PRM_NONE)
    {
        // We might be able to replace the patch with a string replacement.
        if(patchId != 0)
        {
            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);
            if(!info.flags.isCustom)
            {
                if(NULL == text || !text[0])
                {
                    // Look for a user replacement.
                    text = Hu_FindPatchReplacementString(patchId, PRF_NO_PWAD);
                }

                replacement = text;
            }
        }
        else
        {
            replacement = text;
        }
    }
    return replacement;
}

const char* Hu_ChoosePatchReplacement(patchreplacemode_t mode, patchid_t patchId)
{
    return Hu_ChoosePatchReplacement2(mode, patchId, NULL);
}

void WI_DrawPatch3(patchid_t patchId, const char* replacement, const Point2Raw* origin,
    int alignFlags, int patchFlags, short textFlags)
{
    if(replacement && replacement[0])
    {
        // Use the replacement string.
        FR_DrawText3(replacement, origin, alignFlags, textFlags);
        return;
    }
    // Use the original patch.
    GL_DrawPatch3(patchId, origin, alignFlags, patchFlags);
}

void WI_DrawPatch2(patchid_t patchId, const char* replacement, const Point2Raw* origin, int alignFlags)
{
    WI_DrawPatch3(patchId, replacement, origin, alignFlags, 0, 0);
}

void WI_DrawPatch(patchid_t patchId, const char* replacement, const Point2Raw* origin)
{
    WI_DrawPatch2(patchId, replacement, origin, ALIGN_TOPLEFT);
}

void WI_DrawPatchXY3(patchid_t patchId, const char* replacement, int x, int y, int alignFlags,
    int patchFlags, short textFlags)
{
    Point2Raw origin;
    origin.x = x;
    origin.y = y;
    WI_DrawPatch3(patchId, replacement, &origin, alignFlags, patchFlags, textFlags);
}

void WI_DrawPatchXY2(patchid_t patchId, const char* replacement, int x, int y, int alignFlags)
{
    WI_DrawPatchXY3(patchId, replacement, x, y, alignFlags, 0, 0);
}

void WI_DrawPatchXY(patchid_t patchId, const char* replacement, int x, int y)
{
    WI_DrawPatchXY2(patchId, replacement, x, y, ALIGN_TOPLEFT);
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
        DGL_DrawRectf2Color(0, 0, 320, 200, 0.0f, 0.0f, 0.0f, MIN_OF(alpha, .5f));
        return;
    }

    if(effectID == 2)
    {
        DGL_Color4f(alpha, alpha / 2, 0, alpha / 3);
        DGL_BlendMode(BM_INVERSE_MUL);
        DGL_DrawRectf2Tiled(0, 0, 320, 200, 1, 1);
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
            DGL_DrawRectf2Tiled(0, 0, 320, 200, 270 / 8, 4 * 225);
        else if(effectID == 0)
            DGL_DrawRectf2Tiled(0, 0, 320, 200, 270 / 4, 8 * 225);
        else
            DGL_DrawRectf2Tiled(0, 0, 320, 200, 270, 225);
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
        DGL_Color4f(1, 1, 1, 1);
        FR_SetFont(FID(GF_FONTB));
        FR_LoadDefaultAttrib();
        FR_SetLeading(0);

        WI_DrawPatchXY3(m_pause, Hu_ChoosePatchReplacement(PRM_ALLOW_TEXT, m_pause), 0, 0, ALIGN_TOP, DPF_NO_OFFSET, 0);

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

void Hu_DrawMapTitle(const Point2Raw* offset)
{
    const char* lname, *lauthor;
    float y = 0, alpha = 1;
#if __JDOOM__ || __JDOOM64__
    patchid_t patchId;
    uint mapNum;
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

    if(offset)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(offset->x, offset->y, 0);
    }

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, alpha);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], alpha);

#if __JDOOM__ || __JDOOM64__
    // Compose the mapnumber used to check the map name patches array.
# if __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        mapNum = gameMap;
    else
        mapNum = (gameEpisode * 9) + gameMap;
# else // __JDOOM64__
    mapNum = gameMap;
# endif
    patchId = (mapNum < pMapNamesSize? pMapNames[mapNum] : 0);
    WI_DrawPatchXY3(patchId, Hu_ChoosePatchReplacement2(PRM_ALLOW_TEXT, patchId, lname), 0, 0, ALIGN_TOP, 0, DTF_ONLY_SHADOW);

    y += 14;
#elif __JHERETIC__ || __JHEXEN__
    if(lname)
    {
        FR_DrawTextXY3(lname, 0, 0, ALIGN_TOP, DTF_ONLY_SHADOW);
        y += 20;
    }
#endif

    if(lauthor)
    {
        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(.5f, .5f, .5f, alpha);
        FR_DrawTextXY3(lauthor, 0, y, ALIGN_TOP, DTF_ONLY_SHADOW);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    if(offset)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-offset->x, -offset->y, 0);
    }
}

void Hu_MapTitleDrawer(const RectRaw* portGeometry)
{
    Point2Raw origin(SCREENWIDTH/2, 6);
    float scale;

    if(!portGeometry) return;

    // Level information is shown for a few seconds in the beginning of a level.
    if(!cfg.mapTitle || (actualMapTime > 6 * TICSPERSEC)) return;

    R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT,
                                    portGeometry->size.width, portGeometry->size.height,
                                    scalemode_t(cfg.menuScaleMode));

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(scale, scale * 1.2f/*aspect correct*/, 1);
    DGL_Translatef(origin.x, origin.y, 0);

    // Make the title 3/4 smaller.
    DGL_Scalef(.75f, .75f, 1);

    Hu_DrawMapTitle(NULL/*no offset*/);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void M_DrawShadowedPatch3(patchid_t id, int x, int y, int alignFlags, int patchFlags,
    float r, float g, float b, float a)
{
    if(id == 0 || DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED))
        return;

    DGL_Color4f(0, 0, 0, a * .4f);
    GL_DrawPatchXY3(id, x+2, y+2, alignFlags, patchFlags);

    DGL_Color4f(r, g, b, a);
    GL_DrawPatchXY3(id, x, y, alignFlags, patchFlags);
}

void M_DrawShadowedPatch2(patchid_t id, int x, int y, int alignFlags, int patchFlags)
{
    M_DrawShadowedPatch3(id, x, y, alignFlags, patchFlags, 1, 1, 1, 1);
}

void M_DrawShadowedPatch(patchid_t id, int x, int y)
{
    M_DrawShadowedPatch2(id, x, y, ALIGN_TOPLEFT, 0);
}
