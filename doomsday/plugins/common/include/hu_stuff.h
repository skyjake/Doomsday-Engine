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
        patchinfo_t     pInfo;
    } chars[256];
} gamefont_t;

extern patchinfo_t huMinus;
#if __JHERETIC__ || __JHEXEN__
extern patchinfo_t dpSmallNumbers[10];
#endif

// Name graphics of each map.
extern patchinfo_t* mapNamePatches;
// Name graphics of each skill mode.
#if __JDOOM__ || __JDOOM64__
extern patchinfo_t skillModeNames[NUM_SKILL_MODES];
#endif
extern patchinfo_t m_pause; // Paused graphic.

// Name graphics of each episode.
#if __JDOOM__
extern patchinfo_t* episodeNamePatches;
#endif

extern const char shiftXForm[];
extern int typeInTime;

#if __JDOOM__
// Plutonia and TNT map names.
extern char* mapNamesPlut[32],* mapNamesTNT[32];
#endif

#if __JHERETIC__ || __JHEXEN__
extern patchinfo_t dpInvItemBox;
extern patchinfo_t dpInvSelectBox;
extern patchinfo_t dpInvPageLeft[2];
extern patchinfo_t dpInvPageRight[2];
#endif

void            Hu_LoadData(void);
void            Hu_Drawer(void);
void            Hu_Ticker(void);
void            HU_Start(int player);
void            Hu_UnloadData(void);

void            Hu_FogEffectTicker(timespan_t time);
void            Hu_FogEffectSetAlphaTarget(float alpha);
void            Hu_DrawFogEffect(int effectID, DGLuint tex, float texOffset[2], float texAngle, float alpha, float arg1);

void            HU_ScoreBoardUnHide(int player);
void            HU_DrawScoreBoard(int player);

void            Hu_DrawMapTitle(int x, int y, float scale);

typedef struct {
    int winWidth, winHeight;
    float scale;
    int displayMode;
    int scissorState[5];
} borderedprojectionstate_t;

void            Hu_ConfigureBorderedProjection(borderedprojectionstate_t* s);

void            Hu_BeginBorderedProjection(borderedprojectionstate_t* s);
void            Hu_EndBorderedProjection(borderedprojectionstate_t* s);

void            Draw_BeginZoom(float s, float originX, float originY);
void            Draw_EndZoom(void);

/**
 * Text string:
 */
void            M_WriteText(const char* string, int x, int y);
void            M_WriteText2(const char* string, int x, int y, gamefontid_t font, float red, float green, float blue, float alpha);
void            M_WriteText3(const char* string, int x, int y, gamefontid_t font, float red, float green, float blue, float alpha, boolean flagTypeIn, boolean flagShadow, int initialCount);
void            HUlib_drawTextLine2(const char* string, int x, int y, size_t len, gamefontid_t font, boolean drawcursor);

int             M_DrawText(const char* string, int x, int y, boolean direct);
void            M_DrawTitle(const char* string, int y);

void            WI_DrawParamText(const char* string, int x, int y, gamefontid_t font, float defRed, float defGreen, float defBlue, float defAlpha, boolean defCase, boolean defTypeIn, boolean defShadow, int halign);

// Utility routines:
int             M_StringWidth(const char* string, gamefontid_t font);
int             M_StringHeight(const char* string, gamefontid_t font);

/**
 * Single character:
 */
void            M_DrawChar(unsigned char ch, int x, int y, gamefontid_t font);
#if __JHERETIC__
void            IN_DrawShadowChar(unsigned char ch, int x, int y, gamefontid_t font);
#endif

// Utility routines:
int             M_CharWidth(unsigned char ch, gamefontid_t font);
int             M_CharHeight(unsigned char ch, gamefontid_t font);

/**
 * Specialised text strings:
 */
#if __JHERETIC__ || __JHEXEN__
void            Hu_DrawSmallNum(int val, int numDigits, int x, int y, float alpha);
#endif
#if __JHERETIC__
void            HU_DrawBNumber(int val, int x, int y, float red, float green, float blue, float alpha);
void            IN_DrawNumber(int val, int x, int y, int digits, float r, float g, float b, float a);
#endif
#if __JHEXEN__
void            DrBNumber(int val, int x, int y, float red, float green, float blue, float alpha);
#endif

/**
 * Patches:
 */

/**
 * @defGroup drawPatchFlags Draw Patch Flags.
 */
/*@{*/
#define DPF_ALIGN_LEFT      0x1
#define DPF_ALIGN_RIGHT     0x2
#define DPF_NO_OFFSETX      0x4
#define DPF_NO_OFFSETY      0x8
#define DPF_NO_OFFSET       (DPF_NO_OFFSETX | DPF_NO_OFFSETY)
/*@}*/

void            Hu_DrawPatch(patchid_t id, int x, int y);
void            Hu_DrawPatch2(patchid_t id, int x, int y, byte flags);

void            Hu_DrawShadowedPatch(patchid_t id, int x, int y);
void            Hu_DrawShadowedPatch2(patchid_t id, int x, int y, byte flags);
void            Hu_DrawShadowedPatch3(patchid_t id, int x, int y, byte flags, float r, float g, float b, float a);

// Implements patch replacement.
void            WI_DrawPatch(patchid_t id, int x, int y, const char* altstring, boolean builtin);
void            WI_DrawPatch2(patchid_t id, int x, int y, const char* altstring, boolean builtin, int halign);
void            WI_DrawPatch3(patchid_t id, int x, int y, const char* altstring, boolean builtin, int halign, float r, float g, float b, float a);

/**
 * Misc specialised elements:
 */
void            M_DrawBackgroundBox(float x, float y, float w, float h, boolean background, int border, float red, float green, float blue, float alpha);
#if __JHERETIC__ || __JHEXEN__
void            M_DrawSlider(int x, int y, int width, int slot, float alpha);
#else
void            M_DrawSlider(int x, int y, int width, int height, int slot, float alpha);
#endif
void            M_DrawGlowBar(const float a[2], const float b[2], float thickness, boolean left, boolean right, boolean caps, float red, float green, float blue, float alpha);

#endif
