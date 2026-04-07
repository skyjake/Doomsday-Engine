/** @file api_svg.h Scalable Vector Graphics (SVG).
 * @ingroup gl
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_VECTORGRAPHIC_H
#define DOOMSDAY_API_VECTORGRAPHIC_H

#include <de/legacy/point.h>
#include "apis.h"

typedef uint32_t svgid_t;

typedef struct def_svgline_s {
    uint numPoints;
    const Point2Rawf* points;
} def_svgline_t;

DE_API_TYPEDEF(Svg)
{
    de_api_t api;
    void (*NewSvg)(svgid_t svgId, const def_svgline_t* lines, uint numLines);
    void (*DrawSvg)(svgid_t svgId, const Point2Rawf* origin);
    void (*DrawSvg2)(svgid_t svgId, const Point2Rawf* origin, float scale);
    void (*DrawSvg3)(svgid_t svgId, const Point2Rawf* origin, float scale, float angle);
}
DE_API_T(Svg);

#ifndef DE_NO_API_MACROS_SVG
#define R_NewSvg        _api_Svg.NewSvg
#define GL_DrawSvg      _api_Svg.DrawSvg
#define GL_DrawSvg2     _api_Svg.DrawSvg2
#define GL_DrawSvg3     _api_Svg.DrawSvg3
#endif

#if defined __DOOMSDAY__ && defined __CLIENT__
DE_USING_API(Svg);
#endif

#endif /* DOOMSDAY_API_VECTORGRAPHIC_H */
