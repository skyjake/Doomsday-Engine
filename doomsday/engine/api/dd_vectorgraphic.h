/**\file
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

/**
 * dd_vectorgraphic.h: Vector graphics.
 */

#ifndef LIBDENG_VECTORGRAPHIC_H
#define LIBDENG_VECTORGRAPHIC_H

typedef uint32_t vectorgraphicid_t;

// Used during vector graphic creation/registration.
// \todo Refactor me away.
typedef struct {
    float pos[3];
} mpoint_t;

typedef struct vgline_s {
    mpoint_t a, b;
} vgline_t;

void            R_NewVectorGraphic(vectorgraphicid_t vgId, const vgline_t* lines, size_t numLines);

void            GL_DrawVectorGraphic(vectorgraphicid_t vgId, float x, float y);
void            GL_DrawVectorGraphic2(vectorgraphicid_t vgId, float x, float y, float scale);
void            GL_DrawVectorGraphic3(vectorgraphicid_t vgId, float x, float y, float scale, float angle);

#endif /* LIBDENG_VECTORGRAPHIC_H */
