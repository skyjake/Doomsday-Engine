/** @file r_draw.h Misc  Drawing routines.
 *
 * @ingroup render
 *
 * @author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_MISC_H
#define DENG_CLIENT_RENDER_MISC_H

#include "Texture"
#include "TextureVariantSpec"
#include <de/GLTexture>

void R_InitViewWindow();
void R_ShutdownViewWindow();

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder();

TextureVariantSpec const &Rend_PatchTextureSpec(int flags = 0,
    de::gl::Wrapping wrapS = de::gl::ClampToEdge, de::gl::Wrapping wrapT = de::gl::ClampToEdge);

void R_DrawPatch(de::Texture &texture, int x, int y);
void R_DrawPatch(de::Texture &texture, int x, int y, int w, int h, bool useOffsets = true);

void R_DrawPatchTiled(de::Texture &texture, int x, int y, int w, int h,
    de::gl::Wrapping wrapS, de::gl::Wrapping wrapT);

#endif // DENG_CLIENT_RENDER_MISC_H
