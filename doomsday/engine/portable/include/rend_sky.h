/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * rend_sky.h: Sky Sphere and 3D Models
 */

#ifndef __DOOMSDAY_SKY_H__
#define __DOOMSDAY_SKY_H__

// Sky hemispheres.
#define SKYHEMI_UPPER       0x1
#define SKYHEMI_LOWER       0x2
#define SKYHEMI_JUST_CAP    0x4 // Just draw the top or bottom cap.
#define SKYHEMI_FADEOUT_BG  0x8 // Draw the fadeout bg when drawing the cap.

typedef struct {
    float           rgb[3]; // The RGB values.
    short           set, use; // Is this set? Should be used?
    float           limit; // .3 by default.
} fadeout_t;

// Sky layer flags.
#define SLF_ENABLED         0x1 // Layer enabled.
#define SLF_MASKED          0x2 // Mask the layer texture.

typedef struct {
    int             flags;
    material_t*     mat;
    float           offset;
    fadeout_t       fadeout;
} skylayer_t;

extern int      skyDetail;

// Initialization:
void            Rend_SkyRegister(void);

// Functions:
void            Rend_InitSky(void);
void            Rend_ShutdownSky(void);
void            Rend_SkyDetail(int quarterDivs, int rows);
void            Rend_SkyParams(int layer, int param, void* data);
void            Rend_RenderSky(int hemis);

#endif
