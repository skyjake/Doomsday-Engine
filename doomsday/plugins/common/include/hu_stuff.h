/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

#define HU_FONTSTART        '!' // the first font characters
#define HU_FONTEND          '_' // the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE         (HU_FONTEND - HU_FONTSTART + 1)

typedef enum border_e {
    BORDERUP = 1,
    BORDERDOWN
} border_t;

// The fonts.
extern dpatch_t huFont[HU_FONTSIZE];
extern dpatch_t huFontA[HU_FONTSIZE], huFontB[HU_FONTSIZE];
extern dpatch_t huMinus;

#if __JHERETIC__
extern dpatch_t sNumbers[10];
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

// Implements patch replacement.
void        WI_DrawPatch(int x, int y, float r, float g, float b, float a,
                         const dpatch_t* patch, const char *altstring,
                         boolean builtin, int halign);

void        WI_DrawParamText(int x, int y, const char* string,
                             dpatch_t* defFont, float defRed, float defGreen,
                             float defBlue, float defAlpha, boolean defCase,
                             boolean defTypeIn, int halign);

void        M_WriteText(int x, int y, const char *string);
void        M_WriteText2(int x, int y, const char *string, dpatch_t *font, float red,
                         float green, float blue, float alpha);
void        M_WriteText3(int x, int y, const char *string, dpatch_t *font,
                         float red, float green, float blue, float alpha,
                         boolean doTypeIn, int initialCount);

#if __JHERETIC__
void        Hu_DrawSmallNum(int val, int numDigits, int x, int y, float alpha);
#endif

int         M_DrawText(int x, int y, boolean direct, char *string);
void        M_DrawTitle(char *text, int y);

int         M_StringWidth(const char* string, dpatch_t* font);
int         M_StringHeight(const char* string, dpatch_t* font);
void        M_DrawBackgroundBox(float x, float y, float w, float h, float red, float green,
                                float blue, float alpha, boolean background,
                                int border);
#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
void        M_DrawSlider(int x, int y, int width, int slot, float alpha);
#else
void        M_DrawSlider(int x, int y, int width, int height, int slot, float alpha);
#endif

void        Draw_BeginZoom(float s, float originX, float originY);
void        Draw_EndZoom(void);

void        HU_Register(void);

void        HU_Start(int player);
void        Hu_UnloadData(void);
void        HU_Drawer(int player);
char        HU_dequeueChatChar(void);
void        HU_Erase(void);

void        HU_DrawMapCounters(void);

void        Hu_DrawFogEffect(int effectID, DGLuint tex, float texOffset[2],
                             float texAngle, float alpha, float arg1);

void        HU_ScoreBoardUnHide(int player);
void        HU_DrawScoreBoard(int player);

#endif
