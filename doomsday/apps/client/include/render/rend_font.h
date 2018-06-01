/** @file rend_font.h
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Font Renderer
 */

#ifndef DE_FONT_RENDERER
#define DE_FONT_RENDERER

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_INITIALCOUNT        (0) ///< Used for animating type-in effects.

#define DEFAULT_ALIGNFLAGS          (ALIGN_TOPLEFT)
#define DEFAULT_DRAWFLAGS           (DTF_NO_EFFECTS)

#define FR_FORMAT_ESCAPE_CHAR       ((char)0x10) ///< ASCII data link escape

/**
 * Rendering formatted text requires a temporary working buffer in order
 * to split up the larger blocks into individual text fragments.
 *
 * A "small text" buffer is reserved from the application's data segment
 * for the express purpose of manipulating shortish text strings without
 * incurring any dynamic allocation overhead. This value defines the size
 * of this fixed-size working buffer as the number of characters it may
 * potentially hold.
 */
#define FR_SMALL_TEXT_BUFFER_SIZE   (160)

/// Initialize this module.
void FR_Init(void);

/// Shutdown this module.
void FR_Shutdown(void);

/// @return  @c true= Font renderer has been initialized and is available.
dd_bool FR_Available(void);

void FR_Ticker(timespan_t ticLength);

void FR_SetNoFont(void);

// Utility routines:
int FR_SingleLineHeight(const char* text);
int FR_GlyphTopToAscent(const char* text);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_FONT_RENDERER */
