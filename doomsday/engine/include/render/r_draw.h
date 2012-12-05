/**
 * @file r_draw.h Misc Drawing Routines
 * @ingroup render
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_MISC_H
#define LIBDENG_RENDER_MISC_H

#include "resource/texture.h"

#ifdef __cplusplus
extern "C" {
#endif

void R_InitViewWindow(void);
void R_ShutdownViewWindow(void);

void R_SetBorderGfx(Uri const *const *paths);

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder(void);

void R_DrawPatch(Texture *texture, int x, int y);
void R_DrawPatch2(Texture *texture, int x, int y, int w, int h);
void R_DrawPatch3(Texture *texture, int x, int y, int w, int h, boolean useOffsets);

void R_DrawPatchTiled(Texture *texture, int x, int y, int w, int h, int wrapS, int wrapT);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_MISC_H */
