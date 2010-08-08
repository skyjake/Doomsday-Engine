/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2010 Daniel Swanson <danij@dengine.net>
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
 * Sky Sphere and 3D Models
 */

#ifndef LIBDENG_RENDER_SKY_H
#define LIBDENG_RENDER_SKY_H

typedef struct {
    float rgb[3];
    short set, use; /// Is this set? Should it be used?
    float limit; /// .3 by default.
} fadeout_t;

// Sky layer flags.
#define SLF_ENABLED         0x1 // Layer enabled.
#define SLF_MASKED          0x2 // Mask the layer texture.

typedef struct {
    int flags;
    material_t* mat;
    float offset;
    fadeout_t fadeout;
} skylayer_t;

extern int skyDetail;

// Initialization:
void Rend_SkyRegister(void);

void Rend_InitSky(void);
void Rend_ShutdownSky(void);
void Rend_SkyDetail(int quarterDivs, int rows);
void Rend_SkyParams(int layer, int param, void* data);
void Rend_RenderSky(void);

#endif /* LIBDENG_RENDER_SKY_H */
