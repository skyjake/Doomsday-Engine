/**\file hu_stuff.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_HU_STUFF_H
#define LIBCOMMON_HU_STUFF_H

#include "doomsday.h"
#include "gl_drawpatch.h"

// The fonts.
typedef enum {
    GF_FIRST = 1,
    GF_FONTA = GF_FIRST,
    GF_FONTB,
    GF_STATUS,
#if __JDOOM__
    GF_INDEX, // Used for the ready/max ammo on the statusbar
#endif
#if __JDOOM__ || __JDOOM64__
    GF_SMALL, // Used on the intermission.
#endif
#if __JHERETIC__ || __JHEXEN__
    GF_SMALLIN,
#endif
    NUM_GAME_FONTS
} gamefontid_t;

// Vector graphics.
enum {
    VG_FIRST = 1,
    VG_KEYSQUARE = VG_FIRST,
    VG_TRIANGLE,
    VG_ARROW,
#if !__JHEXEN__
    VG_CHEATARROW,
#endif
    VG_XHAIR1,
    VG_XHAIR2,
    VG_XHAIR3,
    VG_XHAIR4,
    VG_XHAIR5,
    VG_XHAIR6,
    NUM_VECTOR_GRAPHICS
}; /* vectorgraphicid_t*/

#define FID(idx) (fonts[idx])

extern fontid_t fonts[NUM_GAME_FONTS];

extern patchid_t* pMapNames; // Name graphics of each map.

#if __JHERETIC__ || __JHEXEN__
extern patchid_t pInvItemBox;
extern patchid_t pInvSelectBox;
extern patchid_t pInvPageLeft[2];
extern patchid_t pInvPageRight[2];
#endif

#if __JDOOM__ || __JDOOM64__
// Quit messages.
#define NUM_QUITMESSAGES    22
extern char* endmsg[NUM_QUITMESSAGES + 1];
#endif

extern boolean shiftdown;
extern const char shiftXForm[];

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

void            Draw_BeginZoom(float s, float originX, float originY);
void            Draw_EndZoom(void);

void            M_DrawTextFragmentShadowed(const char* string, int x, int y, int fontIdx, short flags, int tracking, float r, float g, float b, float a);

void            M_DrawShadowedPatch(patchid_t id, int x, int y);
void            M_DrawShadowedPatch2(patchid_t id, int x, int y, short flags);
void            M_DrawShadowedPatch3(patchid_t id, int x, int y, short flags, float r, float g, float b, float a);

/**
 * @defgroup patchReplacementFlags  Patch Replacement Flags.
 * @{
 */
#define PRF_NO_IWAD             0x1 /// Allow if resource does not originate from an IWAD.
#define PRF_NO_PWAD             0x2 /// Allow if resource does not originate from a PWAD/external source.
/**@}*/

/**
 * Given a unique patch identifier (@a id) lookup a patch replacement string
 * associated with this.
 * @param patchId  Unique patch identifier.
 * @param flags  @see patchReplacementFlags
 * @return  Patch replacement string if defined/found else @c NULL.
 */
const char* Hu_FindPatchReplacementString(patchid_t patchId, int flags);

/**
 * Determine whether a string-replacement for the specified patch is allowed
 * according the current user and/or game configuration.
 *
 * \note If the patch does not originate from an IWAD it will not be replaced.
 *
 * @param altString  A predetermined string to use if appropriate.
 * @param builtin  @c true= @a altString is a built-in replacement (i.e., it
 *      does not originate from an external replacement definition).
 */
const char* Hu_ChoosePatchReplacement2(patchid_t patchId, const char* altString,
    boolean builtin);
const char* Hu_ChoosePatchReplacement(patchId);

/**
 * Implements patch replacement.
 *
 * @param patchId  Unique identifier of the patch to be drawn if no replacement.
 * @param replacement  Patch replacement string. Will be drawn instead of the
 *      patch if not @c NULL.
 * @param x  X-offset to the draw origin.
 * @param y  Y-offset to the draw origin.
 * @param flags  @see drawPatchFlags
 * @param fontId  Used with patch replacement; default font.
 * @param r  Used with patch replacement; default text color red color compontent.
 * @param g  Used with patch replacement; default text color green color compontent.
 * @param b  Used with patch replacement; default text color blue color compontent.
 * @param a  Patch alpha OR default text alpha when used with patch replacement.
 * @param glitter  Used with patch replacement; default text glitter strength.
 * @param shadow  Used with patch replacement; default text shadow strength.
 */
void WI_DrawPatch5(patchid_t patchId, const char* replacement, int x, int y, short flags, fontid_t fontId, float r, float g, float b, float a, float glitter, float shadow);
void WI_DrawPatch4(patchid_t patchId, const char* replacement, int x, int y, short flags, fontid_t fontId, float r, float g, float b, float a);
void WI_DrawPatch3(patchid_t patchId, const char* replacement, int x, int y, short flags, fontid_t fontId);
void WI_DrawPatch2(patchid_t patchId, const char* replacement, int x, int y, short flags);
void WI_DrawPatch(patchid_t patchId, const char* replacement, int x, int y);

/**
 * Misc specialised elements:
 */

// Border drawing modes.
typedef enum border_e {
    BORDERUP = 1,
    BORDERDOWN
} border_t;

void            M_DrawBackgroundBox(float x, float y, float w, float h, boolean background, int border, float red, float green, float blue, float alpha);
void            M_DrawGlowBar(const float a[2], const float b[2], float thickness, boolean left, boolean right, boolean caps, float red, float green, float blue, float alpha);

#endif /* LIBCOMMON_HU_STUFF_H */
