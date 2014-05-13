/** @file rend_main.h Core of the rendering subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RENDER_MAIN_H
#define CLIENT_RENDER_MAIN_H

#ifndef __CLIENT__
#  error "render/rend_main.h only exists in the Client"
#endif

#include "dd_types.h"
#include "def_main.h"
#include "MaterialVariantSpec"
#include "WallEdge"
#include <de/Vector>
#include <de/Matrix>

class Sector;
class SectorCluster;

namespace de {
class Map;
class LightGrid;
}

// Multiplicative blending for dynamic lights?
#define IS_MUL              (dynlightBlend != 1 && !usingFog)

#define MTEX_DETAILS_ENABLED (r_detail && useMultiTexDetails && \
                              defs.count.details.num > 0)
#define IS_MTEX_DETAILS     (MTEX_DETAILS_ENABLED && numTexUnits > 1)
#define IS_MTEX_LIGHTS      (!IS_MTEX_DETAILS && !usingFog && useMultiTexLights \
                             && numTexUnits > 1 && envModAdd)

#define GLOW_HEIGHT_MAX                     (1024.f) /// Absolute maximum

#define OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

#define SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

DENG_EXTERN_C de::Vector3d vOrigin;
DENG_EXTERN_C float vang, vpitch, yfov;
DENG_EXTERN_C float viewsidex, viewsidey;
DENG_EXTERN_C float fogColor[4];

DENG_EXTERN_C byte smoothTexAnim, devMobjVLights;
DENG_EXTERN_C dd_bool usingFog;

DENG_EXTERN_C int renderTextures; /// @c 0= no textures, @c 1= normal mode, @c 2= lighting debug
DENG_EXTERN_C int renderWireframe;
DENG_EXTERN_C int useMultiTexLights;
DENG_EXTERN_C int useMultiTexDetails;

DENG_EXTERN_C int dynlightBlend;

DENG_EXTERN_C int torchAdditive;
DENG_EXTERN_C de::Vector3f torchColor;

DENG_EXTERN_C int rAmbient;
DENG_EXTERN_C float rendLightDistanceAttenuation;
DENG_EXTERN_C int rendLightAttenuateFixedColormap;
DENG_EXTERN_C float rendLightWallAngle;
DENG_EXTERN_C byte rendLightWallAngleSmooth;
DENG_EXTERN_C float rendSkyLight; // cvar
DENG_EXTERN_C byte rendSkyLightAuto; // cvar
DENG_EXTERN_C float lightModRange[255];
DENG_EXTERN_C int extraLight;
DENG_EXTERN_C float extraLightDelta;

DENG_EXTERN_C int devRendSkyMode;
DENG_EXTERN_C int gameDrawHUD;

DENG_EXTERN_C int useBias;

DENG_EXTERN_C int useDynLights;
DENG_EXTERN_C float dynlightFactor, dynlightFogBright;
DENG_EXTERN_C int rendMaxLumobjs;

DENG_EXTERN_C int useGlowOnWalls;
DENG_EXTERN_C float glowFactor, glowHeightFactor;
DENG_EXTERN_C int glowHeightMax;

DENG_EXTERN_C int useShadows;
DENG_EXTERN_C float shadowFactor;
DENG_EXTERN_C int shadowMaxRadius;
DENG_EXTERN_C int shadowMaxDistance;

DENG_EXTERN_C byte useLightDecorations;

DENG_EXTERN_C int useShinySurfaces;

DENG_EXTERN_C float detailFactor, detailScale;

DENG_EXTERN_C int ratioLimit;
DENG_EXTERN_C int mipmapping, filterUI, texQuality, filterSprites;
DENG_EXTERN_C int texMagMode, texAniso;
DENG_EXTERN_C int useSmartFilter;
DENG_EXTERN_C int texMagMode;
DENG_EXTERN_C int glmode[6];
DENG_EXTERN_C dd_bool fillOutlines;
DENG_EXTERN_C dd_bool noHighResTex;
DENG_EXTERN_C dd_bool noHighResPatches;
DENG_EXTERN_C dd_bool highResWithPWAD;
DENG_EXTERN_C byte loadExtAlways;

DENG_EXTERN_C byte devRendSkyAlways;
DENG_EXTERN_C byte rendInfoLums;
DENG_EXTERN_C byte devDrawLums;

DENG_EXTERN_C byte freezeRLs;

void Rend_Register();

void Rend_Init();
void Rend_Shutdown();
void Rend_Reset();

/// @return @c true iff multitexturing is currently enabled for lights.
bool Rend_IsMTexLights();

/// @return @c true iff multitexturing is currently enabled for detail textures.
bool Rend_IsMTexDetails();

void Rend_RenderMap(de::Map &map);

float Rend_FieldOfView();

/**
 * @param useAngles  @c true= Apply viewer angle rotation.
 */
void Rend_ModelViewMatrix(bool useAngles = true);

de::Matrix4f Rend_GetModelViewMatrix(int consoleNum, bool useAngles = true);

#define Rend_PointDist2D(c) (fabs((vOrigin.z-(c)[VY])*viewsidex - (vOrigin.x-(c)[VX])*viewsidey))

/**
 * The DOOM lighting model applies a light level delta to everything when
 * e.g. the player shoots.
 *
 * @return  Calculated delta.
 */
float Rend_ExtraLightDelta();

void Rend_ApplyTorchLight(float *color3, float distance);
void Rend_ApplyTorchLight(de::Vector4f &color, float distance);

/**
 * Apply range compression delta to @a lightValue.
 * @param lightValue  The light level value to apply adaptation to.
 */
void Rend_ApplyLightAdaptation(float &lightValue);

/// Same as Rend_ApplyLightAdaptation except the delta is returned.
float Rend_LightAdaptationDelta(float lightvalue);

/**
 * The DOOM lighting model applies distance attenuation to sector light
 * levels.
 *
 * @param distToViewer  Distance from the viewer to this object.
 * @param lightLevel    Sector lightLevel at this object's origin.
 * @return              The specified lightLevel plus any attentuation.
 */
float Rend_AttenuateLightLevel(float distToViewer, float lightLevel);

float Rend_ShadowAttenuationFactor(coord_t distance);

/**
 * Updates the lightModRange which is used to applify sector light to help
 * compensate for the differences between the OpenGL lighting equation,
 * the software Doom lighting model and the light grid (ambient lighting).
 *
 * The offsets in the lightRangeModTables are added to the sector->lightLevel
 * during rendering (both positive and negative).
 */
void Rend_UpdateLightModMatrix();

/**
 * Draws the lightModRange (for debug)
 */
void Rend_DrawLightModMatrix();

/**
 * Draws the light grid debug visual.
 */
void Rend_LightGridVisual(de::LightGrid &lg);

/**
 * Determines whether the sky light color tinting is enabled.
 */
bool Rend_SkyLightIsEnabled();

/**
 * Returns the effective sky light color.
 */
de::Vector3f Rend_SkyLightColor();

/**
 * Blend the given light value with the luminous object's color, applying any
 * applicable global modifiers and returns the result.
 *
 * @param color  Source light color.
 * @param light  Strength of the light on the illumination point.
 *
 * @return  Calculated result.
 */
de::Vector3f Rend_LuminousColor(de::Vector3f const &color, float light);

/**
 * Given an @a intensity determine the height of the plane glow, applying any
 * applicable global modifiers.
 *
 * @return Calculated result.
 */
coord_t Rend_PlaneGlowHeight(float intensity);

/**
 * Selects a Material for the given map @a surface considering the current map
 * renderer configuration.
 */
Material *Rend_ChooseMapSurfaceMaterial(Surface const &surface);

de::MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec();
de::MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec(int wrapS, int wrapT);

TextureVariantSpec const &Rend_MapSurfaceShinyTextureSpec();

TextureVariantSpec const &Rend_MapSurfaceShinyMaskTextureSpec();

void R_DivVerts(de::Vector3f *dst, de::Vector3f const *src,
    de::WallEdgeSection const &leftEdge, de::WallEdgeSection const &rightEdge);

void R_DivTexCoords(de::Vector2f *dst, de::Vector2f const *src,
    de::WallEdgeSection const &leftEdge, de::WallEdgeSection const &rightEdge);

void R_DivVertColors(de::Vector4f *dst, de::Vector4f const *src,
    de::WallEdgeSection const &leftEdge, de::WallEdgeSection const &rightEdge);

#endif // CLIENT_RENDER_MAIN_H
