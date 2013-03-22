/** @file rend_halo.h Halos and Lens Flares.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_RENDER_HALO_H
#define LIBDENG_RENDER_HALO_H

#include "TextureVariantSpec"

DENG_EXTERN_C int      haloOccludeSpeed;
DENG_EXTERN_C int      haloMode, haloRealistic, haloBright, haloSize;
DENG_EXTERN_C float    haloFadeMax, haloFadeMin, minHaloSize;

void H_Register();

/**
 * Returns the texture variant specification for halos.
 */
texturevariantspecification_t &Rend_HaloTextureSpec();

void H_SetupState(bool dosetup);

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
bool H_RenderHalo(coord_t x, coord_t y, coord_t z, float size,
                  DGLuint tex, float const color[3],
                  coord_t distanceToViewer, float occlusionFactor,
                  float brightnessFactor, float viewXOffset,
                  bool primary, bool viewRelativeRotate);

// Console commands.
D_CMD(FlareConfig);

#endif /* LIBDENG_RENDER_HALO_H */
