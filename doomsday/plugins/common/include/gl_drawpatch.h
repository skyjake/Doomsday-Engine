/**\file gl_drawpatch.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Convenient methods for drawing quads textured with Patch graphics.
 */

#ifndef LIBCOMMON_GRAPHICS_DRAW_PATCH_H
#define LIBCOMMON_GRAPHICS_DRAW_PATCH_H

#include "doomsday.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup drawPatchFlags Draw Patch Flags.
 */
/*@{*/
#define DPF_NO_OFFSETX      0x0010
#define DPF_NO_OFFSETY      0x0020
#define DPF_NO_OFFSET       (DPF_NO_OFFSETX|DPF_NO_OFFSETY)
/*@}*/

/**
 * @param patchId  Unique identifier of the patch to be drawn.
 * @param origin  Orient drawing about this offset (topleft:[0,0]).
 * @param alignFlags  @ref alignmentFlags
 * @param patchFlags  @ref drawPatchFlags
 */
void GL_DrawPatch3(patchid_t id, const Point2Raw* origin, int alignFlags, int patchFlags);
void GL_DrawPatch2(patchid_t id, const Point2Raw* origin, int alignFlags);
void GL_DrawPatch(patchid_t id, const Point2Raw* origin);

/**
 * Same as @a GL_DrawPatch except origin is specified with separate xy coordinates.
 */
void GL_DrawPatchXY3(patchid_t id, int x, int y, int alignFlags, int patchFlags);
void GL_DrawPatchXY2(patchid_t id, int x, int y, int alignFlags);
void GL_DrawPatchXY(patchid_t id, int x, int y);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_GRAPHICS_DRAW_PATCH_H */
