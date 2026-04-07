/** @file hu_stuff.cpp  Miscellaneous routines for heads-up displays and UI.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include "common.h"
#include "hu_stuff.h"

#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <de/string.h>
#include "fi_lib.h"
#include "g_common.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_inventory.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hud/automapstyle.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "r_common.h"

using namespace de;
using namespace common;

/**
 * @defgroup tableColumnFlags  Table Column flags
 */
///@{
#define CF_HIDE                 0x0001 ///< Column is presently hidden (will be collapsed).
#define CF_STRETCH_WIDTH        0x0002 ///< Column width is not fixed.
///@}

/**
 * Defines a column in a table layout.
 */
typedef struct {
    const char*     label;
    int             type;
    int             flags; ///< @ref tableColumnFlags
    int             alignFlags; ///< @ref alignFlags

    /// Auto-initialized:
    float           x;
    float           width;
} column_t;

/**
 * Player "score" information, kills, suicides, etc...
 */
typedef struct {
    int             player, pClass, team;
    int             kills, suicides;
    float           color[3];
} scoreinfo_t;

/**
 * Per-player scoreboard state.
 */
typedef struct {
    int             hideTics;
    float           alpha;
} scoreboardstate_t;

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
    dd_bool         scrollDir;
} fogeffectdata_t;

fontid_t fonts[NUM_GAME_FONTS];

#if __JDOOM__ || __JDOOM64__
/// The end message strings.
const char* endmsg[NUM_QUITMESSAGES + 1];
#endif

dd_bool shiftdown = false;
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

// Misc UI patches:
patchid_t borderPatches[8];
#if __JHERETIC__ || __JHEXEN__
patchid_t pInvItemBox;
patchid_t pInvSelectBox;
patchid_t pInvPageLeft[2];
patchid_t pInvPageRight[2];
#endif
static patchid_t m_pause; // Paused graphic.

// Menu and message background fog effect state.
static fogeffectdata_t fogEffectData;

// Scoreboard states for each player.
static scoreboardstate_t scoreStates[MAXPLAYERS];

// Patch => Text Replacement LUT.
typedef std::map<patchid_t, int> PatchReplacementValues;
PatchReplacementValues patchReplacements;

static int patchReplacementValueIndex(patchid_t patchId, bool canCreate = true)
{
    // Have we previously encountered this?
    PatchReplacementValues::const_iterator found = patchReplacements.find(patchId);
    if(found != patchReplacements.end()) return found->second;

    // No. Look it up.
    dint valueIndex = -1;
    const auto patchPath = String(Str_Text(R_ComposePatchPath(patchId)));
    if(!patchPath.isEmpty())
    {
        valueIndex = Defs().getValueNum("Patch Replacement|" + patchPath);
    }

    if(canCreate)
    {
        patchReplacements.insert(std::pair<patchid_t, dint>(patchId, valueIndex));
    }

    return valueIndex;
}

/**
 * Intialize the background fog effect.
 */
static void initFogEffect()
{
    fogeffectdata_t *fog = &fogEffectData;

    fog->texture = 0;
    fog->alpha = fog->targetAlpha = 0;
    fog->joinY = 0.5f;
    fog->scrollDir = true;

    fog->layers[0].texOffset[VX] = fog->layers[0].texOffset[VY] = 0;
    fog->layers[0].texAngle = 93;
    fog->layers[0].posAngle = 35;

    fog->layers[1].texOffset[VX] = fog->layers[1].texOffset[VY] = 0;
    fog->layers[1].texAngle = 12;
    fog->layers[1].posAngle = 77;
}

static void prepareFogTexture()
{
    if(Get(DD_NOVIDEO)) return;
    // Already prepared?
    if(fogEffectData.texture) return;

    if(CentralLumpIndex().contains("menufog.lmp"))
    {
        res::File1 &lump       = CentralLumpIndex()[CentralLumpIndex().findLast("menufog.lmp")];
        const uint8_t *pixels = lump.cache();
        /// @todo fixme: Do not assume dimensions.
        fogEffectData.texture = DGL_NewTextureWithParams(DGL_LUMINANCE, 64, 64,
            pixels, 0, DGL_NEAREST, DGL_LINEAR, -1 /*best anisotropy*/, DGL_REPEAT, DGL_REPEAT);

        lump.unlock();
    }
}

static void releaseFogTexture()
{
    if(Get(DD_NOVIDEO)) return;
    // Not prepared?
    if(!fogEffectData.texture) return;

    DGL_DeleteTextures(1, (DGLuint *) &fogEffectData.texture);
    fogEffectData.texture = 0;
}

static void declareGraphicPatches()
{
    // View border patches:
    for(uint i = 1; i < 9; ++i)
    {
        borderPatches[i-1] = R_DeclarePatch(borderGraphics[i]);
    }

#if __JDOOM__ || __JDOOM64__
    m_pause = R_DeclarePatch("M_PAUSE");
#elif __JHERETIC__ || __JHEXEN__
    m_pause = R_DeclarePatch("PAUSED");
#endif

#if __JHERETIC__ || __JHEXEN__
    pInvItemBox      = R_DeclarePatch("ARTIBOX");
    pInvSelectBox    = R_DeclarePatch("SELECTBO");
    pInvPageLeft[0]  = R_DeclarePatch("INVGEML1");
    pInvPageLeft[1]  = R_DeclarePatch("INVGEML2");
    pInvPageRight[0] = R_DeclarePatch("INVGEMR1");
    pInvPageRight[1] = R_DeclarePatch("INVGEMR2");
#endif
}

/**
 * Loads the font patches and inits various strings
 * JHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void Hu_LoadData()
{
    // Clear the patch replacement value map (definitions have been re-read).
    patchReplacements.clear();

    initFogEffect();
    prepareFogTexture();

    declareGraphicPatches();

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    R_GetGammaMessageStrings();
#endif

#if __JDOOM__ || __JDOOM64__
    // Quit messages:
    endmsg[0] = GET_TXT(TXT_QUITMSG);
    for(uint i = 1; i <= NUM_QUITMESSAGES; ++i)
    {
        endmsg[i] = GET_TXT(TXT_QUITMESSAGE1 + i - 1);
    }
#endif
}

void Hu_UnloadData()
{
    releaseFogTexture();
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
    if(!str || !str[0]) return;

    const bool applyScale = !FEQUAL(scale, 1.0f);
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

/// Predicate for sorting score infos.
static int scoreInfoCompare(const void *a_, const void *b_)
{
    const scoreinfo_t *a = (scoreinfo_t *) a_;
    const scoreinfo_t *b = (scoreinfo_t *) b_;

    if(a->kills > b->kills) return -1;
    if(b->kills > a->kills) return 1;

    if(gfw_Rule(deathmatch))
    {
        // In deathmatch, suicides affect your place on the scoreboard.
        if(a->suicides < b->suicides) return -1;
        if(b->suicides < a->suicides) return 1;
    }

    return 0;
}

static void sortScoreInfo(scoreinfo_t* vec, size_t size)
{
    qsort(vec, size, sizeof(scoreinfo_t), scoreInfoCompare);
}

static int populateScoreInfo(scoreinfo_t* scoreBoard, int maxPlayers, int player)
{
    DE_UNUSED(player);

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
    int n = 0, inCount;

    memset(scoreBoard, 0, sizeof(*scoreBoard) * maxPlayers);
    inCount = 0;
    for(int i = 0; i < maxPlayers; ++i)
    {
        player_t* plr = &players[i];
        scoreinfo_t* info;

        if(!plr->plr->inGame) continue;

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

        if(gfw_Rule(deathmatch))
        {
            for(int j = 0; j < maxPlayers; ++j)
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
    if(player < 0 || player >= MAXPLAYERS) return;

    player_t* plr = &players[player];
    if(!plr->plr->inGame) return;

    scoreboardstate_t* ss = &scoreStates[player];
    ss->alpha = 1;
    ss->hideTics = 35;
}

static int countTableColumns(const column_t* columns)
{
    int count = 0;
    while(columns[count].label){ count++; }
    return count;
}

static void applyTableLayout(column_t* columns, int numCols, float x, float width,
                             int cellPadding = 0)
{
    DE_ASSERT(columns);
    column_t* col;

    col = columns;
    int numStretchCols = 0;
    for(int n = 0; n < numCols; ++n, col++)
    {
        col->x     = 0;
        col->width = 0;

        // Count the total number of non-fixed width ("stretched") columns.
        if(!(col->flags & CF_HIDE) && (col->flags & CF_STRETCH_WIDTH))
            numStretchCols++;
    }

    col = columns;
    int fixedWidth = 0;
    for(int n = 0; n < numCols; ++n, col++)
    {
        col->width = 0;
        if(col->flags & CF_HIDE) continue; // Collapse.

        if(!(col->flags & CF_STRETCH_WIDTH))
        {
            col->width = FR_TextWidth(col->label) + cellPadding * 2;
            fixedWidth += col->width;
        }
    }

    col = columns;
    for(int n = 0; n < numCols; ++n, col++)
    {
        if(col->flags & CF_HIDE) continue; // Collapse.

        if(col->flags & CF_STRETCH_WIDTH)
        {
            col->width = (width - fixedWidth) / numStretchCols;
        }
    }

    col = columns;
    for(int n = 0; n < numCols; ++n, col++)
    {
        col->x = x;
        if(!(col->flags & CF_HIDE)) // Collapse.
        {
            x += col->width;
        }
    }
}

static void drawTable(float x, float ly, float width, float height,
    column_t* columns, scoreinfo_t* scoreBoard, int inCount, float alpha,
    int player)
{
#define CELL_PADDING            1

    if(!columns) return;
    if(!(alpha > 0)) return;

    int numCols = countTableColumns(columns);
    if(!numCols) return;

    const int lineHeight  = .5f + height / (MAXPLAYERS + 1);
    const int fontHeight  = FR_CharHeight('A');
    float fontScale = float(lineHeight) / (fontHeight + CELL_PADDING * 2);

#ifdef __JHEXEN__
    // Quick fix for Hexen's oversized scoreboard fonts.
    // Something is broken here, though: the above should calculate a
    // suitable font size for all games? -jk
    fontScale *= .55f;
#endif

    applyTableLayout(columns, numCols, x, width, CELL_PADDING);

    /// @todo fixme: all layout/placement should be done in applyTableLayout()

    // Draw the table header:
    DGL_Enable(DGL_TEXTURE_2D);
    const column_t* col = columns;
    for(int n = 0; n < numCols; ++n, col++)
    {
        if(col->flags & CF_HIDE) continue;

        int cX = col->x;
        if(col->alignFlags & ALIGN_RIGHT) cX += col->width - CELL_PADDING;
        else                              cX += CELL_PADDING;

        int cY = ly + lineHeight - CELL_PADDING;
        int alignFlags = (col->alignFlags & ~ALIGN_TOP) | ALIGN_BOTTOM;

        HU_DrawText(col->label, cX, cY, fontScale, 1.f, 1.f, 1.f, alpha, alignFlags, DTF_ONLY_SHADOW);
    }
    ly += lineHeight;
    DGL_Disable(DGL_TEXTURE_2D);

    // Draw the table from left to right, top to bottom:
    for(int i = 0; i < inCount; ++i, ly += lineHeight)
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
        const column_t* col = columns;
        for(int n = 0; n < numCols; ++n, col++)
        {
            if(col->flags & CF_HIDE) continue;

            int cX = col->x;
            if(col->alignFlags & ALIGN_RIGHT) cX += col->width - CELL_PADDING;
            else                              cX += CELL_PADDING;

            int cY = ly + CELL_PADDING;

            switch(col->type)
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

                    if(h > w) scale = float(lineHeight - CELL_PADDING * 2) / h;
                    else      scale = float(col->width - CELL_PADDING * 2) / w;

                    w *= scale;
                    h *= scale;

                    // Align to center on both X+Y axes.
                    cX += ((col->width - CELL_PADDING * 2) - w) / 2.0f;
                    cY += ((lineHeight - CELL_PADDING * 2) - h) / 2.0f;

                    DGL_SetMaterialUI(sprInfo.material, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

                    drawQuad(cX, cY, w, h, sprInfo.texCoord[0], sprInfo.texCoord[1], 1, 1, 1, alpha);
                }
#endif
                break;
                }

            case 1: // Name.
                HU_DrawText(name, cX, cY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, col->alignFlags, DTF_ONLY_SHADOW);
                break;

            case 2: // #Suicides.
                sprintf(buf, "%4i", info->suicides);
                HU_DrawText(buf, cX, cY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, col->alignFlags, DTF_ONLY_SHADOW);
                break;

            case 3: // #Kills.
                sprintf(buf, "%4i", info->kills);
                HU_DrawText(buf, cX, cY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, col->alignFlags, DTF_ONLY_SHADOW);
                break;
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
    }

#undef CELL_PADDING
}

static void drawMapMetaData(float x, float y, float alpha)
{
#define BORDER              2

    de::String title = G_MapTitle(gfw_Session()->mapUri());
    if(title.isEmpty()) title = "Unnamed";

    char buf[256];
    dd_snprintf(buf, 256, "%s - %s", gfw_Session()->rules().description().toLatin1().constData(),
                                     title.toLatin1().constData());

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

    static column_t columns[] = {
        { "cl",       0, 0,                 0,              0, 0 },
        { "name",     1, CF_STRETCH_WIDTH,  ALIGN_TOPLEFT,  0, 0 },
        { "suicides", 2, 0,                 ALIGN_TOPRIGHT, 0, 0 },
        { "frags",    3, 0,                 ALIGN_TOPRIGHT, 0, 0 },
        { NULL,       0, 0,                 0,              0, 0 }
    };
    static bool tableInitialized = false;

    if(!IS_NETGAME) return;

    if(player < 0 || player >= MAXPLAYERS) return;
    scoreboardstate_t* ss = &scoreStates[player];

    if(!(ss->alpha > 0)) return;

    // Are we yet to initialize the table for this game mode?
    if(!tableInitialized)
    {
        // Only display the player class column if more than one class is defined.
        if(NUM_PLAYER_CLASSES == 1)
            columns[0].flags |= CF_HIDE;
        tableInitialized = true;
    }

    // Set up the fixed 320x200 projection.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);

    // Populate and sort the scoreboard according to game rules, type, etc.
    scoreinfo_t scoreBoard[MAXPLAYERS];
    int inCount = populateScoreInfo(scoreBoard, MAXPLAYERS, player);

    // Determine the dimensions of the scoreboard:
    RectRaw geom = {{0, 0}, {SCREENWIDTH - 32, SCREENHEIGHT - 32}};

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(16, 16, 0);

    // Draw a background around the whole thing.
    DGL_DrawRectf2Color(geom.origin.x, geom.origin.y, geom.size.width, geom.size.height,
                        0, 0, 0, .4f * ss->alpha);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetColorAndAlpha(1, 0, 0, ss->alpha);
    FR_DrawTextXY3("ranking", geom.origin.x + geom.size.width / 2, geom.origin.y + LINE_BORDER, ALIGN_TOP, DTF_ONLY_SHADOW);

    FR_SetFont(FID(GF_FONTA));
    drawMapMetaData(geom.origin.x, geom.origin.y + geom.size.height, ss->alpha);
    drawTable(geom.origin.x, geom.origin.y + 20, geom.size.width, geom.size.height - 20,
              columns, scoreBoard, inCount, ss->alpha, player);

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

        if(!plr->plr->inGame) continue;

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

    if(cfg.common.hudFog == 0)
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
        if(cfg.common.hudFog == 2)
        {
            fog->layers[i].texAngle += ((MENUFOGSPEED[i]/4) * ticLength * TICRATE);
            fog->layers[i].posAngle -= (MENUFOGSPEED[!i]    * ticLength * TICRATE);
            fog->layers[i].texOffset[VX] = 160 + 120 * cos(fog->layers[i].posAngle / 180 * DD_PI);
            fog->layers[i].texOffset[VY] = 100 + 100 * sin(fog->layers[i].posAngle / 180 * DD_PI);
        }
        else
        {
            fog->layers[i].texAngle += ((MENUFOGSPEED[i]/4)     * ticLength * TICRATE);
            fog->layers[i].posAngle -= ((MENUFOGSPEED[!i]*1.5f) * ticLength * TICRATE);
            fog->layers[i].texOffset[VX] = 320 + 320 * cos(fog->layers[i].posAngle / 180 * DD_PI);
            fog->layers[i].texOffset[VY] = 240 + 240 * sin(fog->layers[i].posAngle / 180 * DD_PI);
        }
    }

    // Calculate the height of the menuFog 3 Y join
    if(cfg.common.hudFog == 4)
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
    dd_bool left, dd_bool right, dd_bool caps, float red, float green,
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

static const char *patchReplacement(patchid_t patchId)
{
    dint idx = patchReplacementValueIndex(patchId);
    if(idx == -1) return nullptr;
    if(idx >= 0 && idx < Defs().values.size()) return Defs().values[idx].text;
    throw Error("Hu_FindPatchReplacementString", "Failed retrieving text value #" + String::asText(idx));
}

const char *Hu_FindPatchReplacementString(patchid_t patchId, int flags)
{
    const char *replacement = patchReplacement(patchId);
    if(flags & (PRF_NO_IWAD | PRF_NO_PWAD))
    {
        patchinfo_t info;
        R_GetPatchInfo(patchId, &info);
        if(!info.flags.isCustom)
        {
            if(flags & PRF_NO_IWAD)
                return nullptr;
        }
        else
        {
            if(flags & PRF_NO_PWAD)
                return nullptr;
        }
    }
    return replacement;
}

de::String Hu_ChoosePatchReplacement(patchreplacemode_t mode, patchid_t patchId, const de::String &text)
{
    if(mode != PRM_NONE)
    {
        // We might be able to replace the patch with a string replacement.
        if(patchId != 0)
        {
            patchinfo_t info;
            R_GetPatchInfo(patchId, &info);
            if(!info.flags.isCustom)
            {
                if(text.isEmpty())
                {
                    // Look for a user replacement.
                    return de::String(Hu_FindPatchReplacementString(patchId, PRF_NO_PWAD));
                }

                return text;
            }
        }
        else
        {
            return text;
        }
    }

    return ""; // No replacement available/wanted.
}

void WI_DrawPatch(patchid_t patchId, const de::String &replacement, const de::Vec2i &origin,
    int alignFlags, int patchFlags, short textFlags)
{
    if(!replacement.isEmpty())
    {
        // Use the replacement string.
        const Point2Raw originAsPoint2Raw = {{{origin.x, origin.y}}};
        FR_DrawText3(replacement, &originAsPoint2Raw, alignFlags, textFlags);
        return;
    }
    // Use the original patch.
    GL_DrawPatch(patchId, origin, alignFlags, patchFlags);
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
    Hu_DrawFogEffect(cfg.common.hudFog - 1, mfd->texture,
                     mfd->layers[0].texOffset, mfd->layers[0].texAngle,
                     mfd->alpha, fogEffectData.joinY);
    Hu_DrawFogEffect(cfg.common.hudFog - 1, mfd->texture,
                     mfd->layers[1].texOffset, mfd->layers[1].texAngle,
                     mfd->alpha, fogEffectData.joinY);

    // Restore original matrices.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef mfd
}

void Hu_Drawer()
{
    const bool menuOrMessageVisible = (Hu_MenuIsVisible() || Hu_IsMessageActive());
    const bool pauseGraphicVisible = Pause_IsUserPaused() && !FI_StackActive();

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

        WI_DrawPatch(m_pause, Hu_ChoosePatchReplacement(PRM_ALLOW_TEXT, m_pause),
                     de::Vec2i(), ALIGN_TOP, DPF_NO_OFFSET, 0);

        DGL_Disable(DGL_TEXTURE_2D);

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PopMatrix();
    }

    if(!menuOrMessageVisible)
        return;

    // Draw the fog effect?
    if(fogEffectData.alpha > 0 && cfg.common.hudFog)
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

dd_bool Hu_IsStatusBarVisible(int player)
{
#ifdef __JDOOM64__
    DE_UNUSED(player);
    return false;
#else
    if(!ST_StatusBarIsActive(player)) return false;

    if(ST_AutomapIsOpen(player) && cfg.common.automapHudDisplay == 0)
    {
        return false;
    }

    return true;
#endif
}

#if __JDOOM__ || __JDOOM64__
int Hu_MapTitleFirstLineHeight()
{
    int y = 0;
    res::Uri titleImage = G_MapTitleImage(gfw_Session()->mapUri());
    if(!titleImage.isEmpty())
    {
        if(!titleImage.scheme().compareWithoutCase("Patches"))
        {
            patchinfo_t info;
            patchid_t patchId = R_DeclarePatch(titleImage.path());
            if(R_GetPatchInfo(patchId, &info))
            {
                y = info.geometry.size.height + 2;
            }
        }
    }
    return de::max(14, y);
}
#endif

dd_bool Hu_IsMapTitleAuthorVisible()
{
    const de::String author = G_MapAuthor(gfw_Session()->mapUri(), CPP_BOOL(cfg.common.hideIWADAuthor));
    return !author.isEmpty() && (actualMapTime <= 6 * TICSPERSEC);
}

int Hu_MapTitleHeight(void)
{
    int h = (Hu_IsMapTitleAuthorVisible()? 8 : 0);

#if __JDOOM__ || __JDOOM64__
    return Hu_MapTitleFirstLineHeight() + h;
#endif

#if __JHERETIC__ || __JHEXEN__
    return 20 + h;
#endif
}

void Hu_DrawMapTitle(float alpha, dd_bool mapIdInsteadOfAuthor)
{
    const res::Uri mapUri    = gfw_Session()->mapUri();
    const de::String title  = G_MapTitle(mapUri);
    const de::String author = G_MapAuthor(mapUri, CPP_BOOL(cfg.common.hideIWADAuthor));

    float y = 0;

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, alpha);

    FR_SetFont(FID(GF_FONTB));
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(defFontRGB[0], defFontRGB[1], defFontRGB[2], alpha);

#if __JDOOM__ || __JDOOM64__
    patchid_t patchId = 0;
    const res::Uri titleImage = G_MapTitleImage(mapUri);
    if(!titleImage.isEmpty())
    {
        if(!titleImage.scheme().compareWithoutCase("Patches"))
        {
            patchId = R_DeclarePatch(titleImage.path());
        }
    }
    WI_DrawPatch(patchId, Hu_ChoosePatchReplacement(PRM_ALLOW_TEXT, patchId, title),
                 de::Vec2i(), ALIGN_TOP, 0, DTF_ONLY_SHADOW);

    // Following line of text placed according to patch height.
    y += Hu_MapTitleFirstLineHeight();

#elif __JHERETIC__ || __JHEXEN__
    if(!title.isEmpty())
    {
        FR_DrawTextXY3(title, 0, 0, ALIGN_TOP, DTF_ONLY_SHADOW);
        y += 20;
    }
#endif

    if(mapIdInsteadOfAuthor)
    {
        FR_SetFont(FID(GF_FONTA));
#if defined(__JHERETIC__) || defined(__JHEXEN__)
        FR_SetColorAndAlpha(.85f, .85f, .85f, alpha);
#else
        FR_SetColorAndAlpha(.6f, .6f, .6f, alpha);
#endif
        FR_DrawTextXY3(mapUri.path(), 0, y, ALIGN_TOP, DTF_ONLY_SHADOW);
    }
    else if(!author.isEmpty())
    {
        FR_SetFont(FID(GF_FONTA));
        FR_SetColorAndAlpha(.5f, .5f, .5f, alpha);
        FR_DrawTextXY3(author, 0, y, ALIGN_TOP, DTF_ONLY_SHADOW);
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

dd_bool Hu_IsMapTitleVisible(void)
{
    if(!cfg.common.mapTitle) return false;

    return (actualMapTime < 6 * 35) || ST_AutomapIsOpen(DISPLAYPLAYER);
}

static dd_bool needToRespectStatusBarHeightWhenAutomapOpen(void)
{
#ifndef __JDOOM64__
    return Hu_IsStatusBarVisible(DISPLAYPLAYER);
#endif

    return false;
}

static dd_bool needToRespectHudSizeWhenAutomapOpen(void)
{
#ifdef __JDOOM__
    if(cfg.hudShown[HUD_FACE] && !Hu_IsStatusBarVisible(DISPLAYPLAYER) &&
       cfg.common.automapHudDisplay > 0) return true;
#endif
    return false;
}

void Hu_MapTitleDrawer(const RectRaw* portGeometry)
{
    if(!cfg.common.mapTitle || !portGeometry) return;

    // Scale according to the viewport size.
    float scale;
    R_ChooseAlignModeAndScaleFactor(&scale, SCREENWIDTH, SCREENHEIGHT,
                                    portGeometry->size.width, portGeometry->size.height,
                                    scalemode_t(cfg.common.menuScaleMode));

    // Determine origin of the title.
    Point2Raw origin = {{{portGeometry->size.width / 2,
                          6 * portGeometry->size.height / SCREENHEIGHT}}};

    // Should the title be positioned in the bottom of the view?
    if(cfg.common.automapTitleAtBottom &&
            ST_AutomapIsOpen(DISPLAYPLAYER) &&
            (actualMapTime > 6 * TICSPERSEC))
    {
        origin.y = portGeometry->size.height - 1.2f * Hu_MapTitleHeight() * scale;

#if __JHERETIC__ || __JHEXEN__
        if(Hu_InventoryIsOpen(DISPLAYPLAYER) && !Hu_IsStatusBarVisible(DISPLAYPLAYER))
        {
            // Omit the title altogether while the inventory is open.
            return;
        }
#endif
        float off = 0; // in vanilla pixels
        if(needToRespectStatusBarHeightWhenAutomapOpen())
        {
            Size2Raw stBarSize;
            R_StatusBarSize(DISPLAYPLAYER, &stBarSize);
            off += stBarSize.height;
        }
        else if(needToRespectHudSizeWhenAutomapOpen())
        {
            off += 30 * cfg.common.hudScale;
        }

        origin.y -= off * portGeometry->size.height / float(SCREENHEIGHT);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    Point2Raw portOrigin;
    R_ViewPortOrigin(DISPLAYPLAYER, &portOrigin);

    // After scaling, the title is centered horizontally on the screen.
    DGL_Translatef(portOrigin.x + origin.x, portOrigin.y + origin.y, 0);

    DGL_Scalef(scale, scale * 1.2f/*aspect correct*/, 1);

    // Level information is shown for a few seconds in the beginning of a level.
    if(actualMapTime <= 6 * TICSPERSEC)
    {
        // Fade the title in and out.
        float alpha = 1;
        if(actualMapTime < 35)
            alpha = actualMapTime / 35.0f;
        if(actualMapTime > 5 * 35)
            alpha = 1 - (actualMapTime - 5 * 35) / 35.0f;

        // Make the title 3/4 smaller.
        DGL_Scalef(.75f, .75f, 1);

        Hu_DrawMapTitle(alpha, false /* show author */);
    }
    else if(ST_AutomapIsOpen(DISPLAYPLAYER) && (actualMapTime > 6 * TICSPERSEC))
    {
        // When the automap is open, the title is displayed together with the
        // map identifier (URI).

        // Fade the title in.
        float alpha = 1;
        if(actualMapTime < 7 * 35)
            alpha = MINMAX_OF(0, (actualMapTime - 6 * 35) / 35.f, 1);

        DGL_Scalef(.5f, .5f, 1);

        Hu_DrawMapTitle(alpha, true /* show map ID */);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void M_DrawShadowedPatch3(patchid_t id, int x, int y, int alignFlags, int patchFlags,
    float r, float g, float b, float a)
{
    if(id == 0 || DD_GetInteger(DD_NOVIDEO))
        return;

    DGL_Color4f(0, 0, 0, a * .4f);
    GL_DrawPatch(id, Vec2i(x + 2, y + 2), alignFlags, patchFlags);

    DGL_Color4f(r, g, b, a);
    GL_DrawPatch(id, Vec2i(x, y), alignFlags, patchFlags);
}

void M_DrawShadowedPatch2(patchid_t id, int x, int y, int alignFlags, int patchFlags)
{
    M_DrawShadowedPatch3(id, x, y, alignFlags, patchFlags, 1, 1, 1, 1);
}

void M_DrawShadowedPatch(patchid_t id, int x, int y)
{
    M_DrawShadowedPatch2(id, x, y, ALIGN_TOPLEFT, 0);
}
