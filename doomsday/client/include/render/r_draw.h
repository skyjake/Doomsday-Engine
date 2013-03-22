/** @file r_draw.h Misc Drawing Routines
 * @ingroup render
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_system.h"
#include "Texture"

void R_InitViewWindow();
void R_ShutdownViewWindow();

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder();

texturevariantspecification_t &Rend_PatchTextureSpec(int flags = 0,
    int wrapS = GL_CLAMP_TO_EDGE, int wrapT = GL_CLAMP_TO_EDGE);

void R_DrawPatch(de::Texture &texture, int x, int y);
void R_DrawPatch(de::Texture &texture, int x, int y, int w, int h, bool useOffsets = true);

void R_DrawPatchTiled(de::Texture &texture, int x, int y, int w, int h, int wrapS, int wrapT);

#endif // LIBDENG_RENDER_MISC_H
