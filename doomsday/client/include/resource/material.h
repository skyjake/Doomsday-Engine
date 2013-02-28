/** @file material.h Logical material resource.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "MapElement"
#include "def_data.h"
#include "map/p_mapdata.h"
#include "map/p_dmu.h"
#include "uri.hh"
#include <de/Error>
#include <de/Vector>
#include <QFlag>
#include <QList>

// Forward declarations:
enum audioenvironmentclass_e;

namespace de {

class MaterialManifest;
#ifdef __CLIENT__
class MaterialSnapshot;
struct MaterialVariantSpec;
#endif
class Texture;

}

/**
 * Logical material resource.
 *
 * @ingroup resource
 */
class Material : public de::MapElement
{
    struct Instance; // Needs to be friended by Variant (etc...).

    /// Internal typedefs for brevity/cleanliness.
    typedef de::MaterialManifest Manifest;
#ifdef __CLIENT__
    typedef de::MaterialSnapshot Snapshot;
    typedef de::MaterialVariantSpec VariantSpec;
#endif

public:
    /// Maximum number of layers a material supports.
    static int const max_layers = 1;

#ifdef __CLIENT__
    /// Maximum number of (light) decorations a material supports.
    static int const max_decorations = 16;
#endif

    /// The referenced layer does not exist. @ingroup errors
    DENG2_ERROR(UnknownLayerError);

#ifdef __CLIENT__
    /// The referenced decoration does not exist. @ingroup errors
    DENG2_ERROR(UnknownDecorationError);
#endif

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /// @todo Define these values here instead of at API level
    enum Flag
    {
        //Unused1 = MATF_UNUSED1,

        /// Map surfaces using the material should never be drawn.
        NoDraw = MATF_NO_DRAW,

        /// Apply sky masking for map surfaces using the material.
        SkyMask = MATF_SKYMASK
    };
    Q_DECLARE_FLAGS(Flags, Flag)

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

            static Stage *fromDef(ded_material_layer_stage_t const &def);
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

        /// @return  @c true if the layer is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

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

            static Stage *fromDef(ded_detail_stage_t const &def);
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

        /// @return  @c true if the layer is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

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
            de::Vector2f maskDimensions;

            Stage(int _tics, float _variance, de::Texture *_texture,
                  de::Texture *_maskTexture, blendmode_t _blendMode,
                  float _shininess, de::Vector3f const &_minColor,
                  de::Vector2f const &_maskDimensions)
                : tics(_tics), variance(_variance), texture(_texture),
                  maskTexture(_maskTexture), blendMode(_blendMode),
                  shininess(_shininess), minColor(_minColor),
                  maskDimensions(_maskDimensions)
            {}

            static Stage *fromDef(ded_shine_stage_t const &def);
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

        /// @return  @c true if the layer is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

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

#ifdef __CLIENT__

    /**
     * (Light) decoration.
     */
    class Decoration
    {
    public:
        struct Stage
        {
            struct LightLevels
            {
                float min;
                float max;

                LightLevels(float _min = 0, float _max = 0)
                    : min(_min), max(_max)
                {}

                LightLevels(float const minMax[2])
                    : min(minMax[0]), max(minMax[1])
                {}

                LightLevels(LightLevels const &other)
                    : min(other.min), max(other.max)
                {}

                /// Returns a textual representation of the lightlevels.
                de::String asText() const;
            };

            int tics;
            float variance; // Stage variance (time).
            de::Vector2f pos; // Coordinates on the surface.
            float elevation; // Distance from the surface.
            de::Vector3f color; // Light color.
            float radius; // Dynamic light radius (-1 = no light).
            float haloRadius; // Halo radius (zero = no halo).
            LightLevels lightLevels; // Fade by sector lightlevel.

            de::Texture *up, *down, *sides;
            de::Texture *flare;
            int sysFlareIdx; /// @todo Remove me

            Stage(int _tics, float _variance, de::Vector2f const &_pos, float _elevation,
                  de::Vector3f const &_color, float _radius, float _haloRadius,
                  LightLevels const &_lightLevels,
                  de::Texture *upTexture, de::Texture *downTexture, de::Texture *sidesTexture,
                  de::Texture *flareTexture,
                  int _sysFlareIdx = -1 /** @todo Remove me */)
                : tics(_tics), variance(_variance), pos(_pos), elevation(_elevation), color(_color),
                  radius(_radius), haloRadius(_haloRadius), lightLevels(_lightLevels),
                  up(upTexture), down(downTexture), sides(sidesTexture), flare(flareTexture),
                  sysFlareIdx(_sysFlareIdx)
            {}

            static Stage *fromDef(ded_decorlight_stage_t const &def);
        };

        /// A list of stages.
        typedef QList<Stage *> Stages;

    public:
        /**
         * Construct a new default decoration.
         */
        Decoration();

        Decoration(de::Vector2i const &_patternSkip,
                   de::Vector2i const &_patternOffset);

        ~Decoration();

        /**
         * Construct a new decoration from the specified definition.
         */
        static Decoration *fromDef(ded_material_decoration_t const &def);

        /**
         * Construct a new decoration from the specified definition.
         */
        static Decoration *fromDef(ded_decoration_t const &def);

        /// @return  @c true if the decoration is animated; otherwise @c false.
        inline bool isAnimated() const { return stageCount() > 1; }

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

    /**
     * Context-specialized variant. Encapsulates all context variant values
     * and logics pertaining to a specialized version of the @em superior
     * material instance.
     *
     * Variant instances are only created by the superior material when asked
     * to @ref Material::prepare() for render using a context specialization
     * specification which it cannot fulfill/match.
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
         * @param generalCase  Material from which the variant is to be derived.
         * @param spec         Specification used to derive the variant.
         */
        Variant(Material &generalCase, VariantSpec const &spec);
        ~Variant();

        /**
         * Process a system tick event. If not currently paused and still valid;
         * the variant material's layers and decorations are animated.
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
         * @return  Snapshot for the variant.
         *
         * @see Material::chooseVariant(), Material::prepare()
         */
        Snapshot const &prepare(bool forceSnapshotUpdate = false);

        /**
         * Returns the MaterialSnapshot data for the variant.
         */
        Snapshot &snapshot() const;

        /**
         * Reset the staged animation point for the material. The animation
         * states of all layers and decorations will be rewound to the beginning.
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

        friend class Material;
        friend struct Material::Instance;

    private:
        DENG2_PRIVATE(d)
    };

    /// A list of variant instances.
    typedef QList<Variant *> Variants;

#endif // __CLIENT__

public:
    /**
     * @param manifest  Manifest derived to yield the material.
     */
    Material(Manifest &manifest);
    ~Material();

    /**
     * Returns the MaterialManifest derived to yield the material.
     */
    Manifest &manifest() const;

    /**
     * Returns a brief textual description/overview of the material.
     *
     * @return Human-friendly description/overview of the material.
     */
    de::String composeDescription() const;

    /**
     * Returns a textual synopsis of the material's configuration.
     *
     * @return Human-friendly synopsis of the material's configuration.
     */
    de::String composeSynopsis() const;

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
     * Mark the material as invalid.
     *
     * @see isValid();
     */
    void markValid(bool yes);

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

#ifdef __CLIENT__

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

#endif // __CLIENT__

    /// Returns @c true if the material has at least one animated layer.
    bool isAnimated() const;

#ifdef __CLIENT__
    /// Returns @c true if the material has one or more (light) decorations.
    /// Equivalent to @code decorationCount() != 0; @endcode, for convenience.
    inline bool isDecorated() const { return decorationCount() != 0; }
#endif

    /// Returns @c true if the material has a detail texturing layer.
    bool isDetailed() const;

    /// Returns @c true if the material is considered drawable.
    bool isDrawable() const { return !isFlagged(NoDraw); }

    /// Returns @c true if the material has a shine texturing layer.
    bool isShiny() const;

    /// Returns @c true if the material is considered @em skymasked.
    inline bool isSkyMasked() const { return isFlagged(SkyMask); }

    /// Returns @c true if one or more of the material's layers are glowing.
    bool hasGlow() const;

    /**
     * Returns the dimensions of the material in map coordinate space units.
     */
    de::Vector2i const &dimensions() const;

    /// Returns the width of the material in map coordinate space units.
    inline int width() const { return dimensions().x; }

    /// Returns the height of the material in map coordinate space units.
    inline int height() const { return dimensions().y; }

    /**
     * Change the world dimensions of the material.
     * @param newDimensions  New dimensions in map coordinate space units.
     */
    void setDimensions(de::Vector2i const &newDimensions);

    /**
     * Change the world width of the material.
     * @param newWidth  New width in map coordinate space units.
     */
    void setWidth(int newWidth);

    /**
     * Change the world height of the material.
     * @param newHeight  New height in map coordinate space units.
     */
    void setHeight(int newHeight);

    /**
     * Returns @c true if the material is flagged @a flagsToTest.
     */
    inline bool isFlagged(Flags flagsToTest) const { return !!(flags() & flagsToTest); }

    /**
     * Returns the flags for the material.
     */
    Flags flags() const;

    /**
     * Change the material's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param set  @c true to set, @c false to clear.
     */
    void setFlags(Flags flagsToChange, bool set = true);

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
     * Add a new layer to the end of the material's layer stack. Ownership of the
     * layer is @em not transferred to the caller.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     *
     * @param def  Definition for the new layer. Can be @c NULL in which case a
     *             default-initialized layer will be constructed.
     */
    Layer *newLayer(ded_material_layer_t const *def);

    /**
     * Add a new detail layer to the material. Note that only one detail layer is
     * supported (any existing layer will be replaced). Ownership of the layer is
     * @em not transferred to the caller.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     *
     * @param def  Definition for the new layer. Can be @c NULL in which case a
     *             default-initialized layer will be constructed.
     */
    DetailLayer *newDetailLayer(ded_detailtexture_t const *def);

    /**
     * Add a new shine layer to the material. Note that only one shine layer is
     * supported (any existing layer will be replaced). Ownership of the layer is
     * @em not transferred to the caller.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     *
     * @param def  Definition for the new layer. Can be @c NULL in which case a
     *             default-initialized layer will be constructed.
     */
    ShineLayer *newShineLayer(ded_reflection_t const *def);

    /**
     * Returns the number of material layers.
     */
    inline int layerCount() const { return layers().count(); }

    /**
     * Destroys all the material's layers.
     *
     * @note As this invalidates the existing logical state, any previously derived
     *       context variants are cleared in the process (they will be automatically
     *       rebuilt later if/when needed).
     */
    void clearLayers();

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

#ifdef __CLIENT__

    /**
     * Returns the number of material (light) decorations.
     */
    int decorationCount() const { return decorations().count(); }

    /**
     * Destroys all the material's decorations.
     */
    void clearDecorations();

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
     * Returns the number of context variants for the material.
     */
    inline int variantCount() const { return variants().count(); }

    /**
     * Destroys all derived context variants for the material.
     */
    void clearVariants();

    /**
     * Provides access to the list of context variants for efficient traversal.
     */
    Variants const &variants() const;

    /**
     * Choose/create a context variant of the material which fulfills @a spec.
     *
     * @param spec      Specification for the derivation of @a material.
     * @param canCreate @c true= Create a new variant if no suitable one exists.
     *
     * @return  The chosen variant; otherwise @c 0 (if none suitable, when not creating).
     */
    Variant *chooseVariant(VariantSpec const &spec, bool canCreate = false);

#endif // __CLIENT__

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);

private:
    Instance *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Material::Flags)

// Aliases.
#ifdef __CLIENT__
typedef Material::Decoration MaterialDecoration;
typedef Material::Variant MaterialVariant;
#endif

#endif /* LIBDENG_RESOURCE_MATERIAL_H */
