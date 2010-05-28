/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * gl_drawcompositefont.h:
 */

#ifndef LIBCOMMON_GRAPHICS_DRAW_COMPOSITEFONT_H
#define LIBCOMMON_GRAPHICS_DRAW_COMPOSITEFONT_H

#include "doomsday.h"

// The fonts.
typedef enum {
    GF_FIRST = 0,
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

// Used during font creation/registration.
// \todo Refactor me away.
typedef struct fontpatch_s {
    byte        ch;
    char        lumpName[9];
} fontpatch_t;

void            R_InitFont(gamefontid_t fontid, const fontpatch_t* patches, size_t num);

void            R_TextTicker(timespan_t ticLength);
void            R_ResetTextTypeInTimer(void);

/**
 * @defGroup drawTextFlags Draw Text Flags
 */
/*@{*/
#define DTF_ALIGN_LEFT      0x0001
#define DTF_ALIGN_RIGHT     0x0002
#define DTF_ALIGN_BOTTOM    0x0004
#define DTF_ALIGN_TOP       0x0008
#define DTF_NO_TYPEIN       0x0010

#define DTF_NO_EFFECTS      (DTF_NO_TYPEIN)
#define DTF_ALIGN_TOPLEFT   (DTF_ALIGN_TOP|DTF_ALIGN_LEFT)
#define DTF_ALIGN_BOTTOMLEFT (DTF_ALIGN_BOTTOM|DTF_ALIGN_LEFT)
#define DTF_ALIGN_TOPRIGHT  (DTF_ALIGN_TOP|DTF_ALIGN_RIGHT)
#define DTF_ALIGN_BOTTOMRIGHT (DTF_ALIGN_BOTTOM|DTF_ALIGN_RIGHT)
/*@}*/

/**
 * Text strings: A block of possibly formatted and/or multi-line text.
 */
void            GL_DrawText(const char* string, int x, int y, gamefontid_t font, short flags, int defTracking, float defRed, float defGreen, float defBlue, float defAlpha, float defGlitter, float defShadow, boolean defCase);

// Utility routines:
void            GL_TextDimensions(int* width, int* height, const char* string, gamefontid_t font);
int             GL_TextWidth(const char* string, gamefontid_t font);
int             GL_TextHeight(const char* string, gamefontid_t font);

/**
 * Text string fragments: A single line of unformatted text.
 */
void            GL_DrawTextFragment(const char* string, int x, int y);
void            GL_DrawTextFragment2(const char* string, int x, int y, gamefontid_t font);
void            GL_DrawTextFragment3(const char* string, int x, int y, gamefontid_t font, short flags);
void            GL_DrawTextFragment4(const char* string, int x, int y, gamefontid_t font, short flags, int tracking);
void            GL_DrawTextFragment5(const char* string, int x, int y, gamefontid_t font, short flags, int tracking, int initialCount);
void            GL_DrawTextFragment6(const char* string, int x, int y, gamefontid_t font, short flags, int tracking, int initialCount, float glitterStrength);
void            GL_DrawTextFragment7(const char* string, int x, int y, gamefontid_t font, short flags, int tracking, int initialCount, float glitterStrength, float shadowStrength);

// Utility routines:
void            GL_TextFragmentDimensions(int* width, int* height, const char* string, gamefontid_t font);
void            GL_TextFragmentDimensions2(int* width, int* height, const char* string, gamefontid_t font, int tracking);

int             GL_TextFragmentWidth(const char* string, gamefontid_t font);
int             GL_TextFragmentWidth2(const char* string, gamefontid_t font, int tracking);
int             GL_TextFragmentHeight(const char* string, gamefontid_t font);

/**
 * Single character.
 */
void            GL_DrawChar(unsigned char ch, int x, int y);
void            GL_DrawChar2(unsigned char ch, int x, int y, gamefontid_t font);
void            GL_DrawChar3(unsigned char ch, int x, int y, gamefontid_t font, short flags);

// Utility routines:
void            GL_CharDimensions(int* width, int* height, unsigned char ch, gamefontid_t font);
int             GL_CharWidth(unsigned char ch, gamefontid_t font);
int             GL_CharHeight(unsigned char ch, gamefontid_t font);

#endif /* LIBCOMMON_GRAPHICS_DRAW_COMPOSITEFONT_H */
