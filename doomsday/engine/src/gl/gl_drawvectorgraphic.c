/**\file gl_drawvectorgraphic.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008-2010 Daniel Swanson <danij@dengine.net>
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

#include <string.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"

#include "gl/svg.h"

#define DEFAULT_SCALE               (1)
#define DEFAULT_ANGLE               (0)

static boolean inited = false;
static uint svgCount;
static Svg** svgs;

static Svg* svgForId(svgid_t id)
{
    if(id != 0)
    {
        uint i;
        for(i = 0; i < svgCount; ++i)
        {
            Svg* svg = svgs[i];
            if(Svg_UniqueId(svg) == id)
                return svg;
        }
    }
    return NULL;
}

/// @return  1-based index for @a svg if found, else zero.
static uint indexForSvg(Svg* svg)
{
    if(svg)
    {
        uint i;
        for(i = 0; i < svgCount; ++i)
        {
            if(svgs[i] == svg) return i+1;
        }
    }
    return 0;
}

/**
 * Link @a svg into the global collection.
 * @pre Not presently linked.
 */
static Svg* insertSvg(Svg* svg)
{
    if(svg)
    {
        svgs = (Svg**)realloc(svgs, sizeof(*svgs) * ++svgCount);
        if(!svgs) Con_Error("insertSvg: Failed on allocation of %lu bytes enlarging SVG collection.", (unsigned long) (sizeof(*svgs) * svgCount));

        svgs[svgCount-1] = svg;
    }
    return svg;
}

/**
 * Unlink @a svg if present in the global collection.
 */
static Svg* removeSvg(Svg* svg)
{
    uint index = indexForSvg(svg);
    if(index)
    {
        index--; // 1-based index.

        // Unlink from the collection.
        if(index != svgCount-1)
            memmove(svgs + index, svgs + index + 1, sizeof(*svgs) * (svgCount - index - 1));
        svgs = (Svg**)realloc(svgs, sizeof(*svgs) * --svgCount);
    }
    return svg;
}

static void deleteSvg(Svg* svg)
{
    if(!svg) return;

    removeSvg(svg);
    Svg_Delete(svg);
}

static void clearSvgs(void)
{
    while(svgCount)
    {
        deleteSvg(*svgs);
    }
}

void R_InitSvgs(void)
{
    if(inited)
    {
        // Allow re-init.
        clearSvgs();
    }
    else
    {
        // First init.
        svgs = NULL;
        svgCount = 0;
    }
    inited = true;
}

void R_ShutdownSvgs(void)
{
    if(!inited) return;

    clearSvgs();
    inited = false;
}

void R_UnloadSvgs(void)
{
    uint i;
    if(!inited) return;
    if(DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED)) return; // Nothing to do.

    for(i = 0; i < svgCount; ++i)
    {
        Svg_Unload(svgs[i]);
    }
}

void GL_DrawSvg3(svgid_t id, const Point2Rawf* origin, float scale, float angle)
{
    Svg* svg = svgForId(id);

    if(!origin)
    {
#if _DEBUG
        if(id != 0)
            Con_Message("GL_DrawSvg: Invalid origin argument (=NULL), aborting draw.\n");
#endif
        return;
    }
    if(!svg)
    {
#if _DEBUG
        if(id != 0)
            Con_Message("GL_DrawSvg: Unknown SVG id #%u, aborting draw.\n", (unsigned int)id);
#endif
        return;
    }
    if(!Svg_Prepare(svg))
    {
#if _DEBUG
        Con_Message("GL_DrawSvg: Failed preparing SVG #%u, aborting draw.\n", (unsigned int)id);
#endif
        return;
    }
    
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(origin->x, origin->y, 0);

    if(angle != 0 || scale != 1)
    {
        DGL_PushMatrix();
        DGL_Rotatef(angle, 0, 0, 1);
        DGL_Scalef(scale, scale, 1);
    }

    Svg_Draw(svg);

    DGL_MatrixMode(DGL_MODELVIEW);
    if(angle != 0 || scale != 1)
        DGL_PopMatrix();
    DGL_Translatef(-origin->x, -origin->y, 0);
}

void GL_DrawSvg2(svgid_t id, const Point2Rawf* origin, float scale)
{
    GL_DrawSvg3(id, origin, scale, DEFAULT_ANGLE);
}

void GL_DrawSvg(svgid_t id, const Point2Rawf* origin)
{
    GL_DrawSvg2(id, origin, DEFAULT_SCALE);
}

void R_NewSvg(svgid_t id, const def_svgline_t* lines, uint numLines)
{
    Svg* svg, *oldSvg;

    // Valid id?
    if(id == 0) Con_Error("R_NewSvg: Invalid id, zero is reserved.");

    // A new vector graphic.
    svg = Svg_FromDef(id, lines, numLines);
    if(!svg)
    {
#if _DEBUG
        Con_Message("Warning:R_NewSvg: Failed constructing new SVG #%u, aborting.", (unsigned int)id);
#endif
        return;
    }

    // Already a vector graphic with this id?
    oldSvg = svgForId(id);
    if(oldSvg)
    {
        // We are replacing an existing vector graphic.
        deleteSvg(oldSvg);
    }

    // Add the new SVG to the global collection.
    insertSvg(svg);
}
