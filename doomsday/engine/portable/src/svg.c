/**\file svg.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"

#include "svg.h"

struct Svg_s {
    svgid_t id;
    DGLuint dlist;
    size_t lineCount;
    SvgLine* lines;
};

void Svg_Delete(Svg* svg)
{
    assert(svg);
    Svg_Unload(svg);
    free(svg->lines);
    free(svg);
}

svgid_t Svg_UniqueId(Svg* svg)
{
    assert(svg);
    return svg->id;
}

static void draw(const Svg* svg)
{
    uint i;

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    glBegin(GL_LINES);
    for(i = 0; i < svg->lineCount; ++i)
    {
        SvgLine* l = &svg->lines[i];
        glTexCoord2f(l->from.x, l->from.y);
        glVertex2f(l->from.x, l->from.y);
        glTexCoord2f(l->to.x, l->to.y);
        glVertex2f(l->to.x, l->to.y);
    }
    glEnd();
}

static DGLuint constructDisplayList(DGLuint name, const Svg* svg)
{
    if(GL_NewList(name, DGL_COMPILE))
    {
        draw(svg);
        return GL_EndList();
    }
    return 0;
}

void Svg_Draw(Svg* svg)
{
    assert(svg);

    if(novideo || isDedicated)
    {
        assert(0); // Should not have been called!
        return;
    }

    // Have we uploaded our draw-optimized representation yet?
    if(svg->dlist)
    {
        // Draw!
        GL_CallList(svg->dlist);
        return;
    }

    // Draw manually in so-called 'immediate' mode.
    draw(svg);
}

boolean Svg_Prepare(Svg* svg)
{
    assert(svg);
    if(!novideo && !isDedicated)
    {
        if(!svg->dlist)
        {
            svg->dlist = constructDisplayList(0, svg);
        }
    }
    return !!svg->dlist;
}

void Svg_Unload(Svg* svg)
{
    assert(svg);
    if(novideo || isDedicated) return;

    if(svg->dlist)
    {
        GL_DeleteLists(svg->dlist, 1);
        svg->dlist = 0;
    }
}

Svg* Svg_FromDef(svgid_t uniqueId, const SvgLine* lines, size_t lineCount)
{
    Svg* svg;
    size_t i;

    if(!lines || lineCount == 0) return NULL;

    svg = (Svg*)malloc(sizeof(*svg));
    if(!svg) Con_Error("Svg::FromDef: Failed on allocation of %lu bytes for new Svg.", (unsigned long) sizeof(*svg));

    svg->id = uniqueId;
    svg->dlist = 0;
    svg->lineCount = lineCount;
    svg->lines = (SvgLine*)malloc(sizeof(*svg->lines) * lineCount);
    if(!svg->lines) Con_Error("Svg::FromDef: Failed on allocation of %lu bytes for new SVGLine list.", (unsigned long) (sizeof(*svg->lines) * lineCount));

    for(i = 0; i < lineCount; ++i)
    {
        memcpy(&svg->lines[i], &lines[i], sizeof(*svg->lines));
    }

    return svg;
}
