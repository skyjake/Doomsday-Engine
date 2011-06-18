/**\file gl_drawpatch.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
#include "gl_drawpatch.h"

void GL_DrawPatch3(patchid_t id, int posX, int posY, int alignFlags, int patchFlags)
{
    float x = (float) posX, y = (float) posY, w, h;
    patchinfo_t info;

    if(DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED))
        return;

    if(id == 0 || !R_GetPatchInfo(id, &info))
        return;

    DGL_SetPatch(id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);

    if(alignFlags & ALIGN_RIGHT)
        x -= info.width;
    else if(!(alignFlags & ALIGN_LEFT))
        x -= info.width /2;

    if(alignFlags & ALIGN_BOTTOM)
        y -= info.height;
    else if(!(alignFlags & ALIGN_TOP))
        y -= info.height/2;

    w = (float) info.width;
    h = (float) info.height;

    if(!(patchFlags & DPF_NO_OFFSETX))
        x += (float) info.offset;
    if(!(patchFlags & DPF_NO_OFFSETY))
        y += (float) info.topOffset;

    if(info.extraOffset[0])
    {
        // This offset is used only for the extra borders in the
        // "upscaled and sharpened" patches, so we can tweak the values
        // to our liking a bit more.
        x += info.extraOffset[0];
        y += info.extraOffset[1];
        w += abs(info.extraOffset[0])*2;
        h += abs(info.extraOffset[1])*2;
    }

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, 1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, 1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(x, y + h);
    DGL_End();
}

void GL_DrawPatch2(patchid_t id, int x, int y, int alignFlags)
{
    GL_DrawPatch3(id, x, y, alignFlags, 0);
}

void GL_DrawPatch(patchid_t id, int x, int y)
{
    GL_DrawPatch2(id, x, y, ALIGN_TOPLEFT);
}
