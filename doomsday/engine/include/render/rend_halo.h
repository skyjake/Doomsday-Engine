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

/**
 * Render a halo.
 *
 * The caller must check that @c sourcevis, really has a ->light! (? -jk)
 *
 * @param x         X coordinate of the center of the halo.
 * @param y         Y coordinate of the center of the halo.
 * @param z         Z coordinate of the center of the halo.
 * @param size      The precalculated radius of the primary halo.
 * @param tex       Texture to use for the halo.
 * @param color     Color for the halo.
 * @param distanceToViewer  Distance to viewer. Used for fading the halo when far away.
 * @param occlusionFactor   How much the halo is occluded by something.
 * @param brightnessFactor  Overall brightness factor.
 * @param viewXOffset       Horizontal offset for the halo (in viewspace, relative to @a x, @a y, @a z).
 * @param primary   Type of halo:
 *                  - @c true: the primary halo.
 *                  - @c false: the secondary halo. Won't be clipped or occluded
 *                    by anything; they're drawn after everything else,
 *                    during a separate pass. The caller must setup the rendering state.
 * @param viewRelativeRotate  @c true: Halo rotates in relation to its viewspace origin.
 *
 * @return          @c true, iff a halo was rendered.
 */
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
