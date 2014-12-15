/** @file material.h  Logical material resource.
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RESOURCE_MATERIAL_H
#define DENG_RESOURCE_MATERIAL_H

#include <functional>
#include <QList>
#include <de/Error>
#include <de/Observers>
#include <de/Vector>
#include "audio/s_environ.h"
#include "MapElement"
#include "world/dmuargs.h"
#ifdef __CLIENT__
#  include "MaterialVariantSpec"
#endif
#include "Texture"

class MaterialManifest;
#ifdef __CLIENT__
class MaterialAnimator;
#endif

/**
 * Logical material resource.
 *
 * @par Dimensions
 * Material dimensions are interpreted relative to the coordinate space in which
 * the material is used. For example, the dimensions of a Material in the map-surface
 * usage context are thought to be in "map/world space" units.
 *
 * @ingroup resource
 */
class Material : public de::MapElement
{
public:
    /// Notified when the material is about to be deleted.
    DENG2_DEFINE_AUDIENCE2(Deletion,         void materialBeingDeleted(Material const &material))

    /// Notified whenever the logical dimensions change.
    DENG2_DEFINE_AUDIENCE2(DimensionsChange, void materialDimensionsChanged(Material &material))

public:
    /**
     * Construct a new Material and attribute it with the given resource @a manifest.
     */
    Material(MaterialManifest &manifest);
    ~Material();

    /**
     * Returns the attributed MaterialManifest for the material.
     */
    MaterialManifest &manifest() const;

    /**
     * Returns the dimension metrics of the material.
     */
    de::Vector2i const &dimensions() const;

    inline int height() const { return dimensions().y; }
    inline int width () const { return dimensions().x; }

    /**
     * Change the world dimensions of the material to @a newDimensions.
     */
    void setDimensions(de::Vector2i const &newDimensions);

    void setHeight(int newHeight);
    void setWidth (int newWidth);

    /**
     * Returns @c true if the material is marked @em drawable.
     */
    bool isDrawable() const;

    /**
     * Returns @c true if the material is marked @em sky-masked.
     */
    bool isSkyMasked() const;

    /**
     * Returns @c true if the material is marked @em valid.
     *
     * Materials are invalidated only when dependent resources (such as the definition
     * from which it was produced) are destroyed as a result of runtime file unloading.
     *
     * These 'orphaned' materials cannot be immediately destroyed as the game may be
     * holding on to pointers (which are considered eternal). Therefore, materials are
     * invalidated (disabled) and will be ignored until they can actually be destroyed
     * (e.g., the current game is reset or changed).
     */
    bool isValid() const;

    /**
     * Change the do-not-draw property of the material according to @a yes.
     */
    void markDontDraw (bool yes = true);

    /**
     * Change the sky-masked property of the material according to @a yes.
     */
    void markSkyMasked(bool yes = true);

    /**
     * Change the is-valid property of the material according to @a yes.
     */
    void markValid    (bool yes = true);

    /**
     * Returns a human-friendly, textual name for the object.
     */
    de::String describe() const;

    /**
     * Returns a human-friendly, textual description of the full material configuration.
     */
    de::String description() const;

    /**
     * Returns the attributed audio environment identifier for the material.
     */
    AudioEnvironmentId audioEnvironment() const;

    /**
     * Change the attributed audio environment for the material to @a newEnvironment.
     */
    void setAudioEnvironment(AudioEnvironmentId newEnvironment);

public:  // Layers --------------------------------------------------------------------

    /// The referenced layer does not exist. @ingroup errors
    DENG2_ERROR(MissingLayerError);

    /// Maximum number of Layers.
    static int const max_layers = 1;

    /**
     * Base class for modelling a logical layer.
     *
     * A layer in this context is a formalized extension mechanism for customizing the
     * visual composition of a material. Layers are primarily intended for the modelling
     * of animated texture layers.
     *
     * Each material is composed from one or more layers. Layers are arranged in a stack,
     * according to the order in which they should be drawn, from the bottom-most to
     * the top-most layer.
     */
    class Layer
    {
    public:
        /// The referenced stage does not exist. @ingroup errors
        DENG2_ERROR(MissingStageError);

        /**
         * Base class for a logical layer animation stage.
         */
        struct Stage
        {
            int tics;
            float variance;  ///< Stage variance (time).

            Stage(int tics, float variance) : tics(tics), variance(variance) {}
            Stage(Stage const &other) : tics(other.tics), variance(other.variance) {}
            virtual ~Stage() {}

            DENG2_AS_IS_METHODS()

            /**
             * Returns a human-friendly, textual description of the animation stage
             * configuration.
             */
            virtual de::String description() const = 0;
        };

    public:
        virtual ~Layer();

        DENG2_AS_IS_METHODS()

        /**
         * Returns a human-friendly, textual name for the type of material layer.
         */
        virtual de::String describe() const;

        /**
         * Returns a human-friendly, textual synopsis of the material layer.
         */
        de::String description() const;

        /**
         * Returns the total number of animation stages for the material layer.
         */
        int stageCount() const;

        /**
         * Returns @c true if the material layer is animated; otherwise @c false.
         */
        inline bool isAnimated() const { return stageCount() > 1; }

        /**
         * Lookup a material layer animation Stage by it's unique @a index.
         *
         * @param index  Index of the stage to lookup. Will be cycled into valid range.
         */
        Stage &stage(int index) const;

    protected:
        typedef QList<Stage *> Stages;
        Stages _stages;
    };

    /**
     * Returns the number of material layers.
     */
    int layerCount() const;

    /**
     * Add a new layer into the appropriate layer stack position.
     *
     * @note As this alters the layer state, any existing client side MaterialAnimators
     * will need to be reconfigured/destroyed as they will no longer be valid.
     *
     * @param layer  Layer to add. Material takes ownership.
     */
    void addLayer(Layer *layer);

    /**
     * Lookup a Layer by it's unique @a index.
     */
    Layer &layer   (int index) const;
    Layer *layerPtr(int index) const;

    /**
     * Iterate through all the Layers of the material.
     *
     * @param func  Callback to make for each Layer.
     */
    de::LoopResult forAllLayers(std::function<de::LoopResult (Layer &)> func) const;

    /**
     * Destroys all the material's layers.
     *
     * @note As this alters the layer state, any existing client side MaterialAnimators
     * will need to be reconfigured/destroyed as they will no longer be valid.
     */
    void clearAllLayers();

    /// @todo Cache these analyses:
    bool hasAnimatedTextureLayers() const;
    bool hasDetailTextureLayer() const;
    bool hasGlowingTextureLayers() const;
    bool hasShineLayer() const;

#ifdef __CLIENT__
public:  // Decorations ---------------------------------------------------------------

    /// The referenced decoration does not exist. @ingroup errors
    DENG2_ERROR(MissingDecorationError);

    /**
     * Base class for modelling a logical "decoration".
     *
     * A decoration in this context is a formalized extension mechanism for "attaching"
     * additional objects to the material. Each material may have any number of attached
     * decorations.
     *
     * @par Skip Patterns
     * Normally each material decoration is repeated as many times as the material.
     * Meaning that for each time the material dimensions repeat on a given axis, each
     * of the decorations will be repeated also.
     *
     * A skip pattern allows for sparser repeats to be configured. The X and Y axes of
     * a skip pattern correspond to the horizontal and vertical axes of the material,
     * respectively.
     */
    class Decoration
    {
    public:
        /// The referenced stage does not exist. @ingroup errors
        DENG2_ERROR(MissingStageError);

        /**
         * Base class for a logical decoration animation stage.
         */
        struct Stage
        {
            int tics;
            float variance;  ///< Stage variance (time).

            Stage(int tics, float variance) : tics(tics), variance(variance) {}
            Stage(Stage const &other) : tics(other.tics), variance(other.variance) {}
            virtual ~Stage() {}

            DENG2_AS_IS_METHODS()

            /**
             * Returns a human-friendly, textual description of the animation stage
             * configuration.
             */
            virtual de::String description() const = 0;
        };

    public:
        /**
         * Construct a new material Decoration with the given skip pattern configuration.
         */
        Decoration(de::Vector2i const &patternSkip   = de::Vector2i(),
                   de::Vector2i const &patternOffset = de::Vector2i());
        virtual ~Decoration();

        DENG2_AS_IS_METHODS()

        /**
         * Returns a human-friendly, textual name for the type of material decoration.
         */
        virtual de::String describe() const;

        /**
         * Returns a human-friendly, textual synopsis of the material decoration.
         */
        de::String description() const;

        /**
         * Returns the Material 'owner' of the material decoration.
         */
        Material       &material();
        Material const &material() const;

        void setMaterial(Material *newOwner);

        /**
         * Returns the pattern skip configuration for the decoration.
         *
         * @see patternOffset()
         */
        de::Vector2i const &patternSkip() const;

        /**
         * Returns the pattern offset configuration for the decoration.
         *
         * @see patternSkip()
         */
        de::Vector2i const &patternOffset() const;

        /**
         * Returns the total number of animation Stages for the decoration.
         */
        int stageCount() const;

        /**
         * Returns @c true if the material decoration is animated; otherwise @c false.
         */
        inline bool isAnimated() const { return stageCount() > 1; }

        /**
         * Add a @em copy of the given animation @a stage to the decoration. Ownership is
         * unaffected.
         *
         * @return  Index of the newly added stage (0 based).
         */
        int addStage(Stage const &stage);

        /**
         * Lookup an animation Stage by @a index.
         *
         * @param index  Index of the stage to lookup. Will be cycled into valid range.
         */
        Stage &stage(int index) const;

    protected:
        typedef QList<Stage *> Stages;
        Stages _stages;

    private:
        DENG2_PRIVATE(d)
    };

    /**
     * Returns the number of material Decorations.
     */
    int decorationCount() const;

    /**
     * Returns @c true if the material has one or more Decorations.
     */
    inline bool hasDecorations() const { return decorationCount() > 0; }

    /**
     * Iterate through the material Decorations.
     *
     * @param func  Callback to make for each Decoration.
     */
    de::LoopResult forAllDecorations(std::function<de::LoopResult (Decoration &)> func) const;

    /**
     * Add a new (light) decoration to the material.
     *
     * @param decor  Decoration to add. Ownership is given to Material.
     */
    void addDecoration(Decoration *decor);

    /**
     * Lookup the material Decoration by it's unique @a decorIndex.
     */
    Decoration &decoration(int decorIndex);

    /**
     * Destroys all the material's decorations.
     */
    void clearAllDecorations();

public:  // Animators -----------------------------------------------------------------

    /**
     * Returns the total number of MaterialAnimators for the material.
     */
    int animatorCount() const;

    /**
     * Determines if a MaterialAnimator exists for a material variant which fulfills @a spec.
     */
    bool hasAnimator(de::MaterialVariantSpec const &spec);

    /**
     * Find/create an MaterialAnimator for a material variant which fulfils @a spec
     *
     * @param spec  Specification for a material draw-context variant.
     *
     * @return  The (possibly, newly created) Animator.
     */
    MaterialAnimator &getAnimator(de::MaterialVariantSpec const &spec);

    /**
     * Iterate through all the MaterialAnimators for the material.
     *
     * @param func  Callback to make for each Animator.
     */
    de::LoopResult forAllAnimators(std::function<de::LoopResult (MaterialAnimator &)> func) const;

    /**
     * Destroy all the MaterialAnimators for the material.
     */
    void clearAllAnimators();

#endif // __CLIENT__

protected:
    int property(DmuArgs &args) const;

public:
    /// Register the console commands and variables of this module.
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#ifdef __CLIENT__
typedef Material::Decoration MaterialDecoration;
#endif

#endif  // DENG_RESOURCE_MATERIAL_H
