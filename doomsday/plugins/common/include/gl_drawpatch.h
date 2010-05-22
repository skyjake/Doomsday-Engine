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
 * gl_drawpatch.h: Convenient methods for drawing quads textured with
 * Patch-format graphics.
 */

#ifndef LIBCOMMON_GRAPHICS_DRAW_PATCH_H
#define LIBCOMMON_GRAPHICS_DRAW_PATCH_H

#include "dd_types.h"

/**
 * @defGroup drawPatchFlags Draw Patch Flags.
 */
/*@{*/
#define DPF_ALIGN_LEFT      0x0001
#define DPF_ALIGN_RIGHT     0x0002
#define DPF_ALIGN_BOTTOM    0x0004
#define DPF_ALIGN_TOP       0x0008
#define DPF_NO_OFFSETX      0x0010
#define DPF_NO_OFFSETY      0x0020

#define DPF_NO_OFFSET       (DPF_NO_OFFSETX|DPF_NO_OFFSETY)
#define DPF_ALIGN_TOPLEFT   (DPF_ALIGN_TOP|DPF_ALIGN_LEFT)
#define DPF_ALIGN_BOTTOMLEFT (DPF_ALIGN_BOTTOM|DPF_ALIGN_LEFT)
#define DPF_ALIGN_TOPRIGHT  (DPF_ALIGN_TOP|DPF_ALIGN_RIGHT)
#define DPF_ALIGN_BOTTOMRIGHT (DPF_ALIGN_BOTTOM|DPF_ALIGN_RIGHT)
/*@}*/

void            GL_DrawPatch(patchid_t id, int x, int y);
void            GL_DrawPatch2(patchid_t id, int x, int y, short flags);

#endif /* LIBCOMMON_GRAPHICS_DRAW_PATCH_H */
