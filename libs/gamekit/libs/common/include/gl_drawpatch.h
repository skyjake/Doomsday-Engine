/** @file gl_drawpatch.h  Convenient drawing of Patch-textured quads.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GRAPHICS_DRAW_PATCH_H
#define LIBCOMMON_GRAPHICS_DRAW_PATCH_H
#ifdef __cplusplus

#include <de/vector.h>
#include "doomsday.h"

/**
 * @defgroup drawPatchFlags  Draw Patch Flags.
 */
/*@{*/
#define DPF_NO_OFFSETX      0x0010
#define DPF_NO_OFFSETY      0x0020
#define DPF_NO_OFFSET       (DPF_NO_OFFSETX|DPF_NO_OFFSETY)
/*@}*/

/**
 * @param patchId     Unique identifier of the patch to be drawn.
 * @param origin      Orient drawing about this offset (topleft:[0,0]).
 * @param alignFlags  @ref alignmentFlags
 * @param patchFlags  @ref drawPatchFlags
 */
void GL_DrawPatch(patchid_t id, const de::Vec2i &origin = de::Vec2i(),
                  int alignFlags = ALIGN_TOPLEFT, int patchFlags = 0);

#endif  // __cplusplus
#endif  // LIBCOMMON_GRAPHICS_DRAW_PATCH_H
