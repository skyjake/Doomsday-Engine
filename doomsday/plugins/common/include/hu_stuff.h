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

// The fonts.
typedef enum {
    GF_FIRST = 0,
    GF_FONTA = GF_FIRST,
    GF_FONTB,
    GF_STATUS,
#if __JDOOM__
    GF_INDEX,
#endif
#if __JHERETIC__ || __JHEXEN__
    GF_SMALLIN,
#endif
    NUM_GAME_FONTS
} gamefontid_t;

typedef struct gamefont_s {
    struct gamefont_char_s {
        char            lumpname[9];
        patchinfo_t     pInfo;
    } chars[256];
} gamefont_t;

extern patchid_t* mapNamePatches; // Name graphics of each map.
#if __JDOOM__ || __JDOOM64__
extern patchid_t skillModeNames[NUM_SKILL_MODES]; // Name graphics of each skill mode.
#endif
#if __JDOOM__
extern patchid_t* episodeNamePatches; // Name graphics of each episode.
#endif
#if __JHERETIC__ || __JHEXEN__
extern patchid_t dpInvItemBox;
extern patchid_t dpInvSelectBox;
extern patchid_t dpInvPageLeft[2];
extern patchid_t dpInvPageRight[2];
#endif

extern const char shiftXForm[];
extern int typeInTime;

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
 * Text string fragments:
 */

/**
 * @defGroup drawTextFlags Draw Text Flags
 */
/*@{*/
#define DTF_ALIGN_LEFT      0x0001
#define DTF_ALIGN_RIGHT     0x0002
#define DTF_ALIGN_BOTTOM    0x0004
#define DTF_ALIGN_TOP       0x0008
#define DTF_NO_TYPEIN       0x0010
#define DTF_NO_SHADOW       0x0020

#define DTF_NO_EFFECTS      (DTF_NO_TYPEIN|DTF_NO_SHADOW)
#define DTF_ALIGN_TOPLEFT   (DTF_ALIGN_TOP|DTF_ALIGN_LEFT)
#define DTF_ALIGN_BOTTOMLEFT (DTF_ALIGN_BOTTOM|DTF_ALIGN_LEFT)
#define DTF_ALIGN_TOPRIGHT  (DTF_ALIGN_TOP|DTF_ALIGN_RIGHT)
#define DTF_ALIGN_BOTTOMRIGHT (DTF_ALIGN_BOTTOM|DTF_ALIGN_RIGHT)

/*@}*/

void            M_DrawTextFragment(const char* string, int x, int y);
void            M_DrawTextFragment2(const char* string, int x, int y, gamefontid_t font);
void            M_DrawTextFragment3(const char* string, int x, int y, gamefontid_t font, short flags);
void            M_DrawTextFragment4(const char* string, int x, int y, gamefontid_t font, short flags, int initialCount);

// Utility routines:
int             M_TextFragmentWidth(const char* string, gamefontid_t font);
int             M_TextFragmentHeight(const char* string, gamefontid_t font);

/**
 * Text strings (formatted).
 */
void            M_DrawText(const char* string, int x, int y, gamefontid_t font, short flags, float defRed, float defGreen, float defBlue, float defAlpha, boolean defCase);

// Utility routines:
int             M_TextWidth(const char* string, gamefontid_t font);
int             M_TextHeight(const char* string, gamefontid_t font);

/**
 * Single character:
 */
void            M_DrawChar(unsigned char ch, int x, int y);
void            M_DrawChar2(unsigned char ch, int x, int y, gamefontid_t font);
void            M_DrawChar3(unsigned char ch, int x, int y, gamefontid_t font, short flags);

void            M_DrawShadowedChar(unsigned char ch, int x, int y);
void            M_DrawShadowedChar2(unsigned char ch, int x, int y, gamefontid_t font);
void            M_DrawShadowedChar3(unsigned char ch, int x, int y, gamefontid_t font, short flags, float r, float g, float b, float a);

// Utility routines:
int             M_CharWidth(unsigned char ch, gamefontid_t font);
int             M_CharHeight(unsigned char ch, gamefontid_t font);

/**
 * Specialised text strings:
 */
#if __JHERETIC__ || __JHEXEN__
void            Hu_DrawSmallNum(int val, int numDigits, int x, int y, float alpha);
void            DrINumber(int val, int x, int y, float r, float g, float b, float a);
#endif
#if __JHERETIC__
void            M_DrawSmallNumber(int val, int x, int y);
void            IN_DrawNumber(int val, int x, int y, int digits, float r, float g, float b, float a);
void            IN_DrawTime(int x, int y, int h, int m, int s, float r, float g, float b, float a);
#endif

/**
 * Patches:
 */

/**
 * @defGroup drawPatchFlags Draw Patch Flags.
 */
/*@{*/
#define DPF_ALIGN_LEFT      0x0001
#define DPF_ALIGN_RIGHT     0x0002
#define DPF_ALIGN_BOTTOM    0x0004
#define DPF_ALIGN_TOP       0x0008
#define DPF_NO_OFFSETX      0x0010
#define DPF_NO_OFFSETY      0x0020

#define DPF_NO_OFFSET       (DPF_NO_OFFSETX|DPF_NO_OFFSETY)
#define DPF_ALIGN_TOPLEFT   (DPF_ALIGN_TOP|DPF_ALIGN_LEFT)
#define DPF_ALIGN_BOTTOMLEFT (DPF_ALIGN_BOTTOM|DPF_ALIGN_LEFT)
#define DPF_ALIGN_TOPRIGHT  (DPF_ALIGN_TOP|DPF_ALIGN_RIGHT)
#define DPF_ALIGN_BOTTOMRIGHT (DPF_ALIGN_BOTTOM|DPF_ALIGN_RIGHT)
/*@}*/

void            M_DrawPatch(patchid_t id, int x, int y);
void            M_DrawPatch2(patchid_t id, int x, int y, short flags);

void            M_DrawShadowedPatch(patchid_t id, int x, int y);
void            M_DrawShadowedPatch2(patchid_t id, int x, int y, short flags);
void            M_DrawShadowedPatch3(patchid_t id, int x, int y, short flags, float r, float g, float b, float a);

// Implements patch replacement.
void            WI_DrawPatch(patchid_t id, int x, int y);
void            WI_DrawPatch2(patchid_t id, int x, int y, const char* altstring, gamefontid_t font, boolean builtin);
void            WI_DrawPatch3(patchid_t id, int x, int y, const char* altstring, gamefontid_t font, boolean builtin, short flags);
void            WI_DrawPatch4(patchid_t id, int x, int y, const char* altstring, gamefontid_t font, boolean builtin, short flags, float r, float g, float b, float a);

/**
 * Misc specialised elements:
 */

// Border drawing modes.
typedef enum border_e {
    BORDERUP = 1,
    BORDERDOWN
} border_t;

void            M_DrawBackgroundBox(float x, float y, float w, float h, boolean background, int border, float red, float green, float blue, float alpha);
void            M_DrawSlider(int x, int y, int height, int range, int pos, float alpha);
void            M_DrawGlowBar(const float a[2], const float b[2], float thickness, boolean left, boolean right, boolean caps, float red, float green, float blue, float alpha);

#endif
