/**\file rend_main.h
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
 * Core of the rendering subsystem.
 */

#ifndef LIBDENG_REND_MAIN_H
#define LIBDENG_REND_MAIN_H

#include <math.h>
#ifdef __CLIENT__
#  include "rend_list.h"
#endif
#include "r_things.h"

struct materialvariantspec_s;

#define GLOW_HEIGHT_MAX                     (1024.f) /// Absolute maximum

#define OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

#define SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

DENG_EXTERN_C coord_t vOrigin[3];
DENG_EXTERN_C float vang, vpitch, fieldOfView, yfov;
DENG_EXTERN_C byte smoothTexAnim, devMobjVLights;
DENG_EXTERN_C float viewsidex, viewsidey;
DENG_EXTERN_C boolean usingFog;
DENG_EXTERN_C float fogColor[4];
DENG_EXTERN_C int rAmbient;
DENG_EXTERN_C float rendLightDistanceAttentuation;
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

DENG_EXTERN_C byte devRendSkyAlways;
DENG_EXTERN_C byte freezeRLs;

#ifdef __cplusplus
extern "C" {
#endif

void Rend_Register(void);

void Rend_Init(void);
void Rend_Shutdown(void);
void Rend_Reset(void);

void Rend_RenderMap(void);
void Rend_ModelViewMatrix(boolean use_angles);

#define Rend_PointDist2D(c) (fabs((vOrigin[VZ]-c[VY])*viewsidex - (vOrigin[VX]-c[VX])*viewsidey))

coord_t Rend_PointDist3D(coord_t const point[3]);
void Rend_ApplyTorchLight(float *color, float distance);

/**
 * Apply range compression delta to @a lightValue.
 * @param lightValue  Address of the value for adaptation.
 */
void Rend_ApplyLightAdaptation(float *lightValue);

/// Same as Rend_ApplyLightAdaptation except the delta is returned.
float Rend_LightAdaptationDelta(float lightvalue);

void Rend_CalcLightModRange(void);

/**
 * Number of vertices needed for this leaf's trifan.
 */
uint Rend_NumFanVerticesForBspLeaf(BspLeaf *bspLeaf);

void R_DrawLightRange(void);

#ifdef __cplusplus
} // extern "C"

de::MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec();
#endif

#endif /* LIBDENG_REND_MAIN_H */
