/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * hu_stuff.h:
 */

#ifndef __COMMON_HU_STUFF_H__
#define __COMMON_HU_STUFF_H__

#include "doomsday.h"

#include "r_common.h"

typedef enum border_e {
    BORDERUP = 1,
    BORDERDOWN
} border_t;

enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

// The fonts.
typedef enum {
    GF_FIRST = 0,
    GF_FONTA = GF_FIRST,
    GF_FONTB,
    NUM_GAME_FONTS
} gamefontid_t;

typedef struct gamefont_s {
    struct gamefont_char_s {
        char            lumpname[9];
        dpatch_t        patch;
    } chars[256];
} gamefont_t;

extern dpatch_t huMinus;
#if __JHERETIC__ || __JHEXEN__
extern dpatch_t dpSmallNumbers[10];
#endif

// Name graphics of each map.
extern dpatch_t* mapNamePatches;
// Name graphics of each skill mode.
#if __JDOOM__ || __JDOOM64__
extern dpatch_t skillModeNames[NUM_SKILL_MODES];
extern dpatch_t m_pause; // Paused graphic.
#endif
// Name graphics of each episode.
#if __JDOOM__
extern dpatch_t* episodeNamePatches;
#endif

extern const char shiftXForm[];
extern int typeInTime;

#if __JDOOM__
// Plutonia and TNT map names.
extern char *mapNamesPlut[32], *mapNamesTNT[32];
#endif

#if __JHERETIC__ || __JHEXEN__
extern dpatch_t dpInvItemBox;
extern dpatch_t dpInvSelectBox;
extern dpatch_t dpInvPageLeft[2];
extern dpatch_t dpInvPageRight[2];
#endif

void            Hu_LoadData(void);
void            Hu_Drawer(void);
void            Hu_Ticker(void);
void            Hu_FogEffectTicker(timespan_t time);

void            Hu_FogEffectSetAlphaTarget(float alpha);

void            Hu_DrawMapTitle(int x, int y, float scale);

// Implements patch replacement.
void        WI_DrawPatch(int x, int y, float r, float g, float b, float a,
                         const dpatch_t* patch, const char* altstring,
                         boolean builtin, int halign);
void        WI_DrawParamText(int x, int y, const char* string,
                             gamefontid_t font, float defRed,
                             float defGreen, float defBlue, float defAlpha,
                             boolean defCase, boolean defTypeIn,
                             int halign);
void        M_WriteText(int x, int y, const char *string);
void        M_WriteText2(int x, int y, const char *string,
                         gamefontid_t font, float red,
                         float green, float blue, float alpha);
void        M_WriteText3(int x, int y, const char *string,
                         gamefontid_t font, float red, float green,
                         float blue, float alpha,
                         boolean flagTypeIn, boolean flagShadow,
                         int initialCount);
void        HUlib_drawTextLine2(int x, int y, const char* string,
                                size_t len, gamefontid_t font,
                                boolean drawcursor);
void        M_DrawChar(int x, int y, unsigned char ch, gamefontid_t font);
#if __JHERETIC__ || __JHEXEN__
void        Hu_DrawSmallNum(int val, int numDigits, int x, int y,
                            float alpha);
#endif
#if __JHERETIC__
void HU_DrawBNumber(signed int val, int x, int y, float red,
                    float green, float blue, float alpha);
void IN_DrawNumber(int val, int x, int y, int digits, float r, float g,
                   float b, float a);
void IN_DrawShadowChar(int x, int y, unsigned char ch, gamefontid_t font);
#endif
#if __JHEXEN__
void DrBNumber(int val, int x, int y, float red, float green, float blue,
               float alpha);
#endif
int         M_DrawText(int x, int y, boolean direct, char *string);
void        M_DrawTitle(char *text, int y);

int         M_StringWidth(const char* string, gamefontid_t font);
int         M_CharWidth(unsigned char ch, gamefontid_t font);
int         M_CharHeight(unsigned char ch, gamefontid_t font);
int         M_StringHeight(const char* string, gamefontid_t font);

void        M_DrawBackgroundBox(float x, float y, float w, float h,
                                float red, float green, float blue,
                                float alpha, boolean background,
                                int border);
#if __JHERETIC__ || __JHEXEN__
void        M_DrawSlider(int x, int y, int width, int slot, float alpha);
#else
void        M_DrawSlider(int x, int y, int width, int height, int slot,
                         float alpha);
#endif

void        Draw_BeginZoom(float s, float originX, float originY);
void        Draw_EndZoom(void);

void        HU_Start(int player);
void        Hu_UnloadData(void);
char        HU_dequeueChatChar(void);
void        HU_Erase(void);

void        Hu_DrawFogEffect(int effectID, DGLuint tex, float texOffset[2],
                             float texAngle, float alpha, float arg1);

void        HU_ScoreBoardUnHide(int player);
void        HU_DrawScoreBoard(int player);

typedef struct {
    int winWidth, winHeight;
    float scale;
    int displayMode;
    int scissorState[5];
} borderedprojectionstate_t;

void            Hu_ConfigureBorderedProjection(borderedprojectionstate_t* s);

void            Hu_BeginBorderedProjection(borderedprojectionstate_t* s);
void            Hu_EndBorderedProjection(borderedprojectionstate_t* s);

#endif
