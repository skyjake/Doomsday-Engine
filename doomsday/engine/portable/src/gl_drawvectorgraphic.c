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
#include "de_refresh.h"

#define DEFAULT_SCALE               (1)
#define DEFAULT_ANGLE               (0)

static boolean inited = false;
static uint svgCount;
static Svg* svgs;

static Svg* svgForId(svgid_t id)
{
    if(id != 0)
    {
        uint i;
        for(i = 0; i < svgCount; ++i)
        {
            Svg* svg = &svgs[i];
            if(svg->id == id)
                return svg;
        }
    }
    return 0;
}

static void clearSVGs(void)
{
    if(svgCount != 0)
    {
        uint i;
        for(i = 0; i < svgCount; ++i)
            deleteSVG(&svgs[i]);
        free(svgs);
        svgs = 0;
        svgCount = 0;
    }
}

void R_InitSVGs(void)
{
    if(inited) return;

    svgCount = 0;
    svgs = 0;
    inited = true;
}

void R_ShutdownSVGs(void)
{
    if(!inited) return;

    clearSVGs();
    inited = false;
}

void R_UnloadSVGs(void)
{
    uint i;
    if(!inited) return;
    if(DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED)) return; // Nothing to do.

    for(i = 0; i < svgCount; ++i)
    {
        unloadSVG(&svgs[i]);
    }
}

static void draw(const Svg* svg)
{
    uint i;
    DGL_Begin(DGL_LINES);
    for(i = 0; i < svg->count; ++i)
    {
        SvgLine* l = &svg->lines[i];
        DGL_TexCoord2f(0, l->from.x, l->from.y);
        DGL_Vertex2f(l->from.x, l->from.y);
        DGL_TexCoord2f(0, l->to.x, l->to.y);
        DGL_Vertex2f(l->to.x, l->to.y);
    }
    DGL_End();
}

void GL_DrawSVG3(svgid_t id, float x, float y, float scale, float angle)
{
    Svg* svg = prepareSVG(id);
    
    if(!svg) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(x, y, 0);
    if(angle != 0 || scale != 1)
    {
        DGL_PushMatrix();
        DGL_Rotatef(angle, 0, 0, 1);
        DGL_Scalef(scale, scale, 1);
    }

    if(svg->dlist)
    {
        // We have a display list available; call it and get out of here.
        DGL_CallList(svg->dlist);
    }
    else
    {   // No display list available. Lets draw it manually.
        draw(svg);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    if(angle != 0 || scale != 1)
        DGL_PopMatrix();
    DGL_Translatef(-x, -y, 0);
}

void GL_DrawSVG2(svgid_t id, float x, float y, float scale)
{
    GL_DrawSVG3(id, x, y, scale, DEFAULT_ANGLE);
}

void GL_DrawSVG(svgid_t id, float x, float y)
{
    GL_DrawSVG2(id, x, y, DEFAULT_SCALE);
}

void R_NewSVG(svgid_t id, const SvgLine* lines, size_t numLines)
{
    Svg* svg;
    size_t i;

    // Valid id?
    if(id == 0) Con_Error("R_NewSVG: Invalid id, zero is reserved.");

    // Already a vector graphic with this id?
    svg = svgForId(id);
    if(svg)
    {
        // We are replacing an existing vector graphic.
        deleteSVG(svg);
    }
    else
    {   // A new vector graphic.
        svgs = (Svg*)realloc(svgs, sizeof(*svgs) * ++svgCount);
        if(!svgs) Con_Error("R_NewSVG: Failed on allocation of %lu bytes enlarging SVG collection.", (unsigned long) (sizeof(*svgs) * svgCount));

        svg = &svgs[svgCount-1];
        svg->id = id;
        svg->dlist = 0;
    }

    svg->count = numLines;
    svg->lines = (SvgLine*)malloc(sizeof(*svg->lines) * numLines);
    if(!svg->lines) Con_Error("R_NewSVG: Failed on allocation of %lu bytes for new SVGLine list.", (unsigned long) (sizeof(*svg->lines) * numLines));

    for(i = 0; i < numLines; ++i)
    {
        memcpy(&svg->lines[i], &lines[i], sizeof(*svg->lines));
    }
}
