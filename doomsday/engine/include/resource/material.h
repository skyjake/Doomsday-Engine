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
#include "def_data.h"

#include <QList>
#include <de/Error>
#include <de/Vector>
#include "uri.hh"

struct ded_material_s;
struct materialvariant_s;
struct texture_s;

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

namespace de {

// Forward declarations.
class MaterialManifest;
class MaterialSnapshot;
struct MaterialVariantSpec;

}

/**
 * Logical material resource.
 *
 * Prepared states:
 * - 0 Not yet prepared.
 * - 1 Prepared from original game textures.
 * - 2 Prepared from custom or replacement textures.
 *
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
         * @todo should only be accessible by Material (Material).
         */
        Variant(Material &generalCase, de::MaterialVariantSpec const &spec);
        ~Variant();

        /**
         * Retrieve the general case for this variant. Allows for a variant
         * reference to be used in place of a material (implicit indirection).
         *
         * @see generalCase()
         */
        inline operator Material &() const {
            return generalCase();
        }

        /// @return  Superior material from which the variant was derived.
        Material &generalCase() const;

        /// @return  Material variant specification for the variant.
        de::MaterialVariantSpec const &spec() const;

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
        de::MaterialSnapshot &attachSnapshot(de::MaterialSnapshot &snapshot);

        /**
         * Detach the MaterialSnapshot data from the variant, relinquishing
         * ownership to the caller.
         *
         * @see attachSnapshot(), detachSnapshot()
         */
        de::MaterialSnapshot *detachSnapshot();

        /**
         * Returns the MaterialSnapshot data from the variant or otherwise @c 0.
         * Ownership is unaffected.
         *
         * @see attachSnapshot(), detachSnapshot()
         */
        de::MaterialSnapshot *snapshot() const;

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

        Decoration(de::Vector2i const &_patternSkip, de::Vector2i const &_patternOffset);

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
        de::Vector2i const &patternSkip() const;

        /**
         * Retrieve the pattern offset for the decoration. Used with pattern
         * skip to offset the origin of the pattern.
         */
        de::Vector2i const &patternOffset() const;

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
        de::Vector2i patternSkip_;

        /// Pattern skip interval offsets.
        de::Vector2i patternOffset_;

        /// Animation stages.
        Stages stages_;
    };

    /// A list of decorations.
    typedef QList<Material::Decoration *> Decorations;

public:
    /**
     * Construct a new material.
     *
     * @param flags  @see materialFlags
     * @param def  Definition for the material, if any.
     * @param dimensions  Dimensions of the material in map coordinate space units.
     * @param envClass  Environment class for the material.
     */
    Material(short flags, ded_material_t *def, Size2Raw *dimensions,
             material_env_class_t envClass);
    ~Material();

    /**
     * Returns the number of material variants.
     */
    int variantCount() const;

    /**
     * Returns the number of material layers.
     */
    int layerCount() const;

    /**
     * Returns the number of material (light) decorations.
     */
    int decorationCount() const;

    /**
     * Process a system tick event.
     */
    void ticker(timespan_t time);

    /**
     * Destroys all derived material variants for the material.
     */
    void clearVariants();

    /// @return  Definition associated with this.
    struct ded_material_s *definition() const;

    /**
     * Change the associated definition.
     * @param def  New definition. Can be @c NULL.
     */
    void setDefinition(struct ded_material_s *def);

    /// Returns the dimensions of the material in map coordinate space units.
    Size2 const *dimensions() const;

    /**
     * Change the world dimensions of the material.
     * @param newDimensions  New dimensions in map coordinate space units.
     */
    void setDimensions(Size2Raw const *size);

    /// @return  Width of the material in map coordinate space units.
    int width() const;

    void setWidth(int width);

    /// @return  Height of the material in map coordinate space units.
    int height() const;

    void setHeight(int height);

    /// @return  @see materialFlags
    short flags() const;

    /**
     * Change the public Material Flags.
     * @param flags  @ref materialFlags
     */
    void setFlags(short flags);

    /**
     * Returns @c true if the material is considered to be @em valid. A material
     * can only be invalidated when resources it depends on (such as the definition
     * from which it was produced) are removed as result of runtime file unloading.
     *
     * We can't yet purge these 'orphaned' materials as the game may be holding on
     * to pointers (which are considered eternal). Instead, an invalid material is
     * ignored until such time as the current game is reset or is changed.
     */
    bool isValid() const;

    /// @return  @c true= the material has at least one animated layer.
    bool isAnimated() const;

    /// Returns @c true if the material is considered @em skymasked.
    bool isSkyMasked() const;

    /// Returns @c true if the material is considered drawable.
    bool isDrawable() const;

    /// Returns @c true if one or more (light) decorations are defined for the material.
    bool hasDecorations() const;

    /// Returns @c true if one or more of the material's layers are glowing.
    bool hasGlow() const;

    /// @return  Prepared state of this material.
    byte prepared() const;

    /**
     * Change the prepared status of this material.
     * @param state  @c 0: Not yet prepared.
     *               @c 1: Prepared from original game textures.
     *               @c 2: Prepared from custom or replacement textures.
     */
    void setPrepared(byte state);

    /// @return  Unique identifier of the MaterialManifest for the material.
    materialid_t manifestId() const;

    /**
     * Change the unique identifier of the MaterialManifest for the material.
     * @param id  New identifier.
     *
     * @todo Refactor away.
     */
    void setManifestId(materialid_t id);

    /// @return  MaterialEnvironmentClass.
    material_env_class_t environmentClass() const;

    /**
     * Change the associated environment class.
     * @todo If attached to a Map Surface update accordingly!
     * @param envClass  New MaterialEnvironmentClass.
     */
    void setEnvironmentClass(material_env_class_t envClass);

    /// @return  Detail Texture linked to this else @c NULL
    struct texture_s *detailTexture();

    /**
     * Change the Detail Texture linked to this.
     * @param tex  Texture to be linked with.
     */
    void setDetailTexture(struct texture_s *tex);

    /// @return  Detail Texture blending factor for this [0..1].
    float detailStrength();

    /**
     * Change the Detail Texture strength factor for this.
     * @param strength  New strength value (will be clamped to [0..1]).
     */
    void setDetailStrength(float strength);

    /// @return  Detail Texture scale factor for this [0..1].
    float detailScale();

    /**
     * Change the Detail Texture scale factor for this.
     * @param scale  New scale value (will be clamped to [0..1]).
     */
    void setDetailScale(float scale);

    /// @return  Shiny Texture linked to this else @c NULL
    struct texture_s *shinyTexture();

    /**
     * Change the Shiny Texture linked to this.
     * @param tex  Texture to be linked with.
     */
    void setShinyTexture(struct texture_s *tex);

    /// @return  Shiny Texture blendmode for this.
    blendmode_t shinyBlendmode();

    /**
     * Change the Shiny Texture blendmode for this.
     * @param blendmode  New blendmode value.
     */
    void setShinyBlendmode(blendmode_t blendmode);

    /// @return  Shiny Texture strength factor for this.
    float shinyStrength();

    /**
     * Change the Shiny Texture strength factor for this.
     * @param strength  New strength value (will be clamped to [0..1]).
     */
    void setShinyStrength(float strength);

    /// @return  Shiny Texture minColor (RGB triplet) for this.
    float const *shinyMinColor();

    /**
     * Change the Shiny Texture minColor for this.
     * @param colorRGB  New RGB color values (each component will be clamped to [0..1]).
     */
    void setShinyMinColor(float const colorRGB[3]);

    /// @return  ShinyMask Texture linked to this else @c NULL
    struct texture_s *shinyMaskTexture();

    /**
     * Change the material's linked shiny mask texture.
     * @param tex  Texture to be linked with.
     */
    void setShinyMaskTexture(struct texture_s *tex);

    de::MaterialManifest &manifest() const;

    /**
     * Provides access to the list of layers for efficient traversal.
     */
    Layers const &layers() const;

    /**
     * Add a new (light) decoration to the material.
     *
     * @param decor     Decoration to add.
     */
    void addDecoration(Decoration &decor);

    /**
     * Provides access to the list of decorations for efficient traversal.
     */
    Decorations const &decorations() const;

    /**
     * Choose/create a variant of the material which fulfills @a spec.
     *
     * @param spec      Specification for the derivation of @a material.
     * @param canCreate @c true= Create a new variant if no suitable one exists.
     *
     * @return  Chosen variant; otherwise @c NULL if none suitable and not creating.
     */
    Variant *chooseVariant(de::MaterialVariantSpec const &spec, bool canCreate = false);

    /**
     * Provides access to the list of variant instances for efficient traversal.
     */
    Variants const &variants() const;

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int getProperty(setargs_t *args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const *args);

private:
    struct Instance;
    Instance *d;
};

//} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIAL_H */
