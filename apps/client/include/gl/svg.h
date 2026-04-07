/**
 * @file svg.h
 * Scaleable Vector Graphic. @ingroup refresh
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_REFRESH_SVG_H
#define DE_REFRESH_SVG_H

#include "api_svg.h"

struct svgline_s;
typedef struct svgline_s SvgLine;

/**
 * Svg. Scaleable Vector Graphic.
 */
struct svg_s; // The svg instance (opaque).
typedef struct svg_s Svg;

#ifdef __cplusplus
extern "C" {
#endif

void R_InitSvgs(void);

/**
 * Unload any resources needed for vector graphics.
 * Called during shutdown and before a renderer restart.
 */
void R_UnloadSvgs(void);

void R_ShutdownSvgs(void);

void Svg_Delete(Svg* svg);

void Svg_Draw(Svg* svg);

dd_bool Svg_Prepare(Svg* svg);

void Svg_Unload(Svg* svg);

/// @return  Unique identifier associated with this.
svgid_t Svg_UniqueId(Svg* svg);

/**
 * Static members:
 */

/**
 * Try to construct a new Svg instance from specified definition.
 *
 * @return  Newly created Svg instance if definition was valid else @a NULL
 */
Svg* Svg_FromDef(svgid_t uniqueId, const def_svgline_t* lines, uint numLines);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_REFRESH_SVG_H
