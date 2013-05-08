/** @file render/rend_main.h Core of the rendering subsystem.
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

#ifndef DENG_RENDER_MAIN_H
#define DENG_RENDER_MAIN_H

#ifndef __CLIENT__
#  error "render/rend_main.h only exists in the Client"
#endif

#ifndef __cplusplus
#  error "render/rend_main.h requires C++"
#endif

//#include <math.h>

#include "dd_types.h"

#include "MaterialVariantSpec"

#define GLOW_HEIGHT_MAX                     (1024.f) /// Absolute maximum

#define OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

#define SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

DENG_EXTERN_C coord_t vOrigin[3];
DENG_EXTERN_C float vang, vpitch, fieldOfView, yfov;
DENG_EXTERN_C float viewsidex, viewsidey;
DENG_EXTERN_C float fogColor[4];

DENG_EXTERN_C byte smoothTexAnim, devMobjVLights;
DENG_EXTERN_C boolean usingFog;

DENG_EXTERN_C int rAmbient;
DENG_EXTERN_C float rendLightDistanceAttenuation;
DENG_EXTERN_C int rendLightAttenuateFixedColormap;
DENG_EXTERN_C float lightModRange[255];

DENG_EXTERN_C int devRendSkyMode;
DENG_EXTERN_C int gameDrawHUD;

DENG_EXTERN_C int useDynLights;
DENG_EXTERN_C float dynlightFactor, dynlightFogBright;

DENG_EXTERN_C int useWallGlow;
DENG_EXTERN_C float glowFactor, glowHeightFactor;
DENG_EXTERN_C int glowHeightMax;

DENG_EXTERN_C int useShadows;
DENG_EXTERN_C float shadowFactor;
DENG_EXTERN_C int shadowMaxRadius;
DENG_EXTERN_C int shadowMaxDistance;

DENG_EXTERN_C int useShinySurfaces;

DENG_EXTERN_C float detailFactor, detailScale;

DENG_EXTERN_C byte devRendSkyAlways;
DENG_EXTERN_C byte freezeRLs;

void Rend_Register();

void Rend_Init();
void Rend_Shutdown();
void Rend_Reset();
void Rend_RenderMap();

void Rend_ModelViewMatrix(boolean use_angles);

#define Rend_PointDist2D(c) (fabs((vOrigin[VZ]-c[VY])*viewsidex - (vOrigin[VX]-c[VX])*viewsidey))

/**
 * Approximated! The Z axis aspect ratio is corrected.
 */
coord_t Rend_PointDist3D(coord_t const point[3]);

void Rend_ApplyTorchLight(float *color, float distance);

/**
 * Apply range compression delta to @a lightValue.
 * @param lightValue  Address of the value for adaptation.
 */
void Rend_ApplyLightAdaptation(float *lightValue);

/// Same as Rend_ApplyLightAdaptation except the delta is returned.
float Rend_LightAdaptationDelta(float lightvalue);

/**
 * Updates the lightModRange which is used to applify sector light to help
 * compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightLevel
 * during rendering (both positive and negative).
 */
void Rend_CalcLightModRange();

void R_DrawLightRange();

de::MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec();

texturevariantspecification_t &Rend_MapSurfaceShinyTextureSpec();

texturevariantspecification_t &Rend_MapSurfaceShinyMaskTextureSpec();

#endif // DENG_RENDER_MAIN_H
