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

#include "gl_drawcompositefont.h"
#include "gl_drawpatch.h"

#if __JHEXEN__
# include "h2def.h"
#else
# include "doomdef.h"
#endif

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

void            M_DrawTextFragmentShadowed(const char* string, int x, int y, gamefontid_t font, short flags, int tracking, float r, float g, float b, float a);

void            M_DrawShadowedChar(unsigned char ch, int x, int y);
void            M_DrawShadowedChar2(unsigned char ch, int x, int y, gamefontid_t font);
void            M_DrawShadowedChar3(unsigned char ch, int x, int y, gamefontid_t font, short flags, float r, float g, float b, float a);

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
