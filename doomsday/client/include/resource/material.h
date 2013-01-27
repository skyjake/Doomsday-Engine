/** @file material.h Logical material.
 *
 * @authors Copyright � 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright � 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include <QSize>
#include <de/Error>
#include <de/Vector>
#include "uri.hh"

// Forward declarations.
enum audioenvironmentclass_e;
namespace de {
class MaterialManifest;
class MaterialSnapshot;
struct MaterialVariantSpec;
class Texture;
}

/**
 * Logical material resource.
 * @ingroup resource
 */
class Material : public de::MapElement
{
    struct Instance; // Needs to be friended by Variant

    /// Internal typedefs for code brevity/cleanliness.
    typedef de::MaterialManifest Manifest;
    typedef de::MaterialSnapshot Snapshot;
    typedef de::MaterialVariantSpec VariantSpec;

public:
    /// Maximum number of layers a material supports.
    static int const max_layers = DED_MAX_MATERIAL_LAYERS;

    /// Maximum number of (light) decorations a material supports.
    static int const max_decorations = DED_MAX_MATERIAL_DECORATIONS;

    /// The referenced layer does not exist. @ingroup errors
    DENG2_ERROR(UnknownLayerError);

    /// The referenced decoration does not exist. @ingroup errors
    DENG2_ERROR(UnknownDecorationError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /**
     * Context-specialized variant. Encapsulates all context variant values
     * and logics pertaining to a specialized version of the @em superior
     * material instance.
     *
     * Variant instances are usually only created by the superior material
     * when asked to @ref Material::prepare() for render using a context
     * specialization specification which it cannot fulfill/match.
     *
     * @see MaterialVariantSpec
     */
    class Variant
    {
    public:
        /// Current state of a material layer animation.
        struct LayerState
        {
            /// Animation stage else @c -1 => layer not in use.
            int stage;

            /// Remaining (sharp) tics in the current stage.
            short tics;

            /// Intermark from the current stage to the next [0..1].
            float inter;
        };

        /// Current state of a material (light) decoration animation.
        struct DecorationState
        {
            /// Animation stage else @c -1 => decoration not in use.
            int stage;

            /// Remaining (sharp) tics in the current stage.
            short tics;

            /// Intermark from the current stage to the next [0..1].
            float inter;
        };

    private:
        /**
         * @param generalCase   Material from which this variant is derived.
         * @param spec          Specification used to derive this variant.
         *                      Ownership is NOT given to the Variant.
         */
        Variant(Material &generalCase, VariantSpec const &spec);
        ~Variant();

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

    public:
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
        VariantSpec const &spec() const;

        /**
         * Returns @c true if animation of the variant is currently paused
         * (e.g., the variant is for use with an in-game render context and
         * the client has paused the game).
         */
        bool isPaused() const;

        /**
         * Prepare the context variant for render (if necessary, uploading
         * GL textures and updating the state snapshot).
         *
         * @param forceSnapshotUpdate  @c true= Force an update of the state
         *      snapshot. The snapshot is automatically updated when first
         *      prepared for a new render frame. Typically the only time force
         *      is needed is when the material variant has changed since.
         *
         * @return  Snapshot for the prepared context variant.
         *
         * @see Material::chooseVariant(), Material::prepare()
         */
        Snapshot const &prepare(bool forceSnapshotUpdate = false);

        /**
         * Reset the staged animation point for the material. The animation
         * states of all context variants will be rewound to the beginning.
         */
        void resetAnim();

        /**
         * Returns the current state of the layer animation @a layerNum for
         * the variant.
         */
        LayerState const &layer(int layerNum) const;

        /**
         * Returns the current state of the detail layer animation for the
         * variant.
         *
         * @see Material::isDetailed()
         */
        LayerState const &detailLayer() const;

        /**
         * Returns the current state of the shine layer animation for the
         * variant.
         *
         * @see Material::isShiny()
         */
        LayerState const &shineLayer() const;

        /**
         * Returns the current state of the (light) decoration animation
         * @a decorNum for the variant.
         */
        DecorationState const &decoration(int decorNum) const;

        /**
         * Returns the MaterialSnapshot data for the variant if present;
         * otherwise @c 0.
         */
        Snapshot *snapshot() const;

        friend class Material;
        friend struct Material::Instance;

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
        struct Stage
        {
            de::Texture *texture;
            int tics;
            float variance; // Stage variance (time).
            float glowStrength;
            float glowStrengthVariance;
            de::Vector2f texOrigin;

            Stage(de::Texture *_texture, int _tics, float _variance,
                  float _glowStrength, float _glowStrengthVariance,
                  de::Vector2f const _texOrigin)
                : texture(_texture), tics(_tics), variance(_variance),
                  glowStrength(_glowStrength),
                  glowStrengthVariance(_glowStrengthVariance),
                  texOrigin(_texOrigin)
            {}
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default layer.
         */
        Layer();
        ~Layer();

        /**
         * Construct a new layer from the specified definition.
         */
        static Layer *fromDef(ded_material_layer_t const &def);

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

    /// @todo $revise-texture-animation Merge with Material::Layer
    class DetailLayer
    {
    public:
        struct Stage
        {
            int tics;
            float variance;
            de::Texture *texture; // The file/lump with the detail texture.
            float scale;
            float strength;
            float maxDistance;

            Stage(int _tics, float _variance, de::Texture *_texture,
                  float _scale, float _strength, float _maxDistance)
                : tics(_tics), variance(_variance), texture(_texture),
                  scale(_scale), strength(_strength), maxDistance(_maxDistance)
            {}
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default layer.
         */
        DetailLayer();
        ~DetailLayer();

        /**
         * Construct a new layer from the specified definition.
         */
        static DetailLayer *fromDef(ded_detailtexture_t const &def);

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

    /// @todo $revise-texture-animation Merge with Material::Layer
    class ShineLayer
    {
    public:
        struct Stage
        {
            int tics;
            float variance;
            de::Texture *texture;
            de::Texture *maskTexture;
            blendmode_t blendMode; // Blend mode flags (bm_*).
            float shininess;
            de::Vector3f minColor;
            QSizeF maskDimensions;

            Stage(int _tics, float _variance, de::Texture *_texture,
                  de::Texture *_maskTexture, blendmode_t _blendMode,
                  float _shininess, de::Vector3f const &_minColor,
                  QSizeF const &_maskDimensions)
                : tics(_tics), variance(_variance), texture(_texture),
                  maskTexture(_maskTexture), blendMode(_blendMode),
                  shininess(_shininess), minColor(_minColor),
                  maskDimensions(_maskDimensions)
            {}
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default layer.
         */
        ShineLayer();
        ~ShineLayer();

        /**
         * Construct a new layer from the specified definition.
         */
        static ShineLayer *fromDef(ded_reflection_t const &def);

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

        Decoration(de::Vector2i const &_patternSkip,
                   de::Vector2i const &_patternOffset);

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
         *
         * @see patternOffset()
         */
        de::Vector2i const &patternSkip() const;

        /**
         * Retrieve the pattern offset for the decoration. Used with pattern
         * skip to offset the origin of the pattern.
         *
         * @see patternSkip()
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
    typedef QList<Decoration *> Decorations;

public:
    /**
     * Construct a new material.
     *
     * @param manifest  Manifest derived to yield the material.
     * @param def  Definition for the material.
     */
    Material(Manifest &manifest, ded_material_t *def);
    ~Material();

    /**
     * Returns the MaterialManifest derived to yield the material.
     */
    Manifest &manifest() const;

    /**
     * Returns @c true if the material is considered @em valid. A material is
     * only invalidated when resources it depends on (such as the definition
     * from which it was produced) are destroyed as result of runtime file
     * unloading.
     *
     * These 'orphaned' materials cannot be immediately destroyed as the game
     * may be holding on to pointers (which are considered eternal). Invalid
     * materials are instead disabled and then ignored until such time as the
     * current game is reset or changed.
     */
    bool isValid() const;

    /**
     * Process a system tick event for all context variants of the material.
     * Each if not currently paused is animated independently; layer stages
     * and (light) decorations are animated and state property values are
     * updated accordingly.
     *
     * @note If the material is not valid no animation will be done.
     *
     * @param ticLength  Length of the tick in seconds.
     *
     * @see isValid()
     */
    void ticker(timespan_t time);

    /**
     * Choose/create a variant of the material which fulfills @a spec and then
     * immediately prepare it for render (e.g., upload textures if necessary).
     *
     * Intended as a convenient shorthand of the call tree:
     * @code
     *    chooseVariant(@a spec, true)->prepare(@a forceSnapshotUpdate)
     * @endcode
     *
     * @param spec  Specification with which to derive the variant.
     * @param forceSnapshotUpdate  @c true= Force an update of the variant's
     *      state snapshot. The snapshot is automatically updated when first
     *      prepared for a new render frame. Typically the only time force is
     *      needed is when the material variant has changed since.
     *
     * @return  Snapshot for the chosen and prepared variant of Material.
     *
     * @see Materials::variantSpecForContext(), chooseVariant(), Variant::prepare()
     */
    inline Snapshot const &prepare(VariantSpec const &spec, bool forceSnapshotUpdate = false)
    {
        return chooseVariant(spec, true /*can-create*/)->prepare(forceSnapshotUpdate);
    }

    /// Returns @c true if the material has at least one animated layer.
    bool isAnimated() const;

    /// Returns @c true if the material has one or more (light) decorations.
    /// Equivalent to @code decorationCount() != 0; @endcode, for convenience.
    inline bool isDecorated() const { return decorationCount() != 0; }

    /// Returns @c true if the material has a detail texturing layer.
    bool isDetailed() const;

    /// Returns @c true if the material is considered drawable.
    bool isDrawable() const;

    /// Returns @c true if the material has a shine texturing layer.
    bool isShiny() const;

    /// Returns @c true if the material is considered @em skymasked.
    bool isSkyMasked() const;

    /// Returns @c true if one or more of the material's layers are glowing.
    bool hasGlow() const;

    /**
     * Returns the dimensions of the material in map coordinate space units.
     */
    QSize const &dimensions() const;

    /// Returns the width of the material in map coordinate space units.
    inline int width() const { return dimensions().width(); }

    /// Returns the height of the material in map coordinate space units.
    inline int height() const { return dimensions().height(); }

    /**
     * Change the world dimensions of the material.
     * @param newDimensions  New dimensions in map coordinate space units.
     */
    void setDimensions(QSize const &newDimensions);

    /**
     * Change the world width of the material.
     * @param newWidth  New width in map coordinate space units.
     */
    void setWidth(int newWidth);

    /**
     * Change the world height of the material.
     * @param newHeight  New height in map coordinate space units.
     */
    void setHeight(int height);

    /**
     * Returns the @ref materialFlags for the material.
     */
    short flags() const;

    /**
     * Change the material's flags.
     * @param flags  @ref materialFlags
     */
    void setFlags(short flags);

    /**
     * Returns the environment audio class for the material.
     */
    audioenvironmentclass_e audioEnvironment() const;

    /**
     * Change the material's environment audio class.
     * @param newEnvironment  New environment to apply.
     *
     * @todo If attached to a map Surface update accordingly!
     */
    void setAudioEnvironment(audioenvironmentclass_e newEnvironment);

    /**
     * Returns the number of material layers.
     */
    inline int layerCount() const { return layers().count(); }

    /**
     * Provides access to the list of layers for efficient traversal.
     */
    Layers const &layers() const;

    /**
     * Provides access to the detail layer.
     *
     * @see isDetailed()
     */
    DetailLayer const &detailLayer() const;

    /**
     * Provides access to the shine layer.
     *
     * @see isShiny()
     */
    ShineLayer const &shineLayer() const;

    /**
     * Returns the number of material (light) decorations.
     */
    int decorationCount() const { return decorations().count(); }

    /**
     * Add a new (light) decoration to the material.
     *
     * @param decor     Decoration to add.
     *
     * @todo Mark existing variant snapshots as 'dirty'.
     */
    void addDecoration(Decoration &decor);

    /**
     * Provides access to the list of decorations for efficient traversal.
     */
    Decorations const &decorations() const;

    /**
     * Returns the number of material variants.
     */
    inline int variantCount() const { return variants().count(); }

    /**
     * Destroys all derived variants for the material.
     */
    void clearVariants();

    /**
     * Choose/create a variant of the material which fulfills @a spec.
     *
     * @param spec      Specification for the derivation of @a material.
     * @param canCreate @c true= Create a new variant if no suitable one exists.
     *
     * @return  The chosen variant; otherwise @c 0 (if none suitable, when not creating).
     */
    Variant *chooseVariant(VariantSpec const &spec, bool canCreate = false);

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
    int getProperty(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);

    /**
     * Change the associated definition for the material.
     * @param def  New definition (can be @c 0).
     *
     * @deprecated todo: refactor away -ds
     */
    void setDefinition(ded_material_t *def);

private:
    Instance *d;
};

//} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIAL_H */
