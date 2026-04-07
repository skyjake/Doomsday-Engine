/** @file gl_drawpatch.cpp  Convenient drawing of Patch-textured quads.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "gl_drawpatch.h"

using namespace de;

void GL_DrawPatch(patchid_t id, const Vec2i &origin, int alignFlags, int patchFlags)
{
    if(!id) return;
    if(DD_GetInteger(DD_NOVIDEO)) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(id, &info)) return;

    RectRaw rect;
    rect.origin.x = origin.x;
    rect.origin.y = origin.y;

    if(alignFlags & ALIGN_RIGHT)
        rect.origin.x -= info.geometry.size.width;
    else if(!(alignFlags & ALIGN_LEFT))
        rect.origin.x -= info.geometry.size.width /2;

    if(alignFlags & ALIGN_BOTTOM)
        rect.origin.y -= info.geometry.size.height;
    else if(!(alignFlags & ALIGN_TOP))
        rect.origin.y -= info.geometry.size.height/2;

    rect.size.width  = info.geometry.size.width;
    rect.size.height = info.geometry.size.height;

    if(!(patchFlags & DPF_NO_OFFSETX))
        rect.origin.x += info.geometry.origin.x;
    if(!(patchFlags & DPF_NO_OFFSETY))
        rect.origin.y += info.geometry.origin.y;

    if(info.extraOffset[0])
    {
        // This offset is used only for the extra borders in the
        // "upscaled and sharpened" patches, so we can tweak the values
        // to our liking a bit more.
        rect.origin.x += info.extraOffset[0];
        rect.origin.y += info.extraOffset[1];
        rect.size.width  += std::abs(info.extraOffset[0])*2;
        rect.size.height += std::abs(info.extraOffset[1])*2;
    }

    DGL_SetPatch(id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_DrawRect(&rect);
}
