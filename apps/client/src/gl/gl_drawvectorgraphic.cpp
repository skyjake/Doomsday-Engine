/**\file gl_drawvectorgraphic.cpp
 *
 * @authors Copyright © 2008-2010 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DE_NO_API_MACROS_SVG

#include "de_platform.h"
#include "dd_share.h"
#include "gl/svg.h"

#include <string.h>
#include <assert.h>
#include <de/legacy/memory.h>
#include <de/log.h>

#define DEFAULT_SCALE               (1)
#define DEFAULT_ANGLE               (0)

static dd_bool svgInited = false;
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
        svgs = (Svg**) M_Realloc(svgs, sizeof(*svgs) * ++svgCount);
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
        svgs = (Svg**) M_Realloc(svgs, sizeof(*svgs) * --svgCount);
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
    if(svgInited)
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
    svgInited = true;
}

void R_ShutdownSvgs(void)
{
    if(!svgInited) return;

    clearSvgs();
    svgInited = false;
}

void R_UnloadSvgs(void)
{
    uint i;
    if(!svgInited) return;
    if(DD_GetInteger(DD_NOVIDEO)) return; // Nothing to do.

    for(i = 0; i < svgCount; ++i)
    {
        Svg_Unload(svgs[i]);
    }
}

DE_EXTERN_C void GL_DrawSvg3(svgid_t id, const Point2Rawf* origin, float scale, float angle)
{
    Svg* svg = svgForId(id);

    DE_ASSERT(origin != 0);
    DE_ASSERT(svg != 0);

    if(!Svg_Prepare(svg))
    {
        LOGDEV_GL_ERROR("Cannot draw SVG #%i: failed to prepare") << id;
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

DE_EXTERN_C void GL_DrawSvg2(svgid_t id, const Point2Rawf* origin, float scale)
{
    GL_DrawSvg3(id, origin, scale, DEFAULT_ANGLE);
}

DE_EXTERN_C void GL_DrawSvg(svgid_t id, const Point2Rawf* origin)
{
    GL_DrawSvg2(id, origin, DEFAULT_SCALE);
}

DE_EXTERN_C void R_NewSvg(svgid_t id, const def_svgline_t* lines, uint numLines)
{
    Svg* svg, *oldSvg;

    // Valid id?
    DE_ASSERT(id != 0);

    // A new vector graphic.
    svg = Svg_FromDef(id, lines, numLines);
    if(!svg)
    {
        LOGDEV_GL_ERROR("Failed to construct SVG #%i") << id;
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

DE_DECLARE_API(Svg) =
{
    { DE_API_SVG },
    R_NewSvg,
    GL_DrawSvg,
    GL_DrawSvg2,
    GL_DrawSvg3
};
