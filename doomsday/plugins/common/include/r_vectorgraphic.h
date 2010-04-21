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
 * r_vectorgraphic.h: Vector graphics.
 */

#ifndef LIBCOMMON_VECTORGRAPHIC_H
#define LIBCOMMON_VECTORGRAPHIC_H

typedef enum {
    VG_NONE = -1,
    VG_KEYSQUARE,
    VG_TRIANGLE,
    VG_ARROW,
#if !__JHEXEN__
    VG_CHEATARROW,
#endif
    VG_XHAIR1,
    VG_XHAIR2,
    VG_XHAIR3,
    VG_XHAIR4,
    VG_XHAIR5,
    VG_XHAIR6,
    NUM_VECTOR_GRAPHICS
} vectorgraphicname_t;

typedef struct vectorgrap_s {
    DGLuint dlist;
    uint count;
    struct vgline_s* lines;
} vectorgraphic_t;

void                R_InitVectorGraphics(void);
void                R_UnloadVectorGraphics(void);
void                R_ShutdownVectorGraphics(void);

vectorgraphic_t*    R_PrepareVectorGraphic(vectorgraphicname_t id);
void                R_DrawVectorGraphic(vectorgraphic_t* vg);

#endif /* LIBCOMMON_VECTORGRAPHIC_H */
