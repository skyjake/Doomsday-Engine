/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "doomsday.h"

#include "gl_drawpatch.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void GL_DrawPatch2(patchid_t id, int posX, int posY, short flags)
{
    float x = (float) posX, y = (float) posY, w, h;
    patchinfo_t info;

    if(id == 0)
        return;

    DGL_SetPatch(id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
    if(!R_GetPatchInfo(id, &info))
        return;

    if(flags & DPF_ALIGN_RIGHT)
        x -= info.width;
    else if(!(flags & DPF_ALIGN_LEFT))
        x -= info.width /2;

    if(flags & DPF_ALIGN_BOTTOM)
        y -= info.height;
    else if(!(flags & DPF_ALIGN_TOP))
        y -= info.height/2;

    w = (float) info.width;
    h = (float) info.height;

    if(!(flags & DPF_NO_OFFSETX))
        x += (float) info.offset;
    if(!(flags & DPF_NO_OFFSETY))
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

void GL_DrawPatch(patchid_t id, int x, int y)
{
    GL_DrawPatch2(id, x, y, DPF_ALIGN_TOPLEFT);
}
