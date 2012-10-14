/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Halos and Lens Flares.
 */

#ifndef LIBDENG_RENDER_HALO_H
#define LIBDENG_RENDER_HALO_H

#ifdef __cplusplus
extern "C" {
#endif

extern int      haloOccludeSpeed;
extern int      haloMode, haloRealistic, haloBright, haloSize;
extern float    haloFadeMax, haloFadeMin, minHaloSize;

void            H_Register(void);
void            H_SetupState(boolean dosetup);
boolean         H_RenderHalo(coord_t x, coord_t y, coord_t z, float size,
                             DGLuint tex, const float color[3],
                             coord_t distanceToViewer, float occlusionFactor,
                             float brightnessFactor, float viewXOffset,
                             boolean primary, boolean viewRelativeRotate);

// Console commands.
D_CMD(FlareConfig);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_HALO_H */
