/**\file gl_drawpatch.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>
#include "doomsday.h"
#include "common.h"
#include "gl_drawpatch.h"

void GL_DrawPatch3(patchid_t id, const Point2Raw* origin, int alignFlags, int patchFlags)
{
    RectRaw rect;
    patchinfo_t info;

    if(id == 0 || DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED)) return;
    if(!R_GetPatchInfo(id, &info)) return;

    rect.origin.x = origin? origin->x : 0;
    rect.origin.y = origin? origin->y : 0;

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
        rect.size.width  += abs(info.extraOffset[0])*2;
        rect.size.height += abs(info.extraOffset[1])*2;
    }

    DGL_SetPatch(id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    DGL_DrawRect(&rect);
}

void GL_DrawPatch2(patchid_t id, const Point2Raw* origin, int alignFlags)
{
    GL_DrawPatch3(id, origin, alignFlags, 0);
}

void GL_DrawPatch(patchid_t id, const Point2Raw* origin)
{
    GL_DrawPatch2(id, origin, ALIGN_TOPLEFT);
}

void GL_DrawPatchXY3(patchid_t id, int x, int y, int alignFlags, int patchFlags)
{
    Point2Raw origin;
    origin.x = x;
    origin.y = y;
    GL_DrawPatch3(id, &origin, alignFlags, patchFlags);
}

void GL_DrawPatchXY2(patchid_t id, int x, int y, int alignFlags)
{
    GL_DrawPatchXY3(id, x, y, alignFlags, 0);
}

void GL_DrawPatchXY(patchid_t id, int x, int y)
{
    GL_DrawPatchXY2(id, x, y, ALIGN_TOPLEFT);
}
