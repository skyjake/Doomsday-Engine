/**\file svg.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#include "svg.h"

struct Svg_s {
    svgid_t id;
    DGLuint dlist;
    size_t count;
    struct svgline_s* lines;
};

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

static DGLuint constructDisplayList(DGLuint name, const Svg* svg)
{
    if(DGL_NewList(name, DGL_COMPILE))
    {
        draw(svg);
        return DGL_EndList();
    }
    return 0;
}

static Svg* prepareSVG(svgid_t id)
{
    Svg* svg = svgForId(id);
    if(svg)
    {
        if(!svg->dlist)
        {
            svg->dlist = constructDisplayList(0, svg);
        }
        return svg;
    }
    Con_Message("prepareSVG: Warning, no vectorgraphic is known by id %i.", (int) id);
    return NULL;
}

static void unloadSVG(Svg* svg)
{
    if(!(DD_GetInteger(DD_NOVIDEO) || DD_GetInteger(DD_DEDICATED)))
    {
        if(svg->dlist)
            DGL_DeleteLists(svg->dlist, 1);
    }
    svg->dlist = 0;
}

static void deleteSVG(Svg* svg)
{
    unloadSVG(svg);
    free(svg->lines);
    svg->lines = 0;
}
