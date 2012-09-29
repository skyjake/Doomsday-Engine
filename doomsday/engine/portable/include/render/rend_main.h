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
#include "rend_list.h"
#include "r_things.h"

struct materialvariantspecification_s;

#ifdef __cplusplus
extern "C" {
#endif

#define GLOW_HEIGHT_MAX                     (1024.f) /// Absolute maximum

#define OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

#define SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

extern coord_t vOrigin[3];
extern float vang, vpitch, fieldOfView, yfov;
extern byte smoothTexAnim, devMobjVLights;
extern float viewsidex, viewsidey;
extern boolean usingFog;
extern float fogColor[4];
extern int rAmbient;
extern float rendLightDistanceAttentuation;
extern float lightModRange[255];
extern int devRendSkyMode;

extern int useDynLights;
extern float dynlightFactor, dynlightFogBright;

extern int useWallGlow;
extern float glowFactor, glowHeightFactor;
extern int glowHeightMax;

extern int useShadows;
extern float shadowFactor;
extern int shadowMaxRadius;
extern int shadowMaxDistance;

extern int useShinySurfaces;

void Rend_Register(void);

void Rend_Init(void);
void Rend_Shutdown(void);
void Rend_Reset(void);

void            Rend_RenderMap(void);
void            Rend_ModelViewMatrix(boolean use_angles);

#define Rend_PointDist2D(c) (fabs((vOrigin[VZ]-c[VY])*viewsidex - (vOrigin[VX]-c[VX])*viewsidey))

const struct materialvariantspecification_s* Rend_MapSurfaceDiffuseMaterialSpec(void);

coord_t         Rend_PointDist3D(coord_t const point[3]);
void            Rend_ApplyTorchLight(float* color, float distance);

/**
 * Apply range compression delta to @a lightValue.
 * @param lightValue  Address of the value for adaptation.
 */
void Rend_ApplyLightAdaptation(float* lightValue);

/// Same as Rend_ApplyLightAdaptation except the delta is returned.
float Rend_LightAdaptationDelta(float lightvalue);

void            Rend_CalcLightModRange(void);

/**
 * Number of vertices needed for this leaf's trifan.
 */
uint Rend_NumFanVerticesForBspLeaf(BspLeaf* bspLeaf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_REND_MAIN_H */
