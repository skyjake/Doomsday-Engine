/** @file r_draw.h Misc  Drawing routines.
 *
 * @ingroup render
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_RENDER_MISC_H
#define DE_CLIENT_RENDER_MISC_H

#include "resource/clienttexture.h"
#include "resource/texturevariantspec.h"
#include <de/gltexture.h>

void R_InitViewWindow();
void R_ShutdownViewWindow();

/**
 * Draws the border around the view for different size windows.
 */
void R_DrawViewBorder();

const TextureVariantSpec &Rend_PatchTextureSpec(int               flags = 0,
                                                de::gfx::Wrapping wrapS = de::gfx::ClampToEdge,
                                                de::gfx::Wrapping wrapT = de::gfx::ClampToEdge);

void R_DrawPatch(ClientTexture &texture, int x, int y);
void R_DrawPatch(ClientTexture &texture, int x, int y, int w, int h, bool useOffsets = true);

void R_DrawPatchTiled(ClientTexture &   texture,
                      int               x,
                      int               y,
                      int               w,
                      int               h,
                      de::gfx::Wrapping wrapS,
                      de::gfx::Wrapping wrapT);

#endif // DE_CLIENT_RENDER_MISC_H
