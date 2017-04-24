/** @file rend_main.h  Core of the rendering subsystem.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include <de/Matrix>
#include <de/Record>
#include <de/Vector>

#include "def_main.h"
#include "MaterialVariantSpec"
#include "WallEdge"

class Lumobj;
class Sector;
struct VectorLightData;
class ClientMaterial;
class MaterialAnimator;
#if 0
namespace de {
    class LightGrid;
}
#endif
namespace world {
    class ConvexSubspace;
    class Subsector;
}

// Multiplicative blending for dynamic lights?
#define IS_MUL          (dynlightBlend != 1 && !fogParams.usingFog)

#define MTEX_DETAILS_ENABLED (r_detail && DED_Definitions()->details.size() > 0)
#define IS_MTEX_DETAILS (MTEX_DETAILS_ENABLED)
#define IS_MTEX_LIGHTS  (!IS_MTEX_DETAILS && !fogParams.usingFog)

#define GLOW_HEIGHT_MAX (1024.f) /// Absolute maximum

#define OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

#define SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

DENG_EXTERN_C de::Vector3d vOrigin; // Y/Z swizzled for drawing
DENG_EXTERN_C float vang, vpitch, yfov;
DENG_EXTERN_C float viewsidex, viewsidey;

struct FogParams
{
    bool usingFog;
    float fogColor[4];
    float fogStart;
    float fogEnd;
};

extern FogParams fogParams;

DENG_EXTERN_C byte smoothTexAnim, devMobjVLights;

DENG_EXTERN_C int renderTextures; /// @c 0= no textures, @c 1= normal mode, @c 2= lighting debug
DENG_EXTERN_C int renderWireframe;
//DENG_EXTERN_C int useMultiTexLights;
//DENG_EXTERN_C int useMultiTexDetails;

DENG_EXTERN_C int dynlightBlend;

//DENG_EXTERN_C int torchAdditive;
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

//DENG_EXTERN_C int useBias;

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
//DENG_EXTERN_C dd_bool noHighResTex;
//DENG_EXTERN_C dd_bool noHighResPatches;
//DENG_EXTERN_C dd_bool highResWithPWAD;
DENG_EXTERN_C byte loadExtAlways;

DENG_EXTERN_C int devNoCulling;
DENG_EXTERN_C byte devRendSkyAlways;
DENG_EXTERN_C byte rendInfoLums;
DENG_EXTERN_C byte devDrawLums;

DENG_EXTERN_C byte freezeRLs;

struct ResourceConfigVars
{
    de::Variable *noHighResTex;
    de::Variable *noHighResPatches;
    de::Variable *highResWithPWAD;
};

ResourceConfigVars &R_Config();

void Rend_Register();

void Rend_Reset();

/// Reset any cached state that gets normally reused between frames.
void Rend_ResetLookups();

/// @return @c true iff multitexturing is currently enabled for lights.
bool Rend_IsMTexLights();

/// @return @c true iff multitexturing is currently enabled for detail textures.
bool Rend_IsMTexDetails();

void Rend_RenderMap(world::Map &map);

float Rend_FieldOfView();

/**
 * @param inWorldSpace  Apply viewer angles and head position to produce a transformation
 *                      from world space to view space.
 */
void Rend_ModelViewMatrix(bool inWorldSpace = true);

de::Matrix4f Rend_GetModelViewMatrix(int consoleNum, bool inWorldSpace = true);

de::Vector3d Rend_EyeOrigin();

/**
 * Returns the projection matrix that is used for rendering the current frame's
 * 3D portions.
 */
de::Matrix4f Rend_GetProjectionMatrix();

#define Rend_PointDist2D(c) (fabs((vOrigin.z-(c)[VY])*viewsidex - (vOrigin.x-(c)[VX])*viewsidey))

void Rend_SetFixedView(int consoleNum, float yaw, float pitch, float fov, de::Vector2f viewportSize);
void Rend_UnsetFixedView();

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
//void Rend_LightGridVisual(de::LightGrid &lg);

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
de::Vector3f Rend_LuminousColor(de::Vector3f const &color, de::dfloat light);

/**
 * Given an @a intensity determine the height of the plane glow, applying any
 * applicable global modifiers.
 *
 * @return Calculated result.
 */
coord_t Rend_PlaneGlowHeight(de::dfloat intensity);

/**
 * @param point         World space point to evaluate.
 * @param ambientColor  Ambient color of the object being lit.
 * @param subspace      Subspace in which @a origin resides.
 * @param starkLight    @c true= World light has a more pronounced affect.
 *
 * @todo Does not belong here.
 */
de::duint Rend_CollectAffectingLights(de::Vector3d const &point,
    de::Vector3f const &ambientColor = de::Vector3f(1, 1, 1), world::ConvexSubspace *subspace = nullptr,
    bool starkLight = false);

void Rend_DrawVectorLight(VectorLightData const &vlight, de::dfloat alpha);

MaterialAnimator *Rend_SpriteMaterialAnimator(de::Record const &spriteDef);

/**
 * Returns the radius of the given @a sprite, as it would visually appear to be.
 *
 * @note Presently considers rotation 0 only!
 */
de::ddouble Rend_VisualRadius(de::Record const &sprite);

/**
 * Produce a luminous object from the given @a sprite configuration. The properties of
 * any resultant lumobj are configured in "sprite-local" space. This means that it will
 * be positioned relative to the center of the sprite and must be further configured
 * before adding to the map (i.e., translated to the origin in map space).
 *
 * @return  Newly generated Lumobj otherwise @c nullptr.
 */
Lumobj *Rend_MakeLumobj(de::Record const &sprite);

/**
 * Selects a Material for the given map @a surface considering the current map
 * renderer configuration.
 */
ClientMaterial *Rend_ChooseMapSurfaceMaterial(Surface const &surface);

de::MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec();
de::MaterialVariantSpec const &Rend_MapSurfaceMaterialSpec(int wrapS, int wrapT);

TextureVariantSpec const &Rend_MapSurfaceLightmapTextureSpec();

TextureVariantSpec const &Rend_MapSurfaceShinyTextureSpec();
TextureVariantSpec const &Rend_MapSurfaceShinyMaskTextureSpec();

void R_DivVerts(de::Vector3f *dst, de::Vector3f const *src,
    WorldEdge const &leftEdge, WorldEdge const &rightEdge);

void R_DivTexCoords(de::Vector2f *dst, de::Vector2f const *src,
    WorldEdge const &leftEdge, WorldEdge const &rightEdge);

void R_DivVertColors(de::Vector4f *dst, de::Vector4f const *src,
    WorldEdge const &leftEdge, WorldEdge const &rightEdge);

#endif // CLIENT_RENDER_MAIN_H
