/**\file rend_fakeradio.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_FAKERADIO_H
#define LIBDENG_RENDER_FAKERADIO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float shadowRGB[3], shadowDark;
    float shadowSize;
    const shadowcorner_t* botCn, *topCn, *sideCn;
    const edgespan_t* spans;
    const coord_t* segOffset;
    const coord_t* segLength;
    const coord_t* linedefLength;
    const Sector* frontSec, *backSec;
} rendsegradio_params_t;

/// Register the console commands, variables, etc..., of this module.
void Rend_RadioRegister(void);

float Rend_RadioCalcShadowDarkness(float lightLevel);

/**
 * Called to update the shadow properties used when doing FakeRadio for the
 * given linedef.
 */
void Rend_RadioUpdateLinedef(LineDef* line, boolean backSide);

/**
 * Render FakeRadio for the given hedge section.
 */
void Rend_RadioSegSection(const rvertex_t* rvertices, const walldiv_t* divs,
    const rendsegradio_params_t* params);

/**
 * Render FakeRadio for the given BSP leaf.
 */
void Rend_RadioBspLeafEdges(BspLeaf* bspLeaf);

/**
 * Render the shadow poly vertices, for debug.
 */
void Rend_DrawShadowOffsetVerts(void);

#ifdef __cplusplus
}
#endif

#endif /// LIBDENG_RENDER_FAKERADIO_H
