/**
 * @file hu_stuff.h
 * Miscellaneous routines for heads-up displays and UI.
 *
 * @authors Copyright &copy; 2005-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_HU_STUFF_H
#define LIBCOMMON_HU_STUFF_H

#include "doomsday.h"
#include "gl_drawpatch.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    GF_MAPPOINT, ///< Marked points on the automap
    NUM_GAME_FONTS
} gamefontid_t;

// Vector graphics.
enum {
    VG_FIRST = 1,
    VG_KEY = VG_FIRST,
    VG_TRIANGLE,
    VG_ARROW,
#if __JDOOM__
    VG_CHEATARROW,
#endif
    VG_XHAIR1,
    VG_XHAIR2,
    VG_XHAIR3,
    VG_XHAIR4,
    VG_XHAIR5,
    NUM_VECTOR_GRAPHICS
}; /* svgid_t*/

#define FID(idx) (fonts[idx])

extern fontid_t fonts[NUM_GAME_FONTS];

extern patchid_t* pMapNames; // Name graphics of each map.
extern uint pMapNamesSize;

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
extern patchid_t borderPatches[8];

void Hu_LoadData(void);
void Hu_Drawer(void);
void Hu_Ticker(void);
void HU_WakeWidgets(int player);
void Hu_UnloadData(void);

void Hu_MapTitleDrawer(const RectRaw* portGeometry);

void Hu_FogEffectTicker(timespan_t time);
void Hu_FogEffectSetAlphaTarget(float alpha);
void Hu_DrawFogEffect(int effectID, DGLuint tex, float texOffset[2], float texAngle, float alpha, float arg1);

void HU_ScoreBoardUnHide(int player);
void HU_DrawScoreBoard(int player);

void Hu_DrawMapTitle(const Point2Raw* offset);

void Draw_BeginZoom(float s, float originX, float originY);
void Draw_EndZoom(void);

void M_DrawTextFragmentShadowed(const char* string, int x, int y, int alignFlags, short textFlags, float r, float g, float b, float a);

void M_DrawShadowedPatch(patchid_t id, int x, int y);
void M_DrawShadowedPatch2(patchid_t id, int x, int y, int alignFlags, int patchFlags);
void M_DrawShadowedPatch3(patchid_t id, int x, int y, int alignFlags, int patchFlags, float r, float g, float b, float a);

typedef enum {
    PRM_NONE                    = 0, ///< No replacement.
    PRM_ALLOW_TEXT                   ///< Use a text replacement if found.
} patchreplacemode_t;

#define PRM_FIRST               (PRM_NONE)
#define PRM_LAST                (PRM_ALLOW_TEXT)

/**
 * @defgroup patchReplacementFlags  Patch Replacement Flags.
 * @{
 */
#define PRF_NO_IWAD             0x1 ///< Allow if resource does not originate from an IWAD.
#define PRF_NO_PWAD             0x2 ///< Allow if resource does not originate from a PWAD/external source.
/**@}*/

/**
 * Given a unique patch identifier (@a id) lookup a patch replacement string
 * associated with this.
 *
 * @param patchId       Unique patch identifier.
 * @param flags         @ref patchReplacementFlags
 *
 * @return  Patch replacement string if defined/found else @c NULL.
 */
const char* Hu_FindPatchReplacementString(patchid_t patchId, int flags);

/**
 * Determine whether a string-replacement for the specified patch is allowed
 * according the current user and/or game configuration.
 *
 * @note If the patch does not originate from an IWAD it will not be replaced.
 *
 * @param replaceMode   Replacement mode.
 * @param patchId       Unique identifier of the patch to choose a replacement for.
 * @param text          A prechoosen string replacement to be used if appropriate.
 */
const char* Hu_ChoosePatchReplacement2(patchreplacemode_t replaceMode, patchid_t patchId, const char* text);
const char* Hu_ChoosePatchReplacement(patchreplacemode_t replaceMode, patchid_t patchId);

/**
 * Implements patch replacement.
 *
 * @param patchId   Unique identifier of the patch to be drawn if no replacement.
 * @param replacement   Patch replacement string. Will be drawn instead of the
 *                      patch if not @c NULL.
 * @param origin        Orient drawing about this offset (topleft:[0,0]).
 * @param alignFlags    @ref alignmentFlags
 * @param patchFlags    @ref drawPatchFlags
 * @param textFlags     @ref drawTextFlags
 */
void WI_DrawPatch3(patchid_t patchId, const char* replacement, const Point2Raw* origin, int alignFlags, int patchFlags, short textFlags);
void WI_DrawPatch2(patchid_t patchId, const char* replacement, const Point2Raw* origin, int alignFlags);
void WI_DrawPatch(patchid_t patchId, const char* replacement, const Point2Raw* origin);

/**
 * Same as @a WI_DrawPatch except origin is specified with separate xy coordinates.
 */
void WI_DrawPatchXY3(patchid_t patchId, const char* replacement, int x, int y, int alignFlags, int patchFlags, short textFlags);
void WI_DrawPatchXY2(patchid_t patchId, const char* replacement, int x, int y, int alignFlags);
void WI_DrawPatchXY(patchid_t patchId, const char* replacement, int x, int y);

/**
 * Misc specialised elements:
 */

void M_DrawGlowBar(const float a[2], const float b[2], float thickness, boolean left, boolean right, boolean caps, float red, float green, float blue, float alpha);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_HU_STUFF_H */
