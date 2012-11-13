/**
 * @file dd_vectorgraphic.h
 * Scalable Vector Graphics (SVG). @ingroup gl
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_API_VECTORGRAPHIC_H
#define LIBDENG_API_VECTORGRAPHIC_H

/// @addtogroup gl
///@{

typedef uint32_t svgid_t;

#include <de/point.h>

typedef struct def_svgline_s {
    uint numPoints;
    const Point2Rawf* points;
} def_svgline_t;

void R_NewSvg(svgid_t svgId, const def_svgline_t* lines, uint numLines);

void GL_DrawSvg(svgid_t svgId, const Point2Rawf* origin);
void GL_DrawSvg2(svgid_t svgId, const Point2Rawf* origin, float scale);
void GL_DrawSvg3(svgid_t svgId, const Point2Rawf* origin, float scale, float angle);

///@}

#endif /* LIBDENG_API_VECTORGRAPHIC_H */
