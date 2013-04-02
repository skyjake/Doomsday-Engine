/** @file rend_list.h
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

/**
 * Rendering Lists.
 */

#ifndef LIBDENG_REND_LIST_H
#define LIBDENG_REND_LIST_H

#ifdef __SERVER__
#  error "render" not available in a SERVER build
#endif

#include "render/rendpoly.h"

#ifdef __cplusplus
extern "C" {
#endif

// Multiplicative blending for dynamic lights?
#define IS_MUL              (dynlightBlend != 1 && !usingFog)

/**
 * Types of render primitive supported by this module (polygons only).
 */
typedef enum primtype_e {
    PT_FIRST = 0,
    PT_TRIANGLE_STRIP = PT_FIRST, // Used for most stuff.
    PT_FAN,
    NUM_PRIM_TYPES
} primtype_t;

/**
 * @defgroup rendpolyFlags  Rendpoly Flags
 * @ingroup flags
 * @{
 */
#define RPF_SKYMASK                 0x1 /// This primitive is to be added to the sky mask.
#define RPF_LIGHT                   0x2 /// This primitive is to be added to the special lists for lights.
#define RPF_SHADOW                  0x4 /// This primitive is to be added to the special lists for shadows (either object or fakeradio edge shadow).
#define RPF_HAS_DYNLIGHTS           0x8 /// Dynamic light primitives are to be drawn on top of this.

/// Default rendpolyFlags
#define RPF_DEFAULT                 0
/**@}*/

extern int renderTextures;
extern int renderWireframe;
extern int useMultiTexLights;
extern int useMultiTexDetails;

extern int dynlightBlend;

extern int torchAdditive;
extern float torchColor[];

/// Register the console commands, variables, etc..., of this module.
void RL_Register(void);

/// Initialize this module.
void RL_Init(void);

/// Shutdown this module.
void RL_Shutdown(void);

/// @return @c true iff multitexturing is currently enabled for lights.
boolean RL_IsMTexLights(void);

/// @return @c true iff multitexturing is currently enabled for detail textures.
boolean RL_IsMTexDetails(void);

void RL_ClearLists(void);

void RL_DeleteLists(void);

/**
 * Reset the texture unit write state back to the initial default values.
 * Any mappings between logical units and preconfigured RTU states are
 * cleared at this time.
 */
void RL_LoadDefaultRtus(void);

/**
 * Map the texture unit write state for the identified @a idx unit to
 * @a rtu. This creates a reference to @a rtu which MUST continue to
 * remain accessible until the mapping is subsequently cleared or
 * changed (explicitly by call to RL_MapRtu/RL_LoadDefaultRtus, or,
 * implicitly by customizing the unit configuration through one of the
 * RL_RTU* family of functions (at which point a copy will be performed).
 */
void RL_MapRtu(uint idx, const rtexmapunit_t* rtu);

/**
 * Copy the configuration for the identified @a idx texture unit of
 * the primitive writer's internal state from @a rtu.
 *
 * @param idx  Logical index of the texture unit being configured.
 * @param rtu  Configured RTU state to copy the configuration from.
 */
void RL_CopyRtu(uint idx, const rtexmapunit_t* rtu);

/// @todo Avoid modifying the RTU write state for the purposes of primitive
///       specific translations by implementing these as arguments to the
///       RL_Add* family of functions.

/// Change the scale property of the identified @a idx texture unit.
void RL_Rtu_SetScale(uint idx, float s, float t);
void RL_Rtu_SetScalev(uint idx, float const st[2]);

/// Scale the offset and scale properties of the identified @a idx texture unit.
void RL_Rtu_Scale(uint idx, float scalar);
void RL_Rtu_ScaleST(uint idx, float const st[2]);

/// Change the offset property of the identified @a idx texture unit.
void RL_Rtu_SetOffset(uint idx, float s, float t);
void RL_Rtu_SetOffsetv(uint idx, float const st[2]);

/// Translate the offset property of the identified @a idx texture unit.
void RL_Rtu_TranslateOffset(uint idx, float s, float t);
void RL_Rtu_TranslateOffsetv(uint idx, float const st[2]);

/// Change the texture assigned to the identified @a idx texture unit.
void RL_Rtu_SetTextureUnmanaged(uint idx, DGLuint glName, int wrapS, int wrapT);

/**
 * @param primType  Type of primitive being written.
 * @param flags  @ref rendpolyFlags
 * @param colors  Color data values for the primitive. If @c NULL the default
 *                value set [R:255, G:255, B:255, A:255] will be used for all
 *                vertices of the primitive.
 */
void RL_AddPolyWithCoordsModulationReflection(primtype_t primType, int flags,
    uint numElements, const rvertex_t* vertices, const ColorRawf* colors,
    const rtexcoord_t* primaryCoords, const rtexcoord_t* interCoords,
    DGLuint modTex, const ColorRawf* modColor, const rtexcoord_t* modCoords,
    const ColorRawf* reflectionColors, const rtexcoord_t* reflectionCoords,
    const rtexcoord_t* reflectionMaskCoords);

/**
 * @param primType  Type of primitive being written.
 * @param flags  @ref rendpolyFlags
 * @param colors  Color data values for the primitive. If @c NULL the default
 *                value set [R:255, G:255, B:255, A:255] will be used for all
 *                vertices of the primitive.
 */
void RL_AddPolyWithCoordsModulation(primtype_t primType, int flags,
    uint numElements, const rvertex_t* vertices, const ColorRawf* colors,
    const rtexcoord_t* primaryCoords, const rtexcoord_t* interCoords,
    DGLuint modTex, const ColorRawf* modColor, const rtexcoord_t* modCoords);

/**
 * @param primType  Type of primitive being written.
 * @param flags  @ref rendpolyFlags
 * @param colors  Color data values for the primitive. If @c NULL the default
 *                value set [R:255, G:255, B:255, A:255] will be used for all
 *                vertices of the primitive.
 */
void RL_AddPolyWithCoords(primtype_t primType, int flags, uint numElements,
    const rvertex_t* vertices, const ColorRawf* colors,
    const rtexcoord_t* primaryCoords, const rtexcoord_t* interCoords);

/**
 * @param primType  Type of primitive being written.
 * @param flags  @ref rendpolyFlags
 * @param colors  Color data values for the primitive. If @c NULL the default
 *                value set [R:255, G:255, B:255, A:255] will be used for all
 *                vertices of the primitive.
 */
void RL_AddPolyWithModulation(primtype_t primType, int flags, uint numElements,
    const rvertex_t* vertices, const ColorRawf* colors,
    DGLuint modTex, const ColorRawf* modColor, const rtexcoord_t* modCoords);

/**
 * @param primType  Type of primitive being written.
 * @param flags  @ref rendpolyFlags
 * @param colors  Color data values for the primitive. If @c NULL the default
 *                value set [R:255, G:255, B:255, A:255] will be used for all
 *                vertices of the primitive.
 */
void RL_AddPoly(primtype_t primType, int flags, uint numElements,
    const rvertex_t* vertices, const ColorRawf* colors);

void RL_RenderAllLists(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_REND_LIST_H */
