/** @file material.h Logical material.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MATERIAL_H
#define LIBDENG_RESOURCE_MATERIAL_H

#ifndef __cplusplus
#  error "resource/material.h requires C++"
#endif

#include "MapElement"
#include "map/p_mapdata.h"
#include "map/p_dmu.h"

struct ded_material_s;
struct materialvariant_s;

typedef enum {
    MEC_UNKNOWN = -1,
    MEC_FIRST = 0,
    MEC_METAL = MEC_FIRST,
    MEC_ROCK,
    MEC_WOOD,
    MEC_CLOTH,
    NUM_MATERIAL_ENV_CLASSES
} material_env_class_t;

#define VALID_MATERIAL_ENV_CLASS(v) ((v) >= MEC_FIRST && (v) < NUM_MATERIAL_ENV_CLASSES)

/**
 * Prepared states:
 * - 0 Not yet prepared.
 * - 1 Prepared from original game textures.
 * - 2 Prepared from custom or replacement textures.
 *
 * @ingroup resource
 */
struct material_s;
typedef struct material_s material_t;

#ifdef __cplusplus

#include <QList>
#include <de/Error>
#include <de/Vector>
#include "uri.hh"

namespace de {

class MaterialAnim;
class MaterialManifest;
class MaterialSnapshot;
struct MaterialVariantSpec;


/**
 * Logical material resource.
 * @ingroup resource
 */
class Material : public de::MapElement
{
public:
    /**
     * Context-specialized variant. Encapsulates all context variant values
     * and logics pertaining to a specialized version of the @em superior
     * Material instance.
     *
     * Variant instances are usually only created by the superior material
     * when asked to prepare for render (@ref Material_ChooseVariant()) using
     * a context specialization specification which it cannot fulfill/match.
     *
     * @see MaterialVariantSpec
     */
    class Variant
    {
    public:
        /// Maximum number of layers a material supports.
        static int const max_layers = DED_MAX_MATERIAL_LAYERS;

        /// Current state of a material layer.
        struct LayerState
        {
            /// Animation stage else @c -1 => layer not in use.
            int stage;

            /// Remaining (sharp) tics in the current stage.
            short tics;

            /// Intermark from the current stage to the next [0..1].
            float inter;
        };

        /// Maximum number of (light) decorations a material supports.
        static int const max_decorations = DED_MAX_MATERIAL_DECORATIONS;

        /// Current state of a material (light) decoration.
        struct DecorationState
        {
            /// Animation stage else @c -1 => decoration not in use.
            int stage;

            /// Remaining (sharp) tics in the current stage.
            short tics;

            /// Intermark from the current stage to the next [0..1].
            float inter;
        };

    public:
        /// The requested layer does not exist. @ingroup errors
        DENG2_ERROR(InvalidLayerError);

        /// The requested decoration does not exist. @ingroup errors
        DENG2_ERROR(InvalidDecorationError);

        /// Required snapshot data is missing. @ingroup errors
        DENG2_ERROR(MissingSnapshotError);

    public:
        /**
         * @param generalCase   Material from which this variant is derived.
         * @param spec          Specification used to derive this variant.
         *                      Ownership is NOT given to the Variant.
         *
         * @todo should only be accessible by material_t (Material).
         */
        Variant(material_t &generalCase, MaterialVariantSpec const &spec);
        ~Variant();

        /**
         * Retrieve the general case for this variant. Allows for a variant
         * reference to be used in place of a material (implicit indirection).
         *
         * @see generalCase()
         */
        inline operator material_t &() const {
            return generalCase();
        }

        /// @return  Superior material from which the variant was derived.
        material_t &generalCase() const;

        /// @return  Material variant specification for the variant.
        MaterialVariantSpec const &spec() const;

        /**
         * Returns @c true if animation of the variant is currently paused
         * (e.g., the variant is for use with an in-game render context and
         * the client has paused the game).
         */
        bool isPaused() const;

        /**
         * Process a system tick event. If not currently paused and the material
         * is valid; layer stages are animated and state property values are
         * updated accordingly.
         *
         * @param ticLength  Length of the tick in seconds.
         *
         * @see isPaused()
         */
        void ticker(timespan_t ticLength);

        /**
         * Reset the staged animation point for this Material.
         */
        void resetAnim();

        /**
         * Returns the current state of @a layerNum for the variant.
         */
        LayerState const &layer(int layerNum);

        /**
         * Returns the current state of @a decorNum for the variant.
         */
        DecorationState const &decoration(int decorNum);

        /**
         * Attach new MaterialSnapshot data to the variant. If an existing
         * snapshot is already present it will be replaced. Ownership of
         * @a materialSnapshot is given to the variant.
         *
         * @return  Same as @a materialSnapshot for caller convenience.
         *
         * @see detachSnapshot(), snapshot()
         */
        MaterialSnapshot &attachSnapshot(MaterialSnapshot &snapshot);

        /**
         * Detach the MaterialSnapshot data from the variant, relinquishing
         * ownership to the caller.
         *
         * @see attachSnapshot(), detachSnapshot()
         */
        MaterialSnapshot *detachSnapshot();

        /**
         * Returns the MaterialSnapshot data from the variant or otherwise @c 0.
         * Ownership is unaffected.
         *
         * @see attachSnapshot(), detachSnapshot()
         */
        MaterialSnapshot *snapshot() const;

        /**
         * Returns the frame number when the variant's associated snapshot was last
         * prepared/updated.
         */
        int snapshotPrepareFrame() const;

        /**
         * Change the frame number when the variant's snapshot was last prepared/updated.
         * @param frameNum  Frame number to mark the snapshot with.
         */
        void setSnapshotPrepareFrame(int frameNum);

    private:
        struct Instance;
        Instance *d;
    };

    /// A list of variant instances.
    typedef QList<Variant *> Variants;

    /**
     * Each material consitutes at least one layer. Layers are arranged in a
     * stack according to the order in which they should be drawn, from the
     * bottom-most to the top-most layer.
     */
    class Layer
    {
    public:
        /// A list of stages.
        typedef QList<ded_material_layer_stage_t *> Stages;

    public:
        /**
         * Construct a new default layer.
         */
        Layer();

        /**
         * Construct a new layer from the specified definition.
         */
        static Layer *fromDef(ded_material_layer_t &def);

        /**
         * Returns the total number of animation stages for the layer.
         */
        int stageCount() const;

        /**
         * Provides access to the animation stages for efficient traversal.
         */
        Stages const &stages() const;

    private:
        /// Animation stages.
        Stages stages_;
    };

    /// A list of layers.
    typedef QList<Material::Layer *> Layers;

    /**
     * (Light) decoration.
     */
    class Decoration
    {
    public:
        /// A list of stages.
        typedef QList<ded_decorlight_stage_t *> Stages;

    public:
        /**
         * Construct a new default decoration.
         */
        Decoration();

        Decoration(Vector2i const &_patternSkip, Vector2i const &_patternOffset);

        /**
         * Construct a new decoration from the specified definition.
         */
        static Decoration *fromDef(ded_material_decoration_t &def);

        /**
         * Construct a new decoration from the specified definition.
         */
        static Decoration *fromDef(ded_decoration_t &def);

        /**
         * Retrieve the pattern skip for the decoration. Normally a decoration
         * is repeated on a surface as many times as the material does. A skip
         * pattern allows sparser repeats on the horizontal and vertical axes
         * respectively.
         */
        Vector2i const &patternSkip() const;

        /**
         * Retrieve the pattern offset for the decoration. Used with pattern
         * skip to offset the origin of the pattern.
         */
        Vector2i const &patternOffset() const;

        /**
         * Returns the total number of animation stages for the decoration.
         */
        int stageCount() const;

        /**
         * Provides access to the animation stages for efficient traversal.
         */
        Stages const &stages() const;

    private:
        /// Pattern skip intervals.
        Vector2i patternSkip_;

        /// Pattern skip interval offsets.
        Vector2i patternOffset_;

        /// Animation stages.
        Stages stages_;
    };

    /// A list of decorations.
    typedef QList<Material::Decoration *> Decorations;

public:
    Material();
    ~Material();
};

} // namespace de

extern "C" {
#endif

/**
 * Construct a new material.
 *
 * @param flags  @see materialFlags
 * @param def  Definition for the material, if any.
 * @param dimensions  Dimensions of the material in map coordinate space units.
 * @param envClass  Environment class for the material.
 */
material_t *Material_New(short flags, ded_material_t *def, Size2Raw *dimensions,
                         material_env_class_t envClass);

void Material_Delete(material_t *mat);

/**
 * Returns the number of material variants.
 */
int Material_VariantCount(material_t const *mat);

/**
 * Returns the number of material layers.
 */
int Material_LayerCount(material_t const *mat);

/**
 * Returns the number of material (light) decorations.
 */
int Material_DecorationCount(material_t const *mat);

/**
 * Process a system tick event.
 */
void Material_Ticker(material_t *mat, timespan_t time);

/**
 * Destroys all derived material variants for the material.
 */
void Material_ClearVariants(material_t *mat);

/// @return  Definition associated with this.
struct ded_material_s *Material_Definition(material_t const *mat);

/**
 * Change the associated definition.
 * @param def  New definition. Can be @c NULL.
 */
void Material_SetDefinition(material_t *mat, struct ded_material_s *def);

/// Returns the dimensions of the material in map coordinate space units.
Size2 const *Material_Dimensions(material_t const *mat);

/**
 * Change the world dimensions of the material.
 * @param newDimensions  New dimensions in map coordinate space units.
 */
void Material_SetDimensions(material_t *mat, Size2Raw const *size);

/// @return  Width of the material in map coordinate space units.
int Material_Width(material_t const *mat);

void Material_SetWidth(material_t *mat, int width);

/// @return  Height of the material in map coordinate space units.
int Material_Height(material_t const *mat);

void Material_SetHeight(material_t *mat, int height);

/// @return  @see materialFlags
short Material_Flags(material_t const *mat);

/**
 * Change the public Material Flags.
 * @param flags  @ref materialFlags
 */
void Material_SetFlags(material_t *mat, short flags);

/**
 * Returns @c true if the material is considered to be @em valid. A material
 * can only be invalidated when resources it depends on (such as the definition
 * from which it was produced) are removed as result of runtime file unloading.
 *
 * We can't yet purge these 'orphaned' materials as the game may be holding on
 * to pointers (which are considered eternal). Instead, an invalid material is
 * ignored until such time as the current game is reset or is changed.
 */
boolean Material_IsValid(material_t const *mat);

/// @return  @c true= the material has at least one animated layer.
boolean Material_IsAnimated(material_t const *mat);

/// Returns @c true if the material is considered @em skymasked.
boolean Material_IsSkyMasked(material_t const *mat);

/// Returns @c true if the material is considered drawable.
boolean Material_IsDrawable(material_t const *mat);

/// Returns @c true if one or more (light) decorations are defined for the material.
boolean Material_HasDecorations(material_t const *mat);

/// Returns @c true if one or more of the material's layers are glowing.
boolean Material_HasGlow(material_t const *mat);

/// @return  Prepared state of this material.
byte Material_Prepared(material_t const *mat);

/**
 * Change the prepared status of this material.
 * @param state  @c 0: Not yet prepared.
 *               @c 1: Prepared from original game textures.
 *               @c 2: Prepared from custom or replacement textures.
 */
void Material_SetPrepared(material_t *mat, byte state);

/// @return  Unique identifier of the MaterialManifest for the material.
materialid_t Material_ManifestId(material_t const *mat);

/**
 * Change the unique identifier of the MaterialManifest for the material.
 * @param id  New identifier.
 *
 * @todo Refactor away.
 */
void Material_SetManifestId(material_t *mat, materialid_t id);

/// @return  MaterialEnvironmentClass.
material_env_class_t Material_EnvironmentClass(material_t const *mat);

/**
 * Change the associated environment class.
 * @todo If attached to a Map Surface update accordingly!
 * @param envClass  New MaterialEnvironmentClass.
 */
void Material_SetEnvironmentClass(material_t *mat, material_env_class_t envClass);

/// @return  Detail Texture linked to this else @c NULL
struct texture_s *Material_DetailTexture(material_t *mat);

/**
 * Change the Detail Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetDetailTexture(material_t *mat, struct texture_s *tex);

/// @return  Detail Texture blending factor for this [0..1].
float Material_DetailStrength(material_t *mat);

/**
 * Change the Detail Texture strength factor for this.
 * @param strength  New strength value (will be clamped to [0..1]).
 */
void Material_SetDetailStrength(material_t *mat, float strength);

/// @return  Detail Texture scale factor for this [0..1].
float Material_DetailScale(material_t *mat);

/**
 * Change the Detail Texture scale factor for this.
 * @param scale  New scale value (will be clamped to [0..1]).
 */
void Material_SetDetailScale(material_t *mat, float scale);

/// @return  Shiny Texture linked to this else @c NULL
struct texture_s *Material_ShinyTexture(material_t *mat);

/**
 * Change the Shiny Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetShinyTexture(material_t *mat, struct texture_s *tex);

/// @return  Shiny Texture blendmode for this.
blendmode_t Material_ShinyBlendmode(material_t *mat);

/**
 * Change the Shiny Texture blendmode for this.
 * @param blendmode  New blendmode value.
 */
void Material_SetShinyBlendmode(material_t *mat, blendmode_t blendmode);

/// @return  Shiny Texture strength factor for this.
float Material_ShinyStrength(material_t *mat);

/**
 * Change the Shiny Texture strength factor for this.
 * @param strength  New strength value (will be clamped to [0..1]).
 */
void Material_SetShinyStrength(material_t *mat, float strength);

/// @return  Shiny Texture minColor (RGB triplet) for this.
float const *Material_ShinyMinColor(material_t *mat);

/**
 * Change the Shiny Texture minColor for this.
 * @param colorRGB  New RGB color values (each component will be clamped to [0..1]).
 */
void Material_SetShinyMinColor(material_t *mat, float const colorRGB[3]);

/// @return  ShinyMask Texture linked to this else @c NULL
struct texture_s *Material_ShinyMaskTexture(material_t *mat);

/**
 * Change the ShinyMask Texture linked to this.
 * @param tex  Texture to be linked with.
 */
void Material_SetShinyMaskTexture(material_t *mat, struct texture_s *tex);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param material  Material instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Material_GetProperty(material_t const *material, setargs_t *args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param material  Material instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Material_SetProperty(material_t *material, setargs_t const *args);

#ifdef __cplusplus
} // extern "C"

de::MaterialManifest &Material_Manifest(material_t const *material);

/**
 * Provides access to the list of layers for efficient traversal.
 */
de::Material::Layers const &Material_Layers(material_t const *mat);

/**
 * Add a new (light) decoration to the material.
 *
 * @param mat       Material instance.
 * @param decor     Decoration to add.
 */
void Material_AddDecoration(material_t *mat, de::Material::Decoration &decor);

/**
 * Provides access to the list of decorations for efficient traversal.
 */
de::Material::Decorations const &Material_Decorations(material_t const *mat);

/**
 * Choose/create a variant of the material which fulfills @a spec.
 *
 * @param material  Material instance.
 * @param spec      Specification for the derivation of @a material.
 * @param canCreate @c true= Create a new variant if no suitable one exists.
 *
 * @return  Chosen variant; otherwise @c NULL if none suitable and not creating.
 */
de::Material::Variant *Material_ChooseVariant(material_t *mat,
    de::MaterialVariantSpec const &spec, bool canCreate = false);

/**
 * Provides access to the list of variant instances for efficient traversal.
 */
de::Material::Variants const &Material_Variants(material_t const *mat);

#endif // __cplusplus

#endif /* LIBDENG_RESOURCE_MATERIAL_H */
