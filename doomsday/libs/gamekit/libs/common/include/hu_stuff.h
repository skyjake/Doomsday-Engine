/**
 * @file hu_stuff.h
 * Miscellaneous routines for heads-up displays and UI.
 *
 * @authors Copyright &copy; 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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
#  include <de/vector.h>
#endif

#if __JHERETIC__ || __JHEXEN__
DE_EXTERN_C patchid_t pInvItemBox;
DE_EXTERN_C patchid_t pInvSelectBox;
DE_EXTERN_C patchid_t pInvPageLeft[2];
DE_EXTERN_C patchid_t pInvPageRight[2];
#endif

#if __JDOOM__ || __JDOOM64__
// Quit messages.
#define NUM_QUITMESSAGES    22
DE_EXTERN_C const char *endmsg[NUM_QUITMESSAGES + 1];
#endif

DE_EXTERN_C dd_bool shiftdown;
DE_EXTERN_C char const shiftXForm[];
DE_EXTERN_C patchid_t borderPatches[8];

// The fonts.
typedef enum {
    GF_NONE,
    GF_FIRST,
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

DE_EXTERN_C fontid_t fonts[NUM_GAME_FONTS];
#define FID(idx) (fonts[idx])

// Vector graphics.
enum {
    VG_FIRST = 1,
    VG_KEY = VG_FIRST,
    VG_TRIANGLE,
    VG_ARROW,
    VG_CHEATARROW,
    VG_XHAIR1,
    VG_XHAIR2,
    VG_XHAIR3,
    VG_XHAIR4,
    VG_XHAIR5,
    NUM_VECTOR_GRAPHICS
}; /* svgid_t*/

#ifdef __cplusplus
extern "C" {
#endif

void Hu_LoadData(void);
void Hu_Drawer(void);
void Hu_Ticker(void);
void HU_WakeWidgets(int player);
void Hu_UnloadData(void);

void Hu_MapTitleDrawer(const RectRaw* portGeometry);
void Hu_DrawMapTitle(float alpha, dd_bool mapIdInsteadOfAuthor);
dd_bool Hu_IsMapTitleVisible(void);
dd_bool Hu_IsStatusBarVisible(int player);

void Hu_FogEffectTicker(timespan_t time);
void Hu_FogEffectSetAlphaTarget(float alpha);
void Hu_DrawFogEffect(int effectID, DGLuint tex, float texOffset[2], float texAngle, float alpha, float arg1);

void HU_ScoreBoardUnHide(int player);
void HU_DrawScoreBoard(int player);

/**
 * Determines the height of the map title, including the second line
 * (author/map ID).
 */
int Hu_MapTitleHeight(void);

void Draw_BeginZoom(float s, float originX, float originY);
void Draw_EndZoom(void);

void M_DrawTextFragmentShadowed(const char* string, int x, int y, int alignFlags, short textFlags, float r, float g, float b, float a);

void M_DrawShadowedPatch(patchid_t id, int x, int y);
void M_DrawShadowedPatch2(patchid_t id, int x, int y, int alignFlags, int patchFlags);
void M_DrawShadowedPatch3(patchid_t id, int x, int y, int alignFlags, int patchFlags, float r, float g, float b, float a);

/**
 * Misc specialised elements:
 */

void M_DrawGlowBar(const float a[2], const float b[2], float thickness, dd_bool left, dd_bool right, dd_bool caps, float red, float green, float blue, float alpha);

#ifdef __cplusplus
} // extern "C"

enum patchreplacemode_t
{
    PRM_NONE,       ///< No replacement.
    PRM_ALLOW_TEXT  ///< Use a text replacement if found.
};

/**
 * @defgroup patchReplacementFlags  Patch Replacement Flags.
 * @{
 */
#define PRF_NO_IWAD             0x1  ///< Allow if resource does not originate from an IWAD.
#define PRF_NO_PWAD             0x2  ///< Allow if resource does not originate from a PWAD/external source.
/**@}*/

/**
 * Given a unique patch identifier (@a id) lookup a patch replacement string
 * associated with this.
 *
 * @param patchId  Unique patch identifier.
 * @param flags    @ref patchReplacementFlags
 *
 * @return  Patch replacement string if defined/found else @c nullptr.
 */
const char *Hu_FindPatchReplacementString(patchid_t patchId, int flags);

/**
 * Determine whether a string-replacement for the specified patch is allowed
 * according the current user and/or game configuration.
 *
 * @note If the patch does not originate from an IWAD it will not be replaced.
 *
 * @param replaceMode  Replacement mode.
 * @param patchId      Unique identifier of the patch to choose a replacement for.
 * @param text         A prechoosen string replacement to be used if appropriate.
 */
de::String Hu_ChoosePatchReplacement(patchreplacemode_t replaceMode, patchid_t patchId,
                                     const de::String &text = "");

/**
 * Implements patch replacement.
 *
 * @param patchId      Unique identifier of the patch to be drawn if no replacement.
 * @param replacement  Patch replacement string. Will be drawn instead of the
 *                     patch if not @c nullptr.
 * @param origin       Orient drawing about this offset (topleft:[0,0]).
 * @param alignFlags   @ref alignmentFlags
 * @param patchFlags   @ref drawPatchFlags
 * @param textFlags    @ref drawTextFlags
 */
void WI_DrawPatch(patchid_t patchId,
                  const de::String &replacement = "",
                  const de::Vec2i &origin    = de::Vec2i(0, 0),
                  int alignFlags                = ALIGN_TOPLEFT,
                  int patchFlags                = 0,
                  short textFlags               = 0);

#endif // __cplusplus
#endif // LIBCOMMON_HU_STUFF_H
