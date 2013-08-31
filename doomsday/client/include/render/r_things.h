/** @file r_things.h Map Object Management and Refresh.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_RENDER_THINGS_H
#define DENG_RENDER_THINGS_H

#include "Materials"

DENG_EXTERN_C int levelFullBright;
DENG_EXTERN_C float pspOffset[2], pspLightLevelMultiplier;
DENG_EXTERN_C int alwaysAlign;
DENG_EXTERN_C float weaponOffsetScale, weaponFOVShift;
DENG_EXTERN_C int weaponOffsetScaleY;
DENG_EXTERN_C byte weaponScaleMode; // cvar
DENG_EXTERN_C float modelSpinSpeed;
DENG_EXTERN_C int maxModelDistance, noSpriteZWrite;
DENG_EXTERN_C int useSRVO, useSRVOAngle;
DENG_EXTERN_C int psp3d;

#ifdef __cplusplus
extern "C" {
#endif

/// @return  Radius of the mobj as it would visually appear to be.
coord_t R_VisualRadius(struct mobj_s *mo);

/**
 * Calculate the strength of the shadow this mobj should cast.
 *
 * @note Implemented using a greatly simplified version of the lighting equation;
 *       no light diminishing or light range compression.
 */
float R_ShadowStrength(struct mobj_s *mo);

float R_Alpha(struct mobj_s *mo);

/**
 * @return  The current floatbob offset for the mobj, if the mobj is flagged
 *          for bobbing; otherwise @c 0.
 */
coord_t R_GetBobOffset(struct mobj_s *mo);

float R_MovementYaw(float const mom[2]);
float R_MovementXYYaw(float momx, float momy);

float R_MovementPitch(float const mom[3]);
float R_MovementXYZPitch(float momx, float momy, float momz);

/**
 * Generates a vissprite for a mobj if it might be visible.
 */
void R_ProjectSprite(struct mobj_s *mobj);

/**
 * If 3D models are found for psprites, here we will create vissprites for them.
 */
void R_ProjectPlayerSprites(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DENG_RENDER_THINGS_H */
