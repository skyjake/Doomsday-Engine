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
 * dd_compositefont.h:
 */

#ifndef LIBDENG_COMPOSITEFONT_H
#define LIBDENG_COMPOSITEFONT_H

typedef uint32_t compositefontid_t;

// Used during font creation/registration.
// \todo Refactor me away.
typedef struct fontpatch_s {
    byte        ch;
    char        lumpName[9];
} fontpatch_t;

void            R_NewCompositeFont(compositefontid_t fontid, const char* name, const fontpatch_t* patches, size_t num);
compositefontid_t R_CompositeFontNumForName(const char* name);
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
void            GL_DrawText(const char* string, int x, int y, compositefontid_t font, short flags, int defTracking, float defRed, float defGreen, float defBlue, float defAlpha, float defGlitter, float defShadow, boolean defCase);

// Utility routines:
void            GL_TextDimensions(int* width, int* height, const char* string, compositefontid_t font);
int             GL_TextWidth(const char* string, compositefontid_t font);
int             GL_TextHeight(const char* string, compositefontid_t font);

/**
 * Text string fragments: A single line of unformatted text.
 */
void            GL_DrawTextFragment(const char* string, int x, int y);
void            GL_DrawTextFragment2(const char* string, int x, int y, compositefontid_t font);
void            GL_DrawTextFragment3(const char* string, int x, int y, compositefontid_t font, short flags);
void            GL_DrawTextFragment4(const char* string, int x, int y, compositefontid_t font, short flags, int tracking);
void            GL_DrawTextFragment5(const char* string, int x, int y, compositefontid_t font, short flags, int tracking, int initialCount);
void            GL_DrawTextFragment6(const char* string, int x, int y, compositefontid_t font, short flags, int tracking, int initialCount, float glitterStrength);
void            GL_DrawTextFragment7(const char* string, int x, int y, compositefontid_t font, short flags, int tracking, int initialCount, float glitterStrength, float shadowStrength);

// Utility routines:
void            GL_TextFragmentDimensions(int* width, int* height, const char* string, compositefontid_t font);
void            GL_TextFragmentDimensions2(int* width, int* height, const char* string, compositefontid_t font, int tracking);

int             GL_TextFragmentWidth(const char* string, compositefontid_t font);
int             GL_TextFragmentWidth2(const char* string, compositefontid_t font, int tracking);
int             GL_TextFragmentHeight(const char* string, compositefontid_t font);

/**
 * Single character.
 */
void            GL_DrawChar(unsigned char ch, int x, int y);
void            GL_DrawChar2(unsigned char ch, int x, int y, compositefontid_t font);
void            GL_DrawChar3(unsigned char ch, int x, int y, compositefontid_t font, short flags);

// Utility routines:
void            GL_CharDimensions(int* width, int* height, unsigned char ch, compositefontid_t font);
int             GL_CharWidth(unsigned char ch, compositefontid_t font);
int             GL_CharHeight(unsigned char ch, compositefontid_t font);

#endif /* LIBDENG_COMPOSITEFONT_H */
