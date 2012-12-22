/**
 * @file materialvariant.h
 * Logical material variant. @ingroup resource
 *
 * @authors Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIALVARIANT_H
#define LIBDENG_RESOURCE_MATERIALVARIANT_H

#include "render/rendpoly.h"
#include "materials.h"

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

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
namespace de {

class MaterialVariant
{
public:
    MaterialVariant(material_t& generalCase, const materialvariantspecification_t& spec,
                    const ded_material_t& def);
    ~MaterialVariant();

    /**
     * Process a system tick event.
     * @param time  Length of the tick in seconds.
     */
    void ticker(timespan_t time);

    /**
     * Reset the staged animation point for this Material.
     */
    void resetAnim();

    /// @return  Material from which this variant is derived.
    material_t* generalCase() const { return material; }

    /// @return  MaterialVariantSpecification from which this variant is derived.
    const materialvariantspecification_t* spec() const { return varSpec; }

    /**
     * Retrieve a handle for a staged animation layer form this variant.
     * @param layer  Index of the layer to retrieve.
     * @return  MaterialVariantLayer for the specified layer index.
     */
    const materialvariant_layer_t* layer(int layer);

    /**
     * Attach MaterialSnapshot data to this. MaterialVariant is given ownership of @a materialSnapshot.
     * @return  Same as @a materialSnapshot for caller convenience.
     */
    materialsnapshot_t& attachSnapshot(materialsnapshot_t& materialSnapshot);

    /**
     * Detach MaterialSnapshot data from this. Ownership of the data is relinquished to the caller.
     */
    materialsnapshot_t* detachSnapshot();

    /// @return  MaterialSnapshot data associated with this.
    materialsnapshot_t* snapshot() const { return snapshot_; }

    /// @return  Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame() const { return snapshotPrepareFrame_; }

    /**
     * Change the frame when the snapshot was last prepared/updated.
     * @param frame  Frame to mark the snapshot with.
     */
    void setSnapshotPrepareFrame(int frame);

    /// @return  Translated 'next' (or target) MaterialVariant if set, else this.
    MaterialVariant* translationNext();

    /// @return  Translated 'current' MaterialVariant if set, else this.
    MaterialVariant* translationCurrent();

    /// @return  Translation position [0..1]
    float translationPoint();

    /**
     * Change the translation target for this variant.
     *
     * @param current  Translated 'current' MaterialVariant.
     * @param next  Translated 'next' (or target) MaterialVariant.
     */
    void setTranslation(MaterialVariant* current, MaterialVariant* next);

    /**
     * Change the translation point for this variant.
     * @param inter  Translation point.
     */
    void setTranslationPoint(float inter);

private:
    /// Superior Material of which this is a derivative.
    material_t* material;

    /// For "smoothed" Material animation:
    bool hasTranslation;
    MaterialVariant* current;
    MaterialVariant* next;
    float inter;

    /// Specification used to derive this variant.
    const materialvariantspecification_t* varSpec;

    /// Cached copy of current state if any.
    materialsnapshot_t* snapshot_;

    /// Frame count when the snapshot was last prepared/updated.
    int snapshotPrepareFrame_;

    materialvariant_layer_t layers[MATERIALVARIANT_MAXLAYERS];
};

} // namespace de

extern "C" {
#endif

/**
 * C wrapper API:
 */

struct materialvariant_s; // The materialvariant instance (opaque).
typedef struct materialvariant_s MaterialVariant;

MaterialVariant* MaterialVariant_New(struct material_s* generalCase,
    const materialvariantspecification_t* spec);
void MaterialVariant_Delete(MaterialVariant* mat);

void MaterialVariant_Ticker(MaterialVariant* mat, timespan_t time);

void MaterialVariant_ResetAnim(MaterialVariant* mat);

material_t* MaterialVariant_GeneralCase(MaterialVariant* mat);
const materialvariantspecification_t* MaterialVariant_Spec(const MaterialVariant* mat);
const materialvariant_layer_t* MaterialVariant_Layer(MaterialVariant* mat, int layer);

materialsnapshot_t* MaterialVariant_AttachSnapshot(MaterialVariant* mat, materialsnapshot_t* materialSnapshot);
materialsnapshot_t* MaterialVariant_DetachSnapshot(MaterialVariant* mat);
materialsnapshot_t* MaterialVariant_Snapshot(const MaterialVariant* mat);

int MaterialVariant_SnapshotPrepareFrame(const MaterialVariant* mat);
void MaterialVariant_SetSnapshotPrepareFrame(MaterialVariant* mat, int frame);

MaterialVariant* MaterialVariant_TranslationNext(MaterialVariant* mat);
MaterialVariant* MaterialVariant_TranslationCurrent(MaterialVariant* mat);

float MaterialVariant_TranslationPoint(MaterialVariant* mat);

void MaterialVariant_SetTranslation(MaterialVariant* mat,
    MaterialVariant* current, MaterialVariant* next);

void MaterialVariant_SetTranslationPoint(MaterialVariant* mat, float inter);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RESOURCE_MATERIALVARIANT_H */
