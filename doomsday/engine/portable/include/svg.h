/**\file svg.h
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

#ifndef LIBDENG_SVG_H
#define LIBDENG_SVG_H

#include "dd_vectorgraphic.h"

/**
 * Svg. Scaleable Vector Graphic.
 */
struct Svg_s; // The svg instance (opaque).
typedef struct Svg_s Svg;

void Svg_Delete(Svg* svg);

void Svg_Draw(Svg* svg);

boolean Svg_Prepare(Svg* svg);

void Svg_Unload(Svg* svg);

/// @return  Unique identifier associated with this.
svgid_t Svg_UniqueId(Svg* svg);

/**
 * Static non-members:
 */

/**
 * Try to construct a new Svg instance from specified definition.
 *
 * @return  Newly created Svg instance if definition was valid else @a NULL
 */
Svg* Svg_FromDef(svgid_t uniqueId, const SvgLine* lines, size_t numLines);

#endif /* LIBDENG_SVG_H */
