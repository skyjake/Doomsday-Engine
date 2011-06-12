/**\file gl_font.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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
 * Font Management.
 */

#ifndef LIBDENG_GL_FONT_H
#define LIBDENG_GL_FONT_H

#include "bitmapfont.h"

#define DEFAULT_LEADING             (.5)
#define DEFAULT_TRACKING            (0)
#define DEFAULT_INITIALCOUNT        (0) /// Used for animating type-in effects.
#define DEFAULT_GLITTER_STRENGTH    (.5)
#define DEFAULT_SHADOW_STRENGTH     (.5)
#define DEFAULT_SHADOW_XOFFSET      (2)
#define DEFAULT_SHADOW_YOFFSET      (2)

#define DEFAULT_ALIGNFLAGS          (ALIGN_TOPLEFT)
#define DEFAULT_DRAWFLAGS           (DTF_NO_EFFECTS)

/**
 * Initialize the font renderer.
 * @return  @c 0, iff there are no errors.
 */
int FR_Init(void);
void FR_Shutdown(void);

void FR_Ticker(timespan_t ticLength);

/**
 * Mark all fonts as requiring a full update. Called during engine/renderer reset.
 */
void FR_Update(void);

/**
 * Load the specified font as a "system font".
 */
fontid_t FR_LoadSystemFont(const char* name, const char* path);

fontid_t FR_CreateFontFromDef(ded_compositefont_t* def);

/**
 * @return  Ptr to the font associated with the specified id.
 */
bitmapfont_t* FR_BitmapFontForId(fontid_t id);
void FR_DestroyFont(fontid_t id);

// Utility routines:
int FR_SingleLineHeight(const char* text);
int FR_GlyphTopToAscent(const char* text);

ddstring_t** FR_CollectFontNames(int* count);

#endif /* LIBDENG_GL_FONT_H */
