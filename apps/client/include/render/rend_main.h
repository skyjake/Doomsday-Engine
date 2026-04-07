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
#include <de/matrix.h>
#include <de/record.h>
#include <de/vector.h>
#include <de/opengl.h>

#include "def_main.h"
#include "resource/materialvariantspec.h"

struct VectorLightData;
class Lumobj;
class ClientMaterial;
class Map;
class MaterialAnimator;
class Plane;
class Surface;
class WallEdge;
class WorldEdge;

namespace world
{
    class ConvexSubspace;
    class Sector;
    class Subsector;
}

/**
 * FakeRadio shadow data.
 * @ingroup render
 */
struct shadowcorner_t
{
    float corner;
    Plane *proximity;
    float pOffset;
    float pHeight;
};

/**
 * FakeRadio connected edge data.
 * @ingroup render
 */
struct edgespan_t
{
    float length;
    float shift;
};

// Multiplicative blending for dynamic lights?
#define IS_MUL          (dynlightBlend != 1 && !fogParams.usingFog)

#define MTEX_DETAILS_ENABLED (r_detail && DED_Definitions()->details.size() > 0)
#define IS_MTEX_DETAILS (MTEX_DETAILS_ENABLED)
#define IS_MTEX_LIGHTS  (!IS_MTEX_DETAILS && !fogParams.usingFog)

#define GLOW_HEIGHT_MAX (1024.f) /// Absolute maximum

#define OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

#define SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN (.05f)

DE_EXTERN_C de::Vec3d vOrigin; // Y/Z swizzled for drawing
DE_EXTERN_C float vang, vpitch, yfov;
DE_EXTERN_C float viewsidex, viewsidey;

struct FogParams
{
    bool usingFog;
    float fogColor[4];
    float fogStart;
    float fogEnd;
};

extern FogParams fogParams;

DE_EXTERN_C byte smoothTexAnim, devMobjVLights;

DE_EXTERN_C int renderTextures; /// @c 0= no textures, @c 1= normal mode, @c 2= lighting debug
#if defined (DE_OPENGL)
DE_EXTERN_C int renderWireframe;
#endif
//DE_EXTERN_C int useMultiTexLights;
//DE_EXTERN_C int useMultiTexDetails;

DE_EXTERN_C int dynlightBlend;

//DE_EXTERN_C int torchAdditive;
DE_EXTERN_C de::Vec3f torchColor;

DE_EXTERN_C int rAmbient;
DE_EXTERN_C float rendLightDistanceAttenuation;
DE_EXTERN_C int rendLightAttenuateFixedColormap;
DE_EXTERN_C float rendLightWallAngle;
DE_EXTERN_C byte rendLightWallAngleSmooth;
DE_EXTERN_C float rendSkyLight; // cvar
DE_EXTERN_C byte rendSkyLightAuto; // cvar
DE_EXTERN_C float lightModRange[255];
DE_EXTERN_C int extraLight;
DE_EXTERN_C float extraLightDelta;

DE_EXTERN_C int devRendSkyMode;
DE_EXTERN_C int gameDrawHUD;

//DE_EXTERN_C int useBias;

DE_EXTERN_C int useDynLights;
DE_EXTERN_C float dynlightFactor, dynlightFogBright;
DE_EXTERN_C int rendMaxLumobjs;

DE_EXTERN_C int useGlowOnWalls;
DE_EXTERN_C float glowFactor, glowHeightFactor;
DE_EXTERN_C int glowHeightMax;

DE_EXTERN_C int useShadows;
DE_EXTERN_C float shadowFactor;
DE_EXTERN_C int shadowMaxRadius;
DE_EXTERN_C int shadowMaxDistance;

DE_EXTERN_C byte useLightDecorations;

DE_EXTERN_C int useShinySurfaces;

DE_EXTERN_C float detailFactor, detailScale;

DE_EXTERN_C int ratioLimit;
DE_EXTERN_C int mipmapping, filterUI, texQuality, filterSprites;
DE_EXTERN_C int texMagMode, texAniso;
DE_EXTERN_C int useSmartFilter;
DE_EXTERN_C int texMagMode;
DE_EXTERN_C GLenum glmode[6];
DE_EXTERN_C dd_bool fillOutlines;
//DE_EXTERN_C dd_bool noHighResTex;
//DE_EXTERN_C dd_bool noHighResPatches;
//DE_EXTERN_C dd_bool highResWithPWAD;
DE_EXTERN_C byte loadExtAlways;

DE_EXTERN_C int devNoCulling;
DE_EXTERN_C byte devRendSkyAlways;
DE_EXTERN_C byte rendInfoLums;
DE_EXTERN_C byte devDrawLums;

DE_EXTERN_C byte freezeRLs;

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

void Rend_RenderMap(Map &map);

float Rend_FieldOfView();

/**
 * @param inWorldSpace  Apply viewer angles and head position to produce a transformation
 *                      from world space to view space.
 */
void Rend_ModelViewMatrix(bool inWorldSpace = true);

de::Mat4f Rend_GetModelViewMatrix(int consoleNum, bool inWorldSpace = true, bool vgaAspect = true);

de::Vec3d Rend_EyeOrigin();

/**
 * Returns the projection matrix that is used for rendering the current frame's
 * 3D portions.
 *
 * @param fixedFov        If non-zero, overrides the user's FOV with a fixed value.
 * @param clipRangeScale  Multiplier to apply to clip plane distances.
 */
de::Mat4f Rend_GetProjectionMatrix(float fixedFov = 0.0f, float clipRangeScale = 1.0f);

#define Rend_PointDist2D(c) (abs((vOrigin.z-(c)[VY])*viewsidex - (vOrigin.x-(c)[VX])*viewsidey))

void Rend_SetFixedView(int consoleNum, float yaw, float pitch, float fov, de::Vec2f viewportSize);
void Rend_UnsetFixedView();

/**
 * The DOOM lighting model applies a light level delta to everything when
 * e.g. the player shoots.
 *
 * @return  Calculated delta.
 */
float Rend_ExtraLightDelta();

void Rend_ApplyTorchLight(float *color3, float distance);
void Rend_ApplyTorchLight(de::Vec4f &color, float distance);

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
de::Vec3f Rend_SkyLightColor();

/**
 * Blend the given light value with the luminous object's color, applying any
 * applicable global modifiers and returns the result.
 *
 * @param color  Source light color.
 * @param light  Strength of the light on the illumination point.
 *
 * @return  Calculated result.
 */
de::Vec3f Rend_LuminousColor(const de::Vec3f &color, float light);

/**
 * Given an @a intensity determine the height of the plane glow, applying any
 * applicable global modifiers.
 *
 * @return Calculated result.
 */
coord_t Rend_PlaneGlowHeight(float intensity);

/**
 * @param point         World space point to evaluate.
 * @param ambientColor  Ambient color of the object being lit.
 * @param subspace      Subspace in which @a origin resides.
 * @param starkLight    @c true= World light has a more pronounced affect.
 *
 * @todo Does not belong here.
 */
de::duint Rend_CollectAffectingLights(const de::Vec3d &point,
    const de::Vec3f &ambientColor = de::Vec3f(1), world::ConvexSubspace *subspace = nullptr,
    bool starkLight = false);

void Rend_DrawVectorLight(const VectorLightData &vlight, float alpha);

MaterialAnimator *Rend_SpriteMaterialAnimator(const de::Record &spriteDef);

/**
 * Returns the radius of the given @a sprite, as it would visually appear to be.
 *
 * @note Presently considers rotation 0 only!
 */
double Rend_VisualRadius(const de::Record &sprite);

/**
 * Produce a luminous object from the given @a sprite configuration. The properties of
 * any resultant lumobj are configured in "sprite-local" space. This means that it will
 * be positioned relative to the center of the sprite and must be further configured
 * before adding to the map (i.e., translated to the origin in map space).
 *
 * @return  Newly generated Lumobj otherwise @c nullptr.
 */
Lumobj *Rend_MakeLumobj(const de::Record &sprite);

/**
 * Selects a Material for the given map @a surface considering the current map
 * renderer configuration.
 */
ClientMaterial *Rend_ChooseMapSurfaceMaterial(const Surface &surface);

const de::MaterialVariantSpec &Rend_MapSurfaceMaterialSpec();
const de::MaterialVariantSpec &Rend_MapSurfaceMaterialSpec(GLenum wrapS, GLenum wrapT);

const TextureVariantSpec &Rend_MapSurfaceLightmapTextureSpec();

const TextureVariantSpec &Rend_MapSurfaceShinyTextureSpec();
const TextureVariantSpec &Rend_MapSurfaceShinyMaskTextureSpec();

void R_DivVerts(de::Vec3f *dst, const de::Vec3f *src,
    const WorldEdge &leftEdge, const WorldEdge &rightEdge);

void R_DivTexCoords(de::Vec2f *dst, const de::Vec2f *src,
    const WorldEdge &leftEdge, const WorldEdge &rightEdge);

void R_DivVertColors(de::Vec4f *dst, const de::Vec4f *src,
    const WorldEdge &leftEdge, const WorldEdge &rightEdge);

#endif // CLIENT_RENDER_MAIN_H
