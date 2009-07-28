/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * hu_stuff.c: Heads-up displays, font handling, text drawing routines
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_stuff.h"
#include "hu_log.h"
#include "p_mapsetup.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
// Counter Cheat flags.
#define CCH_KILLS               0x1
#define CCH_ITEMS               0x2
#define CCH_SECRET              0x4
#define CCH_KILLS_PRCNT         0x8
#define CCH_ITEMS_PRCNT         0x10
#define CCH_SECRET_PRCNT        0x20
#endif

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         active;
    int             scoreHideTics;
    float           scoreAlpha;
} hudstate_t;

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

typedef struct fontpatch_s {
    byte        ch;
    char        lumpName[9];
} fontpatch_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static gamefont_t gFonts[NUM_GAME_FONTS];
static gamefontid_t currentGFontIndex;

#if __JHERETIC__ || __JHEXEN__
dpatch_t dpSmallNumbers[10];
#endif
dpatch_t huMinus;

int typeInTime = 0;

#if __JDOOM__ || __JDOOM64__
// Name graphics of each map.
dpatch_t* mapNamePatches = NULL;
// Name graphics of each skill mode.
dpatch_t skillModeNames[NUM_SKILL_MODES];
dpatch_t m_pause; // Paused graphic.
#endif

#if __JDOOM__
// Name graphics of each episode.
dpatch_t* episodeNamePatches = NULL;
#endif

#if __JHERETIC__ || __JHEXEN__
dpatch_t dpInvItemBox;
dpatch_t dpInvSelectBox;
dpatch_t dpInvPageLeft[2];
dpatch_t dpInvPageRight[2];
#endif

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

cvar_t hudCVars[] = {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {"map-cheat-counter", 0, CVT_BYTE, &cfg.counterCheat, 0, 63},
    {"map-cheat-counter-scale", 0, CVT_FLOAT, &cfg.counterCheatScale, .1f, 1},
#endif
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static hudstate_t hudStates[MAXPLAYERS];

static dpatch_t borderPatches[8];

static fogeffectdata_t fogEffectData;

// Code -------------------------------------------------------------------

/**
 * Called during pre-init to register cvars and ccmds for the hud displays.
 */
void HU_Register(void)
{
    uint                i;

    for(i = 0; hudCVars[i].name; ++i)
        Con_AddVariable(&hudCVars[i]);
}

void R_SetFontCharacter(gamefontid_t fontid, byte ch, const char* lumpname)
{
	gamefont_t*			font;

	if(!(fontid >= GF_FIRST && fontid < NUM_GAME_FONTS))
	{
		Con_Message("R_SetFontCharacter: Warning, unknown font id %i.\n",
					(int) fontid);
		return;
	}

    font = &gFonts[fontid];
	memset(font->chars[ch].lumpname, 0, sizeof(font->chars[ch].lumpname));
	strncpy(font->chars[ch].lumpname, lumpname, 8);

    // Instruct Doomsday to load the patch in monochrome mode.
    // (2 = weighted average).
    DD_SetInteger(DD_MONOCHROME_PATCHES, 2);
    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, true);

	R_CachePatch(&font->chars[ch].patch, font->chars[ch].lumpname);

    DD_SetInteger(DD_MONOCHROME_PATCHES, 0);
    DD_SetInteger(DD_UPSCALE_AND_SHARPEN_PATCHES, false);
}

void R_InitFont(gamefontid_t fontid, const fontpatch_t* patches, size_t num)
{
    size_t              i;
	gamefont_t*			font;

    if(!(fontid >= GF_FIRST && fontid < NUM_GAME_FONTS))
	{
		Con_Message("R_InitFont: Warning, unknown font id %i.\n",
					(int) fontid);
		return;
	}

	font = &gFonts[fontid];
    memset(font, 0, sizeof(*font));

    for(i = 0; i < num; ++i)
    {
        const fontpatch_t*  p = &patches[i];

        R_SetFontCharacter(fontid, p->ch, p->lumpName);
    }
}

/**
 * Change the current font.
 */
void R_SetFont(gamefontid_t fontid)
{
	if(!(fontid >= GF_FIRST && fontid < NUM_GAME_FONTS))
        return; // No such font.

    currentGFontIndex = fontid;
}

gamefontid_t R_GetCurrentFont(void)
{
    if(currentGFontIndex == -1)
        return 0;

    return currentGFontIndex;
}

/**
 * Loads the font patches and inits various strings
 * JHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void Hu_LoadData(void)
{
    int                 i;
    char                name[9];
#if __JDOOM__ || __JDOOM64__
    static const char*  skillModePatchNames[] =
    {
        "M_JKILL",
        "M_ROUGH",
        "M_HURT",
        "M_ULTRA",
# if __JDOOM__
        "M_NMARE"
# endif
    };
#endif

#if __JDOOM__
    static const char*  episodePatchNames[] =
    {
        "M_EPI1",
        "M_EPI2",
        "M_EPI3",
        "M_EPI4"
    };
#endif
/**
 * \todo Read this information from a definition (ideally with more user
 * friendly mnemonics).
 */
#if __JDOOM__ || __JDOOM64__
    static const fontpatch_t fontA[] = {
        { 32, "STCFN032" }, // ' '
        { 33, "STCFN033" }, // !
        { 34, "STCFN034" }, // "
        { 35, "STCFN035" }, // #
        { 36, "STCFN036" }, // $
        { 37, "STCFN037" }, // %
        { 38, "STCFN038" }, // &
        { 39, "STCFN039" }, // '
        { 40, "STCFN040" }, // (
        { 41, "STCFN041" }, // )
        { 42, "STCFN042" }, // *
        { 43, "STCFN043" }, // +
        { 44, "STCFN044" }, // ,
        { 45, "STCFN045" }, // -
        { 46, "STCFN046" }, // .
        { 47, "STCFN047" }, // /
        { 48, "STCFN048" }, // 0
        { 49, "STCFN049" }, // 1
        { 50, "STCFN050" }, // 2
        { 51, "STCFN051" }, // 3
        { 52, "STCFN052" }, // 4
        { 53, "STCFN053" }, // 5
        { 54, "STCFN054" }, // 6
        { 55, "STCFN055" }, // 7
        { 56, "STCFN056" }, // 8
        { 57, "STCFN057" }, // 9
        { 58, "STCFN058" }, // :
        { 59, "STCFN059" }, // ;
        { 60, "STCFN060" }, // <
        { 61, "STCFN061" }, // =
        { 62, "STCFN062" }, // >
        { 63, "STCFN063" }, // ?
        { 64, "STCFN064" }, // @
        { 65, "STCFN065" }, // A
        { 66, "STCFN066" }, // B
        { 67, "STCFN067" }, // C
        { 68, "STCFN068" }, // D
        { 69, "STCFN069" }, // E
        { 70, "STCFN070" }, // F
        { 71, "STCFN071" }, // G
        { 72, "STCFN072" }, // H
        { 73, "STCFN073" }, // I
        { 74, "STCFN074" }, // J
        { 75, "STCFN075" }, // K
        { 76, "STCFN076" }, // L
        { 77, "STCFN077" }, // M
        { 78, "STCFN078" }, // N
        { 79, "STCFN079" }, // O
        { 80, "STCFN080" }, // P
        { 81, "STCFN081" }, // Q
        { 82, "STCFN082" }, // R
        { 83, "STCFN083" }, // S
        { 84, "STCFN084" }, // T
        { 85, "STCFN085" }, // U
        { 86, "STCFN086" }, // V
        { 87, "STCFN087" }, // W
        { 88, "STCFN088" }, // X
        { 89, "STCFN089" }, // Y
        { 90, "STCFN090" }, // Z
        { 91, "STCFN091" }, // [
        { 92, "STCFN092" }, // '\'
        { 93, "STCFN093" }, // ]
        { 94, "STCFN094" }, // ^
        { 95, "STCFN095" }, // _
        { 96, "STCFN121" }, // `
        { 97, "STCFN065" }, // a
        { 98, "STCFN066" }, // b
        { 99, "STCFN067" }, // c
        { 100, "STCFN068" }, // d
        { 101, "STCFN069" }, // e
        { 102, "STCFN070" }, // f
        { 103, "STCFN071" }, // g
        { 104, "STCFN072" }, // h
        { 105, "STCFN073" }, // i
        { 106, "STCFN074" }, // j
        { 107, "STCFN075" }, // k
        { 108, "STCFN076" }, // l
        { 109, "STCFN077" }, // m
        { 110, "STCFN078" }, // n
        { 111, "STCFN079" }, // o
        { 112, "STCFN080" }, // p
        { 113, "STCFN081" }, // q
        { 114, "STCFN082" }, // r
        { 115, "STCFN083" }, // s
        { 116, "STCFN084" }, // t
        { 117, "STCFN085" }, // u
        { 118, "STCFN086" }, // v
        { 119, "STCFN087" }, // w
        { 120, "STCFN088" }, // x
        { 121, "STCFN089" }, // y
        { 122, "STCFN090" } // z
    };
    static const fontpatch_t fontB[] = {
        { 32, "FONTB032" }, // ' '
        { 33, "FONTB033" }, // !
        { 34, "FONTB034" }, // "
        { 35, "FONTB035" }, // #
        { 36, "FONTB036" }, // $
        { 37, "FONTB037" }, // %
        { 38, "FONTB038" }, // &
        { 39, "FONTB039" }, // '
        { 40, "FONTB040" }, // (
        { 41, "FONTB041" }, // )
        { 42, "FONTB042" }, // *
        { 43, "FONTB043" }, // +
        { 44, "FONTB044" }, // ,
        { 45, "FONTB045" }, // -
        { 46, "FONTB046" }, // .
        { 47, "FONTB047" }, // /
        { 48, "FONTB048" }, // 0
        { 49, "FONTB049" }, // 1
        { 50, "FONTB050" }, // 2
        { 51, "FONTB051" }, // 3
        { 52, "FONTB052" }, // 4
        { 53, "FONTB053" }, // 5
        { 54, "FONTB054" }, // 6
        { 55, "FONTB055" }, // 7
        { 56, "FONTB056" }, // 8
        { 57, "FONTB057" }, // 9
        { 58, "FONTB058" }, // :
        { 59, "FONTB059" }, // ;
        { 60, "FONTB060" }, // <
        { 61, "FONTB061" }, // =
        { 62, "FONTB062" }, // >
        { 63, "FONTB063" }, // ?
        { 64, "FONTB064" }, // @
        { 65, "FONTB065" }, // A
        { 66, "FONTB066" }, // B
        { 67, "FONTB067" }, // C
        { 68, "FONTB068" }, // D
        { 69, "FONTB069" }, // E
        { 70, "FONTB070" }, // F
        { 71, "FONTB071" }, // G
        { 72, "FONTB072" }, // H
        { 73, "FONTB073" }, // I
        { 74, "FONTB074" }, // J
        { 75, "FONTB075" }, // K
        { 76, "FONTB076" }, // L
        { 77, "FONTB077" }, // M
        { 78, "FONTB078" }, // N
        { 79, "FONTB079" }, // O
        { 80, "FONTB080" }, // P
        { 81, "FONTB081" }, // Q
        { 82, "FONTB082" }, // R
        { 83, "FONTB083" }, // S
        { 84, "FONTB084" }, // T
        { 85, "FONTB085" }, // U
        { 86, "FONTB086" }, // V
        { 87, "FONTB087" }, // W
        { 88, "FONTB088" }, // X
        { 89, "FONTB089" }, // Y
        { 90, "FONTB090" }, // Z
        { 97, "FONTB065" }, // a
        { 98, "FONTB066" }, // b
        { 99, "FONTB067" }, // c
        { 100, "FONTB068" }, // d
        { 101, "FONTB069" }, // e
        { 102, "FONTB070" }, // f
        { 103, "FONTB071" }, // g
        { 104, "FONTB072" }, // h
        { 105, "FONTB073" }, // i
        { 106, "FONTB074" }, // j
        { 107, "FONTB075" }, // k
        { 108, "FONTB076" }, // l
        { 109, "FONTB077" }, // m
        { 110, "FONTB078" }, // n
        { 111, "FONTB079" }, // o
        { 112, "FONTB080" }, // p
        { 113, "FONTB081" }, // q
        { 114, "FONTB082" }, // r
        { 115, "FONTB083" }, // s
        { 116, "FONTB084" }, // t
        { 117, "FONTB085" }, // u
        { 118, "FONTB086" }, // v
        { 119, "FONTB087" }, // w
        { 120, "FONTB088" }, // x
        { 121, "FONTB089" }, // y
        { 122, "FONTB090" } // z
    };
#else // __JHERETIC__ || __JHEXEN__
    // Heretic/Hexen don't use ASCII numbered font patches
    // plus they don't even have a full set eg '!' = 1 '_'= 58 !!!
    static const fontpatch_t fontA[] = {
        { 32, "FONTA00" }, // ' '
        { 33, "FONTA01" }, // !
        { 34, "FONTA02" }, // "
        { 35, "FONTA03" }, // #
        { 36, "FONTA04" }, // $
        { 37, "FONTA05" }, // %
        { 38, "FONTA06" }, // &
        { 39, "FONTA07" }, // '
        { 40, "FONTA08" }, // (
        { 41, "FONTA09" }, // )
        { 42, "FONTA10" }, // *
        { 43, "FONTA11" }, // +
        { 44, "FONTA12" }, // ,
        { 45, "FONTA13" }, // -
        { 46, "FONTA14" }, // .
        { 47, "FONTA15" }, // /
        { 48, "FONTA16" }, // 0
        { 49, "FONTA17" }, // 1
        { 50, "FONTA18" }, // 2
        { 51, "FONTA19" }, // 3
        { 52, "FONTA20" }, // 4
        { 53, "FONTA21" }, // 5
        { 54, "FONTA22" }, // 6
        { 55, "FONTA23" }, // 7
        { 56, "FONTA24" }, // 8
        { 57, "FONTA25" }, // 9
        { 58, "FONTA26" }, // :
        { 59, "FONTA27" }, // ;
        { 60, "FONTA28" }, // <
        { 61, "FONTA29" }, // =
        { 62, "FONTA30" }, // >
        { 63, "FONTA31" }, // ?
        { 64, "FONTA32" }, // @
        { 65, "FONTA33" }, // A
        { 66, "FONTA34" }, // B
        { 67, "FONTA35" }, // C
        { 68, "FONTA36" }, // D
        { 69, "FONTA37" }, // E
        { 70, "FONTA38" }, // F
        { 71, "FONTA39" }, // G
        { 72, "FONTA40" }, // H
        { 73, "FONTA41" }, // I
        { 74, "FONTA42" }, // J
        { 75, "FONTA43" }, // K
        { 76, "FONTA44" }, // L
        { 77, "FONTA45" }, // M
        { 78, "FONTA46" }, // N
        { 79, "FONTA47" }, // O
        { 80, "FONTA48" }, // P
        { 81, "FONTA49" }, // Q
        { 82, "FONTA50" }, // R
        { 83, "FONTA51" }, // S
        { 84, "FONTA52" }, // T
        { 85, "FONTA53" }, // U
        { 86, "FONTA54" }, // V
        { 87, "FONTA55" }, // W
        { 88, "FONTA56" }, // X
        { 89, "FONTA57" }, // Y
        { 90, "FONTA58" }, // Z
        { 91, "FONTA61" }, // [
        { 92, "FONTA62" }, // '\'
        { 93, "FONTA63" }, // ]
        { 94, "FONTA64" }, // ^
        { 95, "FONTA59" }, // _
        { 97, "FONTA33" }, // a
        { 98, "FONTA34" }, // b
        { 99, "FONTA35" }, // c
        { 100, "FONTA36" }, // d
        { 101, "FONTA37" }, // e
        { 102, "FONTA38" }, // f
        { 103, "FONTA39" }, // g
        { 104, "FONTA40" }, // h
        { 105, "FONTA41" }, // i
        { 106, "FONTA42" }, // j
        { 107, "FONTA43" }, // k
        { 108, "FONTA44" }, // l
        { 109, "FONTA45" }, // m
        { 110, "FONTA46" }, // n
        { 111, "FONTA47" }, // o
        { 112, "FONTA48" }, // p
        { 113, "FONTA49" }, // q
        { 114, "FONTA50" }, // r
        { 115, "FONTA51" }, // s
        { 116, "FONTA52" }, // t
        { 117, "FONTA53" }, // u
        { 118, "FONTA54" }, // v
        { 119, "FONTA55" }, // w
        { 120, "FONTA56" }, // x
        { 121, "FONTA57" }, // y
        { 122, "FONTA58" } // z
    };
    static const fontpatch_t fontB[] = {
        { 32, "FONTB00" }, // ' '
        { 33, "FONTB01" }, // !
        { 34, "FONTB02" }, // "
        { 35, "FONTB03" }, // #
        { 36, "FONTB04" }, // $
        { 37, "FONTB05" }, // %
        { 38, "FONTB06" }, // &
        { 39, "FONTB07" }, // '
        { 40, "FONTB08" }, // (
        { 41, "FONTB09" }, // )
        { 42, "FONTB10" }, // *
        { 43, "FONTB11" }, // +
        { 44, "FONTB12" }, // ,
        { 45, "FONTB13" }, // -
        { 46, "FONTB14" }, // .
        { 47, "FONTB15" }, // /
        { 48, "FONTB16" }, // 0
        { 49, "FONTB17" }, // 1
        { 50, "FONTB18" }, // 2
        { 51, "FONTB19" }, // 3
        { 52, "FONTB20" }, // 4
        { 53, "FONTB21" }, // 5
        { 54, "FONTB22" }, // 6
        { 55, "FONTB23" }, // 7
        { 56, "FONTB24" }, // 8
        { 57, "FONTB25" }, // 9
        { 58, "FONTB26" }, // :
        { 59, "FONTB27" }, // ;
        { 60, "FONTB28" }, // <
        { 61, "FONTB29" }, // =
        { 62, "FONTB30" }, // >
        { 63, "FONTB31" }, // ?
        { 64, "FONTB32" }, // @
        { 65, "FONTB33" }, // A
        { 66, "FONTB34" }, // B
        { 67, "FONTB35" }, // C
        { 68, "FONTB36" }, // D
        { 69, "FONTB37" }, // E
        { 70, "FONTB38" }, // F
        { 71, "FONTB39" }, // G
        { 72, "FONTB40" }, // H
        { 73, "FONTB41" }, // I
        { 74, "FONTB42" }, // J
        { 75, "FONTB43" }, // K
        { 76, "FONTB44" }, // L
        { 77, "FONTB45" }, // M
        { 78, "FONTB46" }, // N
        { 79, "FONTB47" }, // O
        { 80, "FONTB48" }, // P
        { 81, "FONTB49" }, // Q
        { 82, "FONTB50" }, // R
        { 83, "FONTB51" }, // S
        { 84, "FONTB52" }, // T
        { 85, "FONTB53" }, // U
        { 86, "FONTB54" }, // V
        { 87, "FONTB55" }, // W
        { 88, "FONTB56" }, // X
        { 89, "FONTB57" }, // Y
        { 90, "FONTB58" }, // Z
        { 91, "FONTB59" }, // [
        { 92, "FONTB60" }, // '\'
        { 93, "FONTB61" }, // ]
        { 94, "FONTB62" }, // ^
        { 95, "FONTB63" }, // _
        { 97, "FONTB33" }, // a
        { 98, "FONTB34" }, // b
        { 99, "FONTB35" }, // c
        { 100, "FONTB36" }, // d
        { 101, "FONTB37" }, // e
        { 102, "FONTB38" }, // f
        { 103, "FONTB39" }, // g
        { 104, "FONTB40" }, // h
        { 105, "FONTB41" }, // i
        { 106, "FONTB42" }, // j
        { 107, "FONTB43" }, // k
        { 108, "FONTB44" }, // l
        { 109, "FONTB45" }, // m
        { 110, "FONTB46" }, // n
        { 111, "FONTB47" }, // o
        { 112, "FONTB48" }, // p
        { 113, "FONTB49" }, // q
        { 114, "FONTB50" }, // r
        { 115, "FONTB51" }, // s
        { 116, "FONTB52" }, // t
        { 117, "FONTB53" }, // u
        { 118, "FONTB54" }, // v
        { 119, "FONTB55" }, // w
        { 120, "FONTB56" }, // x
        { 121, "FONTB57" }, // y
        { 122, "FONTB58" } // z
    };
#endif

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
    if(!fogEffectData.texture && !Get(DD_NOVIDEO))
    {
        fogEffectData.texture =
            GL_NewTextureWithParams3(DGL_LUMINANCE, 64, 64,
                                     W_CacheLumpName("menufog", PU_CACHE),
                                     0, DGL_NEAREST, DGL_LINEAR,
                                     -1 /*best anisotropy*/,
                                     DGL_REPEAT, DGL_REPEAT);
    }

    // Load the border patches
    for(i = 1; i < 9; ++i)
        R_CachePatch(&borderPatches[i-1], borderLumps[i]);

    // Patch used for '-' (minus) in the status bar.
#if __JDOOM__
    R_CachePatch(&huMinus, "STTMINUS");
#elif __JDOOM64__
    R_CachePatch(&huMinus, "FONTB046");
#else
    R_CachePatch(&huMinus, "FONTB13");
#endif

#if __JDOOM__ || __JDOOM64__
    for(i = 0; i < NUM_SKILL_MODES; ++i)
    {
        R_CachePatch(&skillModeNames[i], skillModePatchNames[i]);
    }

    R_CachePatch(&m_pause, "M_PAUSE");

    // Load the map name patches.
# if __JDOOM64__
    {
        int NUMCMAPS = 32;
        mapNamePatches = Z_Malloc(sizeof(dpatch_t) * NUMCMAPS, PU_STATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "WILV%2.2d", i);
            R_CachePatch(&mapNamePatches[i], name);
        }
    }
# else
    if(gameMode == commercial)
    {
        int NUMCMAPS = 32;
        mapNamePatches = Z_Malloc(sizeof(dpatch_t) * NUMCMAPS, PU_STATIC, 0);
        for(i = 0; i < NUMCMAPS; ++i)
        {
            sprintf(name, "CWILV%2.2d", i);
            R_CachePatch(&mapNamePatches[i], name);
        }
    }
    else
    {
        int                 j;

        // Don't waste space - patches are loaded back to back
        // ie no space in the array is left for E1M10
        mapNamePatches = Z_Malloc(sizeof(dpatch_t) * (9*4), PU_STATIC, 0);
        for(i = 0; i < 4; ++i) // Number of episodes.
        {
            for(j = 0; j < 9; ++j) // Number of maps per episode.
            {
                sprintf(name, "WILV%2.2d", (i * 10) + j);
                R_CachePatch(&mapNamePatches[(i* 9) + j], name);
            }
        }

        episodeNamePatches = Z_Malloc(sizeof(dpatch_t) * 4, PU_STATIC, 0);
        for(i = 0; i < 4; ++i)
            R_CachePatch(&episodeNamePatches[i], episodePatchNames[i]);
    }
# endif
#endif

    R_InitFont(GF_FONTA, fontA, sizeof(fontA) / sizeof(fontA[0]));
    R_InitFont(GF_FONTB, fontB, sizeof(fontB) / sizeof(fontB[0]));

#if __JHERETIC__ || __JHEXEN__
    for(i = 0; i < 10; ++i)
    {
        sprintf(name, "SMALLIN%d", i);
        R_CachePatch(&dpSmallNumbers[i], name);
    }
#endif

#if __JHERETIC__ || __JHEXEN__
    R_CachePatch(&dpInvItemBox, "ARTIBOX");
    R_CachePatch(&dpInvSelectBox, "SELECTBO");
    R_CachePatch(&dpInvPageLeft[0], "INVGEML1");
    R_CachePatch(&dpInvPageLeft[1], "INVGEML2");
    R_CachePatch(&dpInvPageRight[0], "INVGEMR1");
    R_CachePatch(&dpInvPageRight[1], "INVGEMR2");
#endif

    Chat_Init();
}

void Hu_UnloadData(void)
{
#if __JDOOM__ || __JDOOM64__
    if(mapNamePatches)
        Z_Free(mapNamePatches);
#endif

    if(!Get(DD_NOVIDEO))
    {
        if(fogEffectData.texture)
            DGL_DeleteTextures(1, (DGLuint*) &fogEffectData.texture);
        fogEffectData.texture = 0;
    }
}

void HU_Stop(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];
    hud->active = false;
}

void HU_Start(int player)
{
    hudstate_t*         hud;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    Chat_Start();
    Hu_LogStart(player);

    hud = &hudStates[player];
    if(hud->active)
        HU_Stop(player);

    hud->active = true;
}

void HU_Drawer(int player)
{
    // Don't draw the message log while the map title is up.
    if(!(cfg.mapTitle && actualMapTime < 6 * 35))
    {
        Hu_LogDrawer(player);
    }

    Chat_Drawer(player);
    HU_DrawScoreBoard(player);
}

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

void HU_DrawText(const char* str, gamefontid_t font, float x, float y,
                 float scale, float r, float g, float b, float a,
                 boolean alignRight)
{
    const char*         ch;
    char                c;
    float               w, h;
    dpatch_t*           p;

    if(!str || !str[0])
        return;

    if(alignRight)
    {
        ch = str;
        for(;;)
        {
            c = *ch++;

            if(!c)
                break;

			if(!gFonts[font].chars[c].patch.lump)
                continue;
			p = &gFonts[font].chars[c].patch;

            x -= p->width * scale;
        }
    }

    DGL_Color4f(r, g, b, a);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(x, y, 0);
    DGL_Scalef(scale, scale, 1);
    DGL_Translatef(-x, -y, 0);

    ch = str;
    for(;;)
    {
        c = *ch++;

        if(!c)
            break;

		if(!gFonts[font].chars[c].patch.lump)
            continue;
		p = &gFonts[font].chars[c].patch;

        w = p->width;
        h = p->height;

        GL_DrawPatch_CS(x, y, p->lump);

        x += w;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

typedef struct {
    int                 player, pClass, team;
    int                 kills, suicides;
    float               color[3];
} scoreinfo_t;

int scoreInfoCompare(const void* a, const void* b)
{
    const scoreinfo_t*  infoA = (scoreinfo_t*) a;
    const scoreinfo_t*  infoB = (scoreinfo_t*) b;

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

static int buildScoreBoard(scoreinfo_t* scoreBoard, int maxPlayers,
                           int player)
{
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
    static const float  green[3] = { 0.f,    .8f,  0.f   };
    static const float  gray[3]  = {  .45f,  .45f,  .45f };
    static const float  brown[3] = {  .7f,   .5f,   .4f  };
    static const float  red[3]   = { 1.f,   0.f,   0.f   };
#endif
    int                 i, j, n, inCount;

    memset(scoreBoard, 0, sizeof(*scoreBoard) * maxPlayers);
    inCount = 0;
    for(i = 0, n = 0; i < maxPlayers; ++i)
    {
        player_t*           plr = &players[i];
        scoreinfo_t*        info;

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
        R_GetColorPaletteRGBf(0, info->color, plrColors[info->team], false);
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
    hudstate_t*         hud;
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    hud = &hudStates[player];
    hud->scoreAlpha = 1;
    hud->scoreHideTics = 35;
}

static void drawTable(float x, float ly, float width, float height,
                      column_t* columns, scoreinfo_t* scoreBoard,
                      int inCount, float alpha, int player)
{
#define CELL_PADDING    (1)

    int                 i, n, numCols, numStretchCols;
    float               cX, cY, fixedWidth, lineHeight, fontScale, fontHeight,
                        fontOffsetY;
    float*              colX, *colW;

    if(!columns)
        return;

    if(!(alpha > 0))
        return;

    numStretchCols = 0;
    numCols = 0;
    for(n = 0; columns[n].label; n++)
    {
        numCols++;

        if(columns[n].flags & CF_HIDE)
            continue;

        if(!(columns[n].flags & CF_FIXEDWIDTH))
            numStretchCols++;
    }

    if(!numCols)
        return;

    colX = calloc(1, sizeof(*colX) * numCols);
    colW = calloc(1, sizeof(*colW) * numCols);

    lineHeight = height / (MAXPLAYERS + 1);
    fontHeight = M_StringHeight("AgIq^_", GF_FONTA);
    fontScale = (lineHeight - CELL_PADDING * 2) / fontHeight;
    fontOffsetY = 0;
    if(fontScale > 1)
    {
        fontScale = 1;
        fontOffsetY = (lineHeight - CELL_PADDING * 2 - fontHeight) / 2;
    }

    fixedWidth = 0;
    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE)
            continue;

        if(columns[n].flags & CF_FIXEDWIDTH)
        {
            colW[n] = M_StringWidth(columns[n].label, GF_FONTA) + CELL_PADDING * 2;
            fixedWidth += colW[n];
        }
    }

    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE)
            continue;

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
    for(n = 0; n < numCols; ++n)
    {
        if(columns[n].flags & CF_HIDE)
            continue;

        cX = colX[n];
        cY = ly + fontOffsetY;

        cY += CELL_PADDING;
        if(columns[n].alignRight)
            cX += colW[n] - CELL_PADDING;
        else
            cX += CELL_PADDING;

        HU_DrawText(columns[n].label, GF_FONTA, cX, cY,
                    fontScale, 1.f, 1.f, 1.f, alpha, columns[n].alignRight);
    }
    ly += lineHeight;

    // Draw the table from left to right, top to bottom:
    for(i = 0; i < inCount; ++i, ly += lineHeight)
    {
        scoreinfo_t*        info = &scoreBoard[i];
        const char*         name = Net_GetPlayerName(info->player);
        char                buf[5];

        if(info->player == player)
        {   // Draw a background to make *me* stand out.
            float               val =
                (info->color[0] + info->color[1] + info->color[2]) / 3;

            if(val < .5f)
                val = .2f;
            else
                val = .8f;

            DGL_Disable(DGL_TEXTURING);
            DGL_DrawRect(x, ly, width, lineHeight, val + .2f, val + .2f, val, .5f * alpha);
            DGL_Enable(DGL_TEXTURING);
        }

        // Now draw the fields:
        for(n = 0; n < numCols; ++n)
        {
            if(columns[n].flags & CF_HIDE)
                continue;

            cX = colX[n];
            cY = ly;

/*#if _DEBUG
DGL_Disable(DGL_TEXTURING);
GL_DrawRect(cX + CELL_PADDING, cY + CELL_PADDING,
            colW[n] - CELL_PADDING * 2,
            lineHeight - CELL_PADDING * 2,
            1, 1, 1, .1f * alpha);
DGL_Enable(DGL_TEXTURING);
#endif*/

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
                int                 spr = 0;
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
                    spriteinfo_t        sprInfo;
                    int                 w, h, w2, h2;
                    float               s, t, scale;

                    R_GetSpriteInfo(spr, 0, &sprInfo);
                    w = sprInfo.width;
                    h = sprInfo.height;
                    w2 = M_CeilPow2(w);
                    h2 = M_CeilPow2(h);
                    // Let's calculate texture coordinates.
                    // To remove a possible edge artifact, move the corner a bit up/left.
                    s = (w - 0.4f) / w2;
                    t = (h - 0.4f) / h2;

                    if(h > w)
                        scale = (lineHeight - CELL_PADDING * 2) / h;
                    else
                        scale = (colW[n] - CELL_PADDING * 2) / w;

                    w *= scale;
                    h *= scale;

                    // Align to center on both X+Y axes.
                    cX += ((colW[n] - CELL_PADDING * 2) - w) / 2;
                    cY += ((lineHeight - CELL_PADDING * 2) - h) / 2;

                    DGL_SetMaterial(sprInfo.material);

                    drawQuad(cX, cY, w, h, s, t, 1, 1, 1, alpha);
                }
#endif
                break;
                }

            case 1: // Name.
                HU_DrawText(name, GF_FONTA, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, false);
                break;

            case 2: // #Suicides.
                sprintf(buf, "%4i", info->suicides);
                HU_DrawText(buf, GF_FONTA, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, true);
                break;

            case 3: // #Kills.
                sprintf(buf, "%4i", info->kills);
                HU_DrawText(buf, GF_FONTA, cX, cY + fontOffsetY, fontScale,
                            info->color[0], info->color[1], info->color[2],
                            alpha, true);
                break;
            }
        }
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
        if(deathmatch)
            return dm;

        return coop;
    }

    return sp;
}

static void drawMapMetaData(float x, float y, gamefontid_t font, float alpha)
{
    static const char*  unnamed = "unnamed";
    const char*         lname = P_GetMapNiceName();

    if(!lname)
        lname = unnamed;

    // Map name:
    M_WriteText2(x, y + 16, "map: ", font, 1, 1, 1, alpha);
    M_WriteText2(x += M_StringWidth("map: ", font), y + 16, lname, font,
                 1, 1, 1, alpha);

    x += 8;

    // Game mode:
    M_WriteText2(x += M_StringWidth(lname, font), y + 16, "gamemode: ", font,
                 1, 1, 1, alpha);
    M_WriteText2(x += M_StringWidth("gamemode: ", font), y + 16,
                 P_GetGameModeName(), font, 1, 1, 1, alpha);
}

/**
 * Draws a sorted frags list in the lower right corner of the screen.
 */
void HU_DrawScoreBoard(int player)
{
#define LINE_BORDER		4

    column_t columns[] = {
        {"cl", 0, CF_FIXEDWIDTH, false},
        {"name", 1, 0, false},
        {"suicides", 2, CF_FIXEDWIDTH, true},
        {"frags", 3, CF_FIXEDWIDTH, true},
        {NULL, 0, 0}
    };

    int                 x, y, width, height, inCount;
    hudstate_t*         hud;
    scoreinfo_t         scoreBoard[MAXPLAYERS];

    if(!IS_NETGAME)
        return;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    hud = &hudStates[player];

    if(!(hud->scoreAlpha > 0))
        return;

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
    DGL_Disable(DGL_TEXTURING);
    DGL_DrawRect(x, y, width, height, 0, 0, 0, .4f * hud->scoreAlpha);
    DGL_Enable(DGL_TEXTURING);

    // Title:
    M_WriteText2(x + width / 2 - M_StringWidth("ranking", GF_FONTB) / 2,
                 y + LINE_BORDER, "ranking", GF_FONTB, 1, 0, 0, hud->scoreAlpha);

    drawMapMetaData(x, y + 16, GF_FONTA, hud->scoreAlpha);

    drawTable(x, y + 20, width, height - 20, columns, scoreBoard, inCount,
              hud->scoreAlpha, player);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Draws the world time in the top right corner of the screen
 */
static void drawWorldTimer(void)
{
#if __JHEXEN__
    int     days, hours, minutes, seconds;
    int     worldTimer;
    char    timeBuffer[15];
    char    dayBuffer[20];

    worldTimer = players[DISPLAYPLAYER].worldTimer;

    worldTimer /= 35;
    days = worldTimer / 86400;
    worldTimer -= days * 86400;
    hours = worldTimer / 3600;
    worldTimer -= hours * 3600;
    minutes = worldTimer / 60;
    worldTimer -= minutes * 60;
    seconds = worldTimer;

    sprintf(timeBuffer, "%.2d : %.2d : %.2d", hours, minutes, seconds);
    M_WriteText2(240, 8, timeBuffer, GF_FONTA, 1, 1, 1, 1);

    if(days)
    {
        if(days == 1)
        {
            sprintf(dayBuffer, "%.2d DAY", days);
        }
        else
        {
            sprintf(dayBuffer, "%.2d DAYS", days);
        }

        M_WriteText2(240, 20, dayBuffer, GF_FONTA, 1, 1, 1, 1);
        if(days >= 5)
        {
            M_WriteText2(230, 35, "YOU FREAK!!!", GF_FONTA, 1, 1, 1, 1);
        }
    }
#endif
}

/**
 * Handles what counters to draw eg title, timer, dm stats etc
 */
void HU_DrawMapCounters(void)
{
    player_t           *plr;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    char                buf[40], tmp[20];
    int                 x = 5, y = LINEHEIGHT_A * 3;
#endif

    plr = &players[DISPLAYPLAYER];

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    DGL_Color3f(1, 1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
#endif

    DGL_Enable(DGL_TEXTURING);

    drawWorldTimer();

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    Draw_BeginZoom(cfg.counterCheatScale, x, y);

    if(cfg.counterCheat)
    {
        // Kills.
        if(cfg.counterCheat & (CCH_KILLS | CCH_KILLS_PRCNT))
        {
            strcpy(buf, "Kills: ");
            if(cfg.counterCheat & CCH_KILLS)
            {
                sprintf(tmp, "%i/%i ", plr->killCount, totalKills);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_KILLS_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_KILLS ? "(" : ""),
                        totalKills ? plr->killCount * 100 / totalKills : 100,
                        (cfg.counterCheat & CCH_KILLS ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, GF_FONTA, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }

        // Items.
        if(cfg.counterCheat & (CCH_ITEMS | CCH_ITEMS_PRCNT))
        {
            strcpy(buf, "Items: ");
            if(cfg.counterCheat & CCH_ITEMS)
            {
                sprintf(tmp, "%i/%i ", plr->itemCount, totalItems);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_ITEMS_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_ITEMS ? "(" : ""),
                        totalItems ? plr->itemCount * 100 / totalItems : 100,
                        (cfg.counterCheat & CCH_ITEMS ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, GF_FONTA, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }

        // Secrets.
        if(cfg.counterCheat & (CCH_SECRET | CCH_SECRET_PRCNT))
        {
            strcpy(buf, "Secret: ");
            if(cfg.counterCheat & CCH_SECRET)
            {
                sprintf(tmp, "%i/%i ", plr->secretCount, totalSecret);
                strcat(buf, tmp);
            }
            if(cfg.counterCheat & CCH_SECRET_PRCNT)
            {
                sprintf(tmp, "%s%i%%%s", (cfg.counterCheat & CCH_SECRET ? "(" : ""),
                        totalSecret ? plr->secretCount * 100 / totalSecret : 100,
                        (cfg.counterCheat & CCH_SECRET ? ")" : ""));
                strcat(buf, tmp);
            }

            M_WriteText2(x, y, buf, GF_FONTA, 1, 1, 1, 1);

            y += LINEHEIGHT_A;
        }
    }

    Draw_EndZoom();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#endif
}

void Hu_Ticker(void)
{
    int                 i;

    Hu_LogTicker();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        hudstate_t*         hud = &hudStates[i];
        player_t*           plr = &players[i];

        if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
            continue;

        if(hud->scoreHideTics > 0)
        {
            hud->scoreHideTics--;
        }
        else
        {
            if(hud->scoreAlpha > 0)
                hud->scoreAlpha -= .05f;
        }
    }
}

/**
 * Updates on Game Tick.
 */
void Hu_FogEffectTicker(timespan_t time)
{
#define fog                 (&fogEffectData)
#define FOGALPHA_FADE_STEP (.07f)

    int                 i;
    static const float MENUFOGSPEED[2] = {.05f, -.085f};
    static trigger_t    fixed = { 1 / 35.0 };

    if(!M_RunTrigger(&fixed, time))
        return;

    if(cfg.hudFog == 0)
        return;

    // Move towards the target alpha
    if(fog->alpha != fog->targetAlpha)
    {
        float               diff = fog->targetAlpha - fog->alpha;

        if(fabs(diff) > FOGALPHA_FADE_STEP)
        {
            fog->alpha += FOGALPHA_FADE_STEP * (diff > 0? 1 : -1);
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
            fog->layers[i].texAngle += MENUFOGSPEED[i] / 4;
            fog->layers[i].posAngle -= MENUFOGSPEED[!i];
            fog->layers[i].texOffset[VX] =
                160 + 120 * cos(fog->layers[i].posAngle / 180 * PI);
            fog->layers[i].texOffset[VY] =
                100 + 100 * sin(fog->layers[i].posAngle / 180 * PI);
        }
        else
        {
            fog->layers[i].texAngle += MENUFOGSPEED[i] / 4;
            fog->layers[i].posAngle -= MENUFOGSPEED[!i] * 1.5f;
            fog->layers[i].texOffset[VX] =
                320 + 320 * cos(fog->layers[i].posAngle / 180 * PI);
            fog->layers[i].texOffset[VY] =
                240 + 240 * sin(fog->layers[i].posAngle / 180 * PI);
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

/**
 * Expected: <whitespace> = <whitespace> <float>
 */
float WI_ParseFloat(char** str)
{
    float               value;
    char*               end;

    *str = M_SkipWhite(*str);
    if(**str != '=')
        return 0; // Now I'm confused!

    *str = M_SkipWhite(*str + 1);
    value = strtod(*str, &end);
    *str = end;
    return value;
}

/**
 * Draw a string of text controlled by parameter blocks.
 */
void WI_DrawParamText(int x, int y, const char* inString, gamefontid_t defFont,
                      float defRed, float defGreen, float defBlue,
                      float defAlpha, boolean defCase, boolean defTypeIn,
                      int halign)
{
#define SMALLBUFF_SIZE  80

    char                smallBuff[SMALLBUFF_SIZE+1], *bigBuff = NULL;
    char                temp[256], *str, *string, *end;
    gamefontid_t        font = defFont;
    float               r = defRed, g = defGreen, b = defBlue, a = defAlpha;
    float               offX = 0, offY = 0, width = 0;
    float               scaleX = 1, scaleY = 1, angle = 0, extraScale;
    float               cx = x, cy = y;
    int                 charCount = 0;
    boolean             typeIn = defTypeIn;
    boolean             caseScale = defCase;
    struct {
        float               scale, offset;
    } caseMod[2]; // 1=upper, 0=lower
    int                 curCase = -1;
    int                 alignx = 0;
    size_t              len;

    if(!inString || !inString[0])
        return;

    len = strlen(inString);
    if(len < SMALLBUFF_SIZE)
    {
        memcpy(smallBuff, inString, len);
        smallBuff[len] = '\0';

        str = smallBuff;
    }
    else
    {
        bigBuff = malloc(len+1);
        memcpy(bigBuff, inString, len);
        bigBuff[len+1] = '\0';

        str = bigBuff;
    }

    caseMod[0].scale = 1;
    caseMod[0].offset = 3;
    caseMod[1].scale = 1.25f;
    caseMod[1].offset = 0;

    // With centrally aligned strings we need to calculate the width
    // of the whole visible string before we can draw any characters.
    // So we'll need to make two passes on the string.
    if(halign == ALIGN_CENTER)
    {
        string = str;
        while(*string)
        {
            if(*string == '{')      // Parameters included?
            {
                string++;
                while(*string && *string != '}')
                {
                    string = M_SkipWhite(string);

                    // We are only interested in font changes
                    // at this stage.
                    if(!strnicmp(string, "fonta", 5))
                    {
                        font = GF_FONTA;
                        string += 5;
                    }
                    else if(!strnicmp(string, "fontb", 5))
                    {
                        font = GF_FONTB;
                        string += 5;
                    }
                    else
                    {
                        // Unknown, skip it.
                        if(*string != '}')
                            string++;
                    }
                }

                // Skip over the closing brace.
                if(*string)
                    string++;
            }

            for(end = string; *end && *end != '{';)
            {
                // Find the end of the visible part of the string.
                for(; *end && *end != '{'; end++);

                strncpy(temp, string, end - string);
                temp[end - string] = 0;

                width += M_StringWidth(temp, font);

                string = end;       // Continue from here.
            }
        }
        width /= 2;
    }

    string = str;
    while(*string)
    {
        // Parse and draw the replacement string.
        if(*string == '{')      // Parameters included?
        {
            string++;
            while(*string && *string != '}')
            {
                string = M_SkipWhite(string);

                // What do we have here?
                if(!strnicmp(string, "fonta", 5))
                {
                    font = GF_FONTA;
                    string += 5;
                }
                else if(!strnicmp(string, "fontb", 5))
                {
                    font = GF_FONTB;
                    string += 5;
                }
                else if(!strnicmp(string, "flash", 5))
                {
                    string += 5;
                    typeIn = true;
                }
                else if(!strnicmp(string, "noflash", 7))
                {
                    string += 7;
                    typeIn = false;
                }
                else if(!strnicmp(string, "case", 4))
                {
                    string += 4;
                    caseScale = true;
                }
                else if(!strnicmp(string, "nocase", 6))
                {
                    string += 6;
                    caseScale = false;
                }
                else if(!strnicmp(string, "ups", 3))
                {
                    string += 3;
                    caseMod[1].scale = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "upo", 3))
                {
                    string += 3;
                    caseMod[1].offset = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "los", 3))
                {
                    string += 3;
                    caseMod[0].scale = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "loo", 3))
                {
                    string += 3;
                    caseMod[0].offset = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "break", 5))
                {
                    string += 5;
                    cx = x;
					cy += scaleY * gFonts[font].chars[0].patch.height;
                }
                else if(!strnicmp(string, "r", 1))
                {
                    string++;
                    r = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "g", 1))
                {
                    string++;
                    g = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "b", 1))
                {
                    string++;
                    b = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "x", 1))
                {
                    string++;
                    offX = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "y", 1))
                {
                    string++;
                    offY = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "scalex", 6))
                {
                    string += 6;
                    scaleX = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "scaley", 6))
                {
                    string += 6;
                    scaleY = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "scale", 5))
                {
                    string += 5;
                    scaleX = scaleY = WI_ParseFloat(&string);
                }
                else if(!strnicmp(string, "angle", 5))
                {
                    string += 5;
                    angle = WI_ParseFloat(&string);
                }
                else
                {
                    // Unknown, skip it.
                    if(*string != '}')
                        string++;
                }
            }

            // Skip over the closing brace.
            if(*string)
                string++;
        }

        for(end = string; *end && *end != '{';)
        {
            if(caseScale)
            {
                curCase = -1;
                // Select a substring with characters of the same case
                // (or whitespace).
                for(; *end && *end != '{'; end++)
                {
                    // We can skip whitespace.
                    if(isspace(*end))
                        continue;

                    if(curCase < 0)
                        curCase = (isupper(*end) != 0);
                    else if(curCase != (isupper(*end) != 0))
                        break;
                }
            }
            else
            {
                // Find the end of the visible part of the string.
                for(; *end && *end != '{'; end++);
            }

            strncpy(temp, string, end - string);
            temp[end - string] = 0;
            string = end;       // Continue from here.

            if(halign == ALIGN_CENTER){
                alignx = width;
            } else if (halign == ALIGN_RIGHT){
                alignx = scaleX * M_StringWidth(temp, font);
            } else {
                alignx = 0;
            }

            // Setup the scaling.
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();

            // Rotate.
            if(angle != 0)
            {
                // The origin is the specified (x,y) for the patch.
                // We'll undo the VGA aspect ratio (otherwise the result would
                // be skewed).
                DGL_Translatef(x, y, 0);
                DGL_Scalef(1, 200.0f / 240.0f, 1);
                DGL_Rotatef(angle, 0, 0, 1);
                DGL_Scalef(1, 240.0f / 200.0f, 1);
                DGL_Translatef(-x, -y, 0);
            }

            DGL_Translatef(cx + offX - alignx,
                          cy + offY +
                          (caseScale ? caseMod[curCase].offset : 0), 0);
            extraScale = (caseScale ? caseMod[curCase].scale : 1);
            DGL_Scalef(scaleX, scaleY * extraScale, 1);

            // Draw it.
            M_WriteText3(0, 0, temp, font, r, g, b, a, typeIn, false,
                         typeIn ? charCount : 0);
            charCount += strlen(temp);

            // Advance the current position.
            cx += scaleX * M_StringWidth(temp, font);

            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();
        }
    }

    // Free temporary storage.
    if(bigBuff)
        free(bigBuff);

#undef SMALLBUFF_SIZE
}

/**
 * Find string width from huFont chars
 * Skips parameter blocks eg "{param}Text" = 4 chars
 */
int M_StringWidth(const char* string, gamefontid_t font)
{
    uint                i;
    int                 w = 0, maxWidth = -1;
    int                 c;
    boolean             skip;

    for(i = 0, skip = false; i < strlen(string); ++i)
    {
        c = string[i];

        if(string[i] == '{')
            skip = true;

        if(skip == false && string[i] != '\n')
            w += gFonts[font].chars[c].patch.width;

        if(string[i] == '}')
            skip = false;

        if(string[i] == '\n')
        {
            if(w > maxWidth)
                maxWidth = w;
            w = 0;
        }
        else if(i == strlen(string) - 1 && maxWidth == -1)
        {
            maxWidth = w;
        }
    }

    return maxWidth;
}

int M_CharWidth(unsigned char ch, gamefontid_t font)
{
	return gFonts[font].chars[ch].patch.width;
}

int M_CharHeight(unsigned char ch, gamefontid_t font)
{
	return gFonts[font].chars[ch].patch.height;
}

/**
 * Find string height from huFont chars
 */
int M_StringHeight(const char* string, gamefontid_t font)
{
    uint				i;
	int					h, height = gFonts[font].chars['A'].patch.height;

    h = height;
    for(i = 0; i < strlen(string); ++i)
    {
        if(string[i] == '\n')
            h += height;
    }

    return h;
}

void M_LetterFlash(int x, int y, int w, int h, int bright, float r, float g,
                   float b, float a)
{
    float               fsize = 4 + bright, red, green, blue, alpha;
    float               fw = fsize * w / 2.0f, fh = fsize * h / 2.0f;

    // Don't draw anything for very small letters.
    if(h <= 4)
        return;

    // Don't bother with hidden letters.
    if(!(a > 0.f))
        return;

    red   = MINMAX_OF(0.f, r, 1.f);
    green = MINMAX_OF(0.f, g, 1.f);
    blue  = MINMAX_OF(0.f, b, 1.f);
    alpha = MINMAX_OF(0.f, a, 1.f);

    DGL_Bind(Get(DD_DYNLIGHT_TEXTURE));

    if(bright)
        DGL_BlendMode(BM_ADD);
    else
        DGL_BlendFunc(DGL_ZERO, DGL_ONE_MINUS_SRC_ALPHA);

    DGL_DrawRect(x + w / 2.0f - fw / 2, y + h / 2.0f - fh / 2, fw, fh, red,
                 green, blue, alpha);

    DGL_BlendMode(BM_NORMAL);
}

void M_DrawChar(int x, int y, unsigned char ch, gamefontid_t font)
{
    lumpnum_t           lump;

	lump = gFonts[font].chars[ch].patch.lump;

    GL_DrawPatch_CS(x, y, lump);
}

void HUlib_drawTextLine2(int x, int y, const char* string, size_t len,
                         gamefontid_t fontid, boolean drawCursor)
{
	const gamefont_t*	font = &gFonts[fontid];
    size_t              i;

    DGL_Color3fv(cfg.hudColor);

    for(i = 0; i < len; ++i)
    {
        unsigned char		c = string[i];
        int					w;
		const dpatch_t*     p = &font->chars[c].patch;

		w = p->width;
        if(x + w > SCREENWIDTH)
            break;

		GL_DrawPatch_CS(x, y, p->lump);
        x += w;
    }

    // Draw the cursor if requested.
	if(drawCursor &&
       x + font->chars['_'].patch.width <= SCREENWIDTH)
		GL_DrawPatch_CS(x, y, font->chars['_'].patch.lump);
}

#if __JHERETIC__
void HU_DrawBNumber(signed int val, int x, int y, float red,
                    float green, float blue, float alpha)
{
    const dpatch_t*     patch;
    int                 xpos;
    int                 oldval;

    oldval = val;
    xpos = x;
    if(val < 0)
    {
        val = 0;
    }

    if(val > 99)
    {
        patch = &gFonts[GF_FONTB].chars['0' + val / 100].patch;

        GL_DrawPatchLitAlpha(xpos + 8 - patch->width / 2, y +2, 0, alpha * .4f,
                             patch->lump);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - patch->width / 2, y, patch->lump);
        DGL_Color4f(1, 1, 1, 1);
    }

    val = val % 100;
    xpos += 12;
    if(val > 9 || oldval > 99)
    {
        patch = &gFonts[GF_FONTB].chars['0' + val / 10].patch;

        GL_DrawPatchLitAlpha(xpos + 8 - patch->width / 2, y +2, 0, alpha * .4f,
                             patch->lump);
        DGL_Color4f(red, green, blue, alpha);
        GL_DrawPatch_CS(xpos + 6 - patch->width / 2, y, patch->lump);
        DGL_Color4f(1, 1, 1, 1);
    }

    val = val % 10;
    xpos += 12;
    patch = &gFonts[GF_FONTB].chars['0' + val].patch;

    GL_DrawPatchLitAlpha(xpos + 8 - patch->width / 2, y +2, 0, alpha * .4f,
                         patch->lump);
    DGL_Color4f(red, green, blue, alpha);
    GL_DrawPatch_CS(xpos + 6 - patch->width / 2, y, patch->lump);
    DGL_Color4f(1, 1, 1, 1);
}
#endif

#if __JHERETIC__
void IN_DrawShadowChar(int x, int y, unsigned char ch, gamefontid_t font)
{
    GL_DrawPatchLitAlpha(x+2, y+2, 0, .4f,
                         gFonts[font].chars[ch].patch.lump);
    DGL_Color4f(defFontRGB[0], defFontRGB[1], defFontRGB[2], 1);
    M_DrawChar(x, y, ch, font);
}
#elif __JHEXEN__
void GL_DrawShadowedPatch2(float x, float y, float r, float g, float b,
                           float a, lumpnum_t lump)
{
    GL_DrawPatchLitAlpha(x + 2, y + 2, 0, a * .4f, lump);
    DGL_Color4f(r, g, b, a);
    GL_DrawPatch_CS(x, y, lump);
    DGL_Color4f(1, 1, 1, 1);
}
#endif

#if __JHERETIC__
void IN_DrawNumber(int val, int x, int y, int digits, float r, float g,
                   float b, float a)
{
    int                 xpos;
    int                 oldval;
    int                 realdigits;
    boolean             neg;

    oldval = val;
    xpos = x;
    neg = false;
    realdigits = 1;

    if(val < 0)
    {
        val = -val;
        neg = true;
        if(val > 99)
        {
            val = 99;
        }
    }

    if(val > 9)
    {
        realdigits++;
        if(digits < realdigits)
        {
            realdigits = digits;
            val = 9;
        }
    }

    if(val > 99)
    {
        realdigits++;
        if(digits < realdigits)
        {
            realdigits = digits;
            val = 99;
        }
    }

    if(val > 999)
    {
        realdigits++;
        if(digits < realdigits)
        {
            realdigits = digits;
            val = 999;
        }
    }

    if(digits == 4)
    {
		GL_DrawPatchLitAlpha(xpos + 8 - gFonts[GF_FONTB].chars['0' + val / 1000].patch.width / 2 - 12, y + 2, 0, .4f, gFonts[GF_FONTB].chars['0' + val / 1000].patch.lump);
        DGL_Color4f(r, g, b, a);
        GL_DrawPatch_CS(xpos + 6 - gFonts[GF_FONTB].chars['0' + val / 1000].patch.width / 2 - 12, y, gFonts[GF_FONTB].chars['0' + val / 1000].patch.lump);
    }

    if(digits > 2)
    {
        if(realdigits > 2)
        {
            GL_DrawPatchLitAlpha(xpos + 8 - gFonts[GF_FONTB].chars['0' + val / 100].patch.width / 2, y+2, 0, .4f, gFonts[GF_FONTB].chars['0' + val / 100].patch.lump);
            DGL_Color4f(r, g, b, a);
            GL_DrawPatch_CS(xpos + 6 - gFonts[GF_FONTB].chars['0' + val / 100].patch.width / 2, y, gFonts[GF_FONTB].chars['0' + val / 100].patch.lump);
        }
        xpos += 12;
    }

    val = val % 100;
    if(digits > 1)
    {
        if(val > 9)
        {
            GL_DrawPatchLitAlpha(xpos + 8 - gFonts[GF_FONTB].chars['0' + val / 10].patch.width / 2, y+2, 0, .4f, gFonts[GF_FONTB].chars['0' + val / 10].patch.lump);
            DGL_Color4f(r, g, b, a);
            GL_DrawPatch_CS(xpos + 6 - gFonts[GF_FONTB].chars['0' + val / 10].patch.width / 2, y, gFonts[GF_FONTB].chars['0' + val / 10].patch.lump);
        }
        else if(digits == 2 || oldval > 99)
        {
            GL_DrawPatchLitAlpha(xpos+2, y+2, 0, .4f, gFonts[GF_FONTB].chars['0'].patch.lump);
            DGL_Color4f(r, g, b, a);
            GL_DrawPatch_CS(xpos, y, gFonts[GF_FONTB].chars['0'].patch.lump);
        }
        xpos += 12;
    }

    val = val % 10;
    GL_DrawPatchLitAlpha(xpos + 8 - gFonts[GF_FONTB].chars['0' + val].patch.width / 2, y+2, 0, .4f, gFonts[GF_FONTB].chars['0' + val].patch.lump);
    DGL_Color4f(r, g, b, a);
    GL_DrawPatch_CS(xpos + 6 - gFonts[GF_FONTB].chars['0' + val].patch.width / 2, y, gFonts[GF_FONTB].chars['0' + val].patch.lump);
    if(neg)
    {
        GL_DrawPatchLitAlpha(xpos + 8 - gFonts[GF_FONTB].chars['-'].patch.width / 2 - 12 * (realdigits), y+2, 0, .4f, gFonts[GF_FONTB].chars['-'].patch.lump);
        DGL_Color4f(r, g, b, a);
        GL_DrawPatch_CS(xpos + 6 - gFonts[GF_FONTB].chars['-'].patch.width / 2 - 12 * (realdigits), y, gFonts[GF_FONTB].chars['-'].patch.lump);
    }
}
#endif

#if __JHEXEN__
/**
 * Draws a three digit number using FontB
 */
void DrBNumber(int val, int x, int y, float red, float green, float blue,
               float alpha)
{
    dpatch_t*           patch;
    int                 xpos;
    int                 oldval;

    // Limit to three digits.
    if(val > 999)
        val = 999;
    if(val < -999)
        val = -999;

    oldval = val;
    xpos = x;
    if(val < 0)
    {
        val = 0;
    }

    if(val > 99)
    {
        patch = &gFonts[GF_FONTB].chars['0' + val / 100].patch;
        GL_DrawShadowedPatch2(xpos + 6 - patch->width / 2, y, red, green,
                              blue, alpha, patch->lump);
    }

    val = val % 100;
    xpos += 12;
    if(val > 9 || oldval > 99)
    {
        patch = &gFonts[GF_FONTB].chars['0' + val / 10].patch;
        GL_DrawShadowedPatch2(xpos + 6 - patch->width / 2, y, red, green,
                              blue, alpha, patch->lump);
    }

    val = val % 10;
    xpos += 12;
    patch = &gFonts[GF_FONTB].chars['0' + val].patch;
    GL_DrawShadowedPatch2(xpos + 6 - patch->width / 2, y, red, green, blue,
                          alpha, patch->lump);
}
#endif

/**
 * Write a string using the huFont.
 */
void M_WriteText(int x, int y, const char* string)
{
    M_WriteText2(x, y, string, GF_FONTA, 1, 1, 1, 1);
}

void M_WriteText2(int x, int y, const char* string, gamefontid_t font,
                  float red, float green, float blue, float alpha)
{
    M_WriteText3(x, y, string, font, red, green, blue, alpha, false, true, 0);
}

/**
 * Write a string using a colored, custom font.
 * Also do a type-in effect.
 */
void M_WriteText3(int x, int y, const char* string, gamefontid_t font,
                  float red, float green, float blue, float alpha,
                  boolean flagTypeIn, boolean flagShadow, int initialCount)
{
    const char*         ch;
    unsigned char       c;
    int                 pass, w, h, cx, cy, count, yoff;
    float               flash, flashColor[4];

    flashColor[CR] = (red >= 0?   ((1 + 2 * MINMAX_OF(0, red,   1)) / 3) : 1);
    flashColor[CB] = (blue >= 0?  ((1 + 2 * MINMAX_OF(0, blue,  1)) / 3) : 1);
    flashColor[CG] = (green >= 0? ((1 + 2 * MINMAX_OF(0, green, 1)) / 3) : 1);
    flashColor[CA] = cfg.menuGlitter * (alpha>=0? MINMAX_OF(0, alpha, 1) : 1);

    for(pass = (flagShadow? 0 : 1); pass < 2; ++pass)
    {
        count = initialCount;

        if(red >= 0)
            DGL_Color4f(red, green, blue, alpha);

        ch = string;
        cx = x;
        cy = y;

        for(;;)
        {
            c = *ch++;
            yoff = 0;
            flash = 0;
            // Do the type-in effect?
            if(flagTypeIn && cfg.menuEffects != 0)
            {
                int                 maxCount =
                    (typeInTime > 0? typeInTime * 2 : 0);

                if(count == maxCount)
                {
                    flash = 1;
                    if(red >= 0)
                        DGL_Color4f(1, 1, 1, 1);
                }
                else if(count + 1 == maxCount)
                {
                    flash = 0.5f;
                    if(red >= 0)
                        DGL_Color4f((1 + red) / 2, (1 + green) / 2, (1 + blue) / 2,
                                   alpha);
                }
                else if(count + 2 == maxCount)
                {
                    flash = 0.25f;
                    if(red >= 0)
                        DGL_Color4f(red, green, blue, alpha);
                }
                else if(count + 3 == maxCount)
                {
                    flash = 0.12f;
                    if(red >= 0)
                        DGL_Color4f(red, green, blue, alpha);
                }
                else if(count > maxCount)
                {
                    break;
                }
            }
            count++;

            if(!c)
                break;

            if(c == '\n')
            {
                cx = x;
                cy += 12;
                continue;
            }

			w = gFonts[font].chars[c].patch.width;
			h = gFonts[font].chars[c].patch.height;

			if(!gFonts[font].chars[c].patch.lump)
            {
                // A character we don't have a patch for...?!
                continue;
            }

            if(pass)
            {
                // The character itself.
				GL_DrawPatch_CS(cx, cy + yoff, gFonts[font].chars[c].patch.lump);

                // Do something flashy!
                if(flash > 0)
                {
                    M_LetterFlash(cx, cy + yoff, w, h, true,
                                  flashColor[CR], flashColor[CG],
                                  flashColor[CB], flashColor[CA] * flash);
                }
            }
            else if(flagShadow && cfg.menuShadow > 0)
            {
                // Shadow.
                M_LetterFlash(cx, cy + yoff, w, h, false, 1, 1, 1,
                              (red <
                               0 ? DGL_GetInteger(DGL_CURRENT_COLOR_A) /
                               255.0f : alpha) * cfg.menuShadow);
            }

            cx += w;
        }
    }
}

#if __JHERETIC__ || __JHEXEN__
void Hu_DrawSmallNum(int val, int numDigits, int x, int y, float alpha)
{
    int                 w = dpSmallNumbers[0].width;
    boolean             drawMinus = false;

    if(val < 0)
    {
        if(numDigits == 2 && val < -9)
            val = -9;
        else if(numDigits == 3 && val < -99)
            val = -99;
        val = -val;
        drawMinus = true;
    }

    // In the special case of 0, you draw 0.
    if(val == 0)
        WI_DrawPatch(x - w, y, 1, 1, 1, alpha, &dpSmallNumbers[0], NULL,
                     false, ALIGN_LEFT);

    // Draw the number.
    while(val && numDigits--)
    {
        x -= w;
        WI_DrawPatch(x, y, 1, 1, 1, alpha, &dpSmallNumbers[val % 10],
                     NULL, false, ALIGN_LEFT);
        val /= 10;
    }

    // Draw a minus sign if necessary.
    if(drawMinus)
        WI_DrawPatch(x - 8, y, 1, 1, 1, alpha, &huMinus, NULL, false,
                     ALIGN_LEFT);
}
#endif

/**
 * This routine tests for a string-replacement for the patch.
 * If one is found, it's used instead of the original graphic.
 *
 * "{fontb; r=0.5; g=1; b=0; x=2; y=-2}This is good!"
 *
 * If the patch is not in an IWAD, it won't be replaced!
 *
 * @param altstring     String to use instead of the patch if appropriate.
 * @param built-in      (True) if the altstring is a built-in replacement
 *                      (ie it does not originate from a DED definition).
 */
void WI_DrawPatch(int x, int y, float r, float g, float b, float a,
                  const dpatch_t* patch, const char* altstring,
                  boolean builtin, int halign)
{
    char                def[80], *string;
    int                 patchString = 0;
    int                 posx = x;

    if(IS_DEDICATED)
        return;

    if(!patch)
        return;

    if(altstring && !builtin)
    {   // We have already determined a string to replace this with.
        if(W_IsFromIWAD(patch->lump))
        {
            WI_DrawParamText(x, y, altstring, GF_FONTB, r, g, b, a, false,
                             true, halign);
            return;
        }
    }
    else if(cfg.usePatchReplacement)
    {   // We might be able to replace the patch with a string
        if(!patch)
            return;

        strcpy(def, "Patch Replacement|");
        strcat(def, W_LumpName(patch->lump));

        patchString = Def_Get(DD_DEF_VALUE, def, &string);

        if(W_IsFromIWAD(patch->lump))
        {
            // A user replacement?
            if(patchString)
            {
                WI_DrawParamText(x, y, string, GF_FONTB, r, g, b, a, false,
                                 true, halign);
                return;
            }

            // A built-in replacement?
            if(cfg.usePatchReplacement == 2 && altstring)
            {
                WI_DrawParamText(x, y, altstring, GF_FONTB, r, g, b, a, false,
                                 true, halign);
                return;
            }
        }
    }

    if(!patch)
        return;

    // No replacement possible/wanted - use the original patch.
    if(halign == ALIGN_CENTER)
        posx -= patch->width /2;
    else if(halign == ALIGN_RIGHT)
        posx -= patch->width;

    DGL_Color4f(1, 1, 1, a);
    GL_DrawPatch_CS(posx, y, patch->lump);
}

/**
 * Draws a box using the border patches, a border is drawn outside.
 */
void M_DrawBackgroundBox(float x, float y, float w, float h, float red,
                         float green, float blue, float alpha,
                         boolean background, int border)
{
    dpatch_t*           t = 0, *b = 0, *l = 0, *r = 0, *tl = 0, *tr = 0, *br = 0, *bl = 0;
    int                 up = -1;

    switch(border)
    {
    case BORDERUP:
        t = &borderPatches[2];
        b = &borderPatches[0];
        l = &borderPatches[1];
        r = &borderPatches[3];
        tl = &borderPatches[6];
        tr = &borderPatches[7];
        br = &borderPatches[4];
        bl = &borderPatches[5];

        up = -1;
        break;

    case BORDERDOWN:
        t = &borderPatches[0];
        b = &borderPatches[2];
        l = &borderPatches[3];
        r = &borderPatches[1];
        tl = &borderPatches[4];
        tr = &borderPatches[5];
        br = &borderPatches[6];
        bl = &borderPatches[7];

        up = 1;
        break;

    default:
        break;
    }

    DGL_Color4f(red, green, blue, alpha);

    if(background)
    {
        DGL_SetMaterial(P_ToPtr(DMU_MATERIAL,
            P_MaterialNumForName(borderLumps[0], MN_FLATS)));
        DGL_DrawRectTiled(x, y, w, h, 64, 64);
    }

    if(border)
    {
        // Top
        DGL_SetPatch(t->lump, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x, y - t->height, w, t->height,
                          up * t->width, up * t->height);
        // Bottom
        DGL_SetPatch(b->lump, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x, y + h, w, b->height, up * b->width,
                          up * b->height);
        // Left
        DGL_SetPatch(l->lump, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x - l->width, y, l->width, h,
                          up * l->width, up * l->height);
        // Right
        DGL_SetPatch(r->lump, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x + w, y, r->width, h, up * r->width,
                          up * r->height);

        // Top Left
        DGL_SetPatch(tl->lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x - tl->width, y - tl->height,
                     tl->width, tl->height,
                     red, green, blue, alpha);
        // Top Right
        DGL_SetPatch(tr->lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x + w, y - tr->height, tr->width, tr->height,
                     red, green, blue, alpha);
        // Bottom Right
        DGL_SetPatch(br->lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x + w, y + h, br->width, br->height,
                     red, green, blue, alpha);
        // Bottom Left
        DGL_SetPatch(bl->lump, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect(x - bl->width, y + h, bl->width, bl->height,
                     red, green, blue, alpha);
    }
}

/**
 * Draws a menu slider control
 */
#if __JHERETIC__ || __JHEXEN__
void M_DrawSlider(int x, int y, int width, int slot, float alpha)
#else
void M_DrawSlider(int x, int y, int width, int height, int slot, float alpha)
#endif
{
#if __JHERETIC__ || __JHEXEN__
    float               unit = (width * 8 + 2) / width;

    DGL_Color4f( 1, 1, 1, alpha);

    GL_DrawPatch_CS(x - 32, y, W_GetNumForName("M_SLDLT"));
    GL_DrawPatch_CS(x + width * 8, y, W_GetNumForName("M_SLDRT"));

    DGL_SetPatch(W_GetNumForName("M_SLDMD1"), DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectTiled(x - 1, y + 1, width * 8 + 2, 13, 8, 13);

    DGL_Color4f( 1, 1, 1, alpha);
    GL_DrawPatch_CS(x + 4 + slot * unit, y + 7, W_GetNumForName("M_SLDKB"));
#else
    int         xx;
    float       scale = height / 13.0f;

    xx = x;
    DGL_SetPatch(W_GetNumForName("M_THERML"), DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, alpha);
    xx += 6 * scale;
    DGL_SetPatch(W_GetNumForName("M_THERM2"), DGL_REPEAT, DGL_CLAMP_TO_EDGE);
    DGL_DrawRectTiled(xx, y, 8 * width * scale, height, 8 * scale, height);
    xx += 8 * width * scale;
    DGL_SetPatch(W_GetNumForName("M_THERMR"), DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_DrawRect(xx, y, 6 * scale, height, 1, 1, 1, alpha);

    DGL_SetPatch(W_GetNumForName("M_THERMO"), DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_DrawRect(x + (6 + slot * 8) * scale, y, 6 * scale, height, 1, 1, 1,
                 alpha);
#endif
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
        DGL_DrawRect(0, 0, 320, 200, 0.0f, 0.0f, 0.0f, MIN_OF(alpha, .5f));
        return;
    }

    if(effectID == 2)
    {
        DGL_Disable(DGL_TEXTURING);
        DGL_Color4f(alpha, alpha / 2, 0, alpha / 3);
        DGL_BlendMode(BM_INVERSE_MUL);
        DGL_DrawRectTiled(0, 0, 320, 200, 1, 1);
        DGL_Enable(DGL_TEXTURING);
    }

    DGL_Bind(tex);
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
            DGL_DrawRectTiled(0, 0, 320, 200, 270 / 8, 4 * 225);
        else if(effectID == 0)
            DGL_DrawRectTiled(0, 0, 320, 200, 270 / 4, 8 * 225);
        else
            DGL_DrawRectTiled(0, 0, 320, 200, 270, 225);
    }

    DGL_MatrixMode(DGL_TEXTURE);
    DGL_PopMatrix();

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
    if((Hu_MenuIsActive() || Hu_MenuAlpha() > 0) || Hu_IsMessageActive())
    {
        // Use a plain 320x200 projection.
        DGL_MatrixMode(DGL_PROJECTION);
        DGL_LoadIdentity();
        DGL_PushMatrix();
        DGL_Ortho(0, 0, 320, 200, -1, 1);

        // Draw the fog effect?
        if(fogEffectData.alpha > 0 && cfg.hudFog &&
           !((Hu_MenuIsActive() || Hu_MenuAlpha() > 0) &&
              MN_CurrentMenuHasBackground()))
            drawFogEffect();

        if(Hu_IsMessageActive())
            Hu_MsgDrawer();
        else
            Hu_MenuDrawer();

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PopMatrix();
    }
}

void Hu_FogEffectSetAlphaTarget(float alpha)
{
    fogEffectData.targetAlpha = MINMAX_OF(0, alpha, 1);
}
