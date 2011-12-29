/**\file dd_vectorgraphic.h
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

/**
 * SVG (Scalable Vector Graphic).
 */

#ifndef LIBDENG_API_VECTORGRAPHIC_H
#define LIBDENG_API_VECTORGRAPHIC_H

typedef uint32_t svgid_t;

void R_NewSVG(svgid_t svgId, const SvgLine* lines, size_t numLines);

void GL_DrawSVG(svgid_t svgId, float x, float y);
void GL_DrawSVG2(svgid_t svgId, float x, float y, float scale);
void GL_DrawSVG3(svgid_t svgId, float x, float y, float scale, float angle);

#endif /* LIBDENG_API_VECTORGRAPHIC_H */
