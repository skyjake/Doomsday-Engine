/**\file materialvariant.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MATERIALVARIANT_H
#define LIBDENG_MATERIALVARIANT_H

#include "r_data.h"

#ifdef __cplusplus
extern "C" {
#endif

struct texturevariant_s;
struct texturevariantspecification_s;
struct materialvariant_s;

typedef struct materialvariantspecification_s {
    materialcontext_t context;
    struct texturevariantspecification_s* primarySpec;
} materialvariantspecification_t;

#define MATERIALVARIANT_MAXLAYERS       DDMAX_MATERIAL_LAYERS

// Material texture unit idents:
enum {
    MTU_PRIMARY,
    MTU_DETAIL,
    MTU_REFLECTION,
    MTU_REFLECTION_MASK,
    NUM_MATERIAL_TEXTURE_UNITS
};

typedef struct materialsnapshot_s {
    /// Variant Material used to derive this snapshot.
    struct materialvariant_s* material;

    /// @c true= this material is entirely opaque.
    boolean isOpaque;

    /// Size in world units.
    Size2Raw size;

    /// Glow strength multiplier.
    float glowing;

    /// Minimum sector light color for shiny texturing.
    vec3f_t shinyMinColor;

    /// Textures used on each texture unit.
    struct texturevariant_s* textures[NUM_MATERIAL_TEXTURE_UNITS];

    /// Texture unit configuration.
    rtexmapunit_t units[NUM_MATERIAL_TEXTURE_UNITS];
} materialsnapshot_t;

// Helper macros for accessing MaterialSnapshot data elements.
#define MST(ms, u)             ((ms)->textures[u])
#define MSU(ms, u)             ((ms)->units[u])

#define MSU_texture(ms, u)     (MST(ms, u)? TextureVariant_GeneralCase(MST(ms, u)) : NULL)
#define MSU_gltexture(ms, u)   (MST(ms, u)? TextureVariant_GLName(MST(ms, u)) : 0)
#define MSU_texturespec(ms, u) (MST(ms, u)? TextureVariant_Spec(MST(ms, u)) : NULL)

typedef struct materialvariant_layer_s {
    int stage; // -1 => layer not in use.
    struct texture_s* texture;
    float texOrigin[2]; /// Origin of the texture in material-space.
    float glow;
    short tics;
} materialvariant_layer_t;

typedef struct materialvariant_s materialvariant_t;

materialvariant_t* MaterialVariant_New(struct material_s* generalCase,
    const materialvariantspecification_t* spec);

void MaterialVariant_Delete(materialvariant_t* mat);

/**
 * Process a system tick event.
 * @param ticLength  Length of the tick in seconds.
 */
void MaterialVariant_Ticker(materialvariant_t* mat, timespan_t time);

/**
 * Reset the staged animation point for this Material.
 */
void MaterialVariant_ResetAnim(materialvariant_t* mat);

/// @return  Material from which this variant is derived.
struct material_s* MaterialVariant_GeneralCase(materialvariant_t* mat);

/// @return  MaterialVariantSpecification from which this variant is derived.
const materialvariantspecification_t* MaterialVariant_Spec(const materialvariant_t* mat);

/**
 * Retrieve a handle for a staged animation layer form this variant.
 * @param layer  Index of the layer to retrieve.
 * @return  MaterialVariantLayer for the specified layer index.
 */
const materialvariant_layer_t* MaterialVariant_Layer(materialvariant_t* mat, int layer);

/**
 * Attach MaterialSnapshot data to this. MaterialVariant is given ownership of @a materialSnapshot.
 * @return  Same as @a materialSnapshot for caller convenience.
 */
materialsnapshot_t* MaterialVariant_AttachSnapshot(materialvariant_t* mat, materialsnapshot_t* materialSnapshot);

/**
 * Detach MaterialSnapshot data from this. Ownership of the data is relinquished to the caller.
 */
materialsnapshot_t* MaterialVariant_DetachSnapshot(materialvariant_t* mat);

/// @return  MaterialSnapshot data associated with this.
materialsnapshot_t* MaterialVariant_Snapshot(const materialvariant_t* mat);

/// @return  Frame count when the snapshot was last prepared/updated.
int MaterialVariant_SnapshotPrepareFrame(const materialvariant_t* mat);

/**
 * Change the frame when the snapshot was last prepared/updated.
 * @param frame  Frame to mark the snapshot with.
 */
void MaterialVariant_SetSnapshotPrepareFrame(materialvariant_t* mat, int frame);

/// @return  Translated 'next' (or target) MaterialVariant if set, else this.
materialvariant_t* MaterialVariant_TranslationNext(materialvariant_t* mat);

/// @return  Translated 'current' MaterialVariant if set, else this.
materialvariant_t* MaterialVariant_TranslationCurrent(materialvariant_t* mat);

/// @return  Translation position [0..1]
float MaterialVariant_TranslationPoint(materialvariant_t* mat);

/**
 * Change the translation target for this variant.
 *
 * @param current  Translated 'current' MaterialVariant.
 * @param next  Translated 'next' (or target) MaterialVariant.
 */
void MaterialVariant_SetTranslation(materialvariant_t* mat,
    materialvariant_t* current, materialvariant_t* next);

/**
 * Change the translation point for this variant.
 * @param inter  Translation point.
 */
void MaterialVariant_SetTranslationPoint(materialvariant_t* mat, float inter);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_MATERIALVARIANT_H */
