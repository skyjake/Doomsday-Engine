/** @file material.h  Client-side material.
 *
 * @authors Copyright © 2009-2016 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_RESOURCE_CLIENTMATERIAL_H
#define DE_RESOURCE_CLIENTMATERIAL_H

#include <doomsday/world/material.h>
#include "resource/materialvariantspec.h"

class MaterialAnimator;

/**
 * Material.
 *
 * @par Dimensions
 * Material dimensions are interpreted relative to the coordinate space in which the material
 * is used. For example, the dimensions of a Material in the map-surface usage context are
 * thought to be in "map/world space" units.
 *
 * @ingroup resource
 */
class ClientMaterial : public world::Material
{
public:
    /**
     * Construct a new Material and attribute it with the given resource @a manifest.
     */
    ClientMaterial(world::MaterialManifest &manifest);

    ~ClientMaterial() override;

    bool isAnimated() const override;

    /**
     * Returns a human-friendly, textual description of the full material configuration.
     */
    de::String description() const override;

//- Decorations -------------------------------------------------------------------------

    /// The referenced decoration does not exist. @ingroup errors
    DE_ERROR(MissingDecorationError);

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
        DE_ERROR(MissingStageError);

        /**
         * Base class for a logical decoration animation stage.
         */
        struct Stage
        {
            int tics;
            float variance;  ///< Stage variance (time).

            Stage(int tics, float variance) : tics(tics), variance(variance) {}
            Stage(const Stage &other) : tics(other.tics), variance(other.variance) {}
            virtual ~Stage() {}

            DE_CAST_METHODS()

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
        Decoration(const de::Vec2i &patternSkip   = de::Vec2i(),
                   const de::Vec2i &patternOffset = de::Vec2i());
        virtual ~Decoration();

        DE_CAST_METHODS()

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
        ClientMaterial       &material();
        const ClientMaterial &material() const;

        void setMaterial(ClientMaterial *newOwner);

        /**
         * Returns the pattern skip configuration for the decoration.
         *
         * @see patternOffset()
         */
        const de::Vec2i &patternSkip() const;

        /**
         * Returns the pattern offset configuration for the decoration.
         *
         * @see patternSkip()
         */
        const de::Vec2i &patternOffset() const;

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
        int addStage(const Stage &stage);

        /**
         * Lookup an animation Stage by @a index.
         *
         * @param index  Index of the stage to lookup. Will be cycled into valid range.
         */
        Stage &stage(int index) const;

    protected:
        typedef de::List<Stage *> Stages;
        Stages _stages;

    private:
        DE_PRIVATE(d)
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
    de::LoopResult forAllDecorations(const std::function<de::LoopResult (Decoration &)> &func) const;

    /**
     * Add a new (light) decoration to the material.
     *
     * @param decor  Decoration to add. Ownership is given to Material.
     */
    void addDecoration(Decoration *decor);

    /**
     * Destroys all the material's decorations.
     */
    void clearAllDecorations();

//- Animators ---------------------------------------------------------------------------

    /**
     * Returns the total number of MaterialAnimators for the material.
     */
    int animatorCount() const;

    MaterialAnimator &animator(int index) const;

    /**
     * Determines if a MaterialAnimator exists for a material variant which fulfills @a spec.
     */
    bool hasAnimator(const de::MaterialVariantSpec &spec);

    /**
     * Find/create an MaterialAnimator for a material variant which fulfils @a spec
     *
     * @param spec  Specification for a material draw-context variant.
     *
     * @return  The (possibly, newly created) Animator.
     */
    MaterialAnimator &getAnimator(const de::MaterialVariantSpec &spec);

    /**
     * Iterate through all the MaterialAnimators for the material.
     *
     * @param func  Callback to make for each Animator.
     */
    de::LoopResult forAllAnimators(const std::function<de::LoopResult (MaterialAnimator &)> &func) const;

    /**
     * Destroy all the MaterialAnimators for the material.
     */
    void clearAllAnimators();

    static ClientMaterial &find(const res::Uri &uri);

private:
    DE_PRIVATE(d)
};

typedef ClientMaterial::Decoration MaterialDecoration;

#endif  // DE_RESOURCE_CLIENTMATERIAL_H
