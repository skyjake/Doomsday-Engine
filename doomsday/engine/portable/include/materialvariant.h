/**\file materialvariant.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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

struct texturevariantspecification_s;
struct texturevariant_s;

typedef struct materialvariantspecification_s {
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

typedef struct material_textureunit_s {
    const struct texturevariant_s* tex;
    int magMode;
    blendmode_t blendMode; /// Currently used only with reflection.
    float scale[2]; /// Material-space scale multiplier.
    float offset[2]; /// Material-space origin translation.
    float alpha;
} material_textureunit_t;

typedef struct material_snapshot_s {
    int width, height; // In world units.
    boolean isOpaque;
    boolean isDecorated;
    float color[3]; /// Average color (for lighting).
    float colorAmplified[3]; /// Average color amplified (for lighting).
    float topColor[3]; /// Average top line color (for sky fadeout).
    float glowing;
    material_textureunit_t units[NUM_MATERIAL_TEXTURE_UNITS];

    /// \todo: the following should be removed once incorporated into the layers (above).
    struct shinydata_s {
        float minColor[3];
    } shiny;
} material_snapshot_t;

#define MSU(ms, u) ((ms)->units[u])

typedef struct materialvariant_layer_s {
    int stage; // -1 => layer not in use.
    textureid_t tex;
    float texOrigin[2]; /// Origin of the texture in material-space.
    float glow;
    short tics;
} materialvariant_layer_t;

typedef struct materialvariant_s {
    materialvariant_layer_t _layers[MATERIALVARIANT_MAXLAYERS];

    /// Superior Material of which this is a derivative.
    struct material_s* _generalCase;

    /// For "smoothed" Material animation:
    struct materialvariant_s* _current;
    struct materialvariant_s* _next;
    float _inter;

    /// Specification used to derive this variant.
    materialvariantspecification_t* _spec;
} materialvariant_t;

materialvariant_t* MaterialVariant_Construct(struct material_s* generalCase,
    materialvariantspecification_t* spec);

void MaterialVariant_Destruct(materialvariant_t* mat);

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
materialvariantspecification_t* MaterialVariant_Spec(const materialvariant_t* mat);

/**
 * Retrieve a handle for a staged animation layer form this variant.
 * @param layer  Index of the layer to retrieve.
 * @return  MaterialVariantLayer for the specified layer index.
 */
const materialvariant_layer_t* MaterialVariant_Layer(materialvariant_t* mat, int layer);

/// @return  Translated 'next' (or target) MaterialVariant if set, else this.
materialvariant_t* MaterialVariant_TranslationNext(materialvariant_t* mat);

/// @return  Translated 'current' MaterialVariant if set, else this.
materialvariant_t* MaterialVariant_TranslationCurrent(materialvariant_t* mat);

/// @return  Translation position [0...1]
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

#endif /* LIBDENG_MATERIALVARIANT_H */
