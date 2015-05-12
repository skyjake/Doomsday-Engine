/** @file skydrawable.h  Drawable specialized for the sky.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_SKYDRAWABLE_H
#define DENG_CLIENT_RENDER_SKYDRAWABLE_H

#include <de/libcore.h>
#include <de/Error>
#include <de/Observers>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/sky.h>
#include "MaterialVariantSpec"
#include "ModelDef"

class Sky;

/**
 * Specialized drawable for Sky visualization.
 *
 * This version supports only two layers. (More would be a waste of resources?)
 *
 * @ingroup render
 */
class SkyDrawable
{
public:
    /**
     * Animates the drawable according to the configured Sky.
     */
    class Animator
    {
    public:
        /// Required layer state is missing. @ingroup errors
        DENG2_ERROR(MissingLayerStateError);

        /// Required model state is missing. @ingroup errors
        DENG2_ERROR(MissingModelStateError);

        struct LayerState
        {
            float offset;
        };

        struct ModelState
        {
            int frame;
            int timer;
            int maxTimer;
            float yaw;
        };

    public:
        Animator();
        Animator(SkyDrawable &sky);
        virtual ~Animator();

        void setSky(SkyDrawable *sky);
        SkyDrawable &sky() const;

        /**
         * Determines whether the specified animation layer state @a index is valid.
         * @see layer()
         */
        bool hasLayer(int index) const;

        /**
         * Lookup an animation layer state by it's unique @a index.
         * @see hasLayer()
         */
        LayerState &layer(int index);

        /// @copydoc layer()
        LayerState const &layer(int index) const;

        /**
         * Determines whether the specified animation model state @a index is valid.
         * @see model()
         */
        bool hasModel(int index) const;

        /**
         * Lookup an animation model state by it's unique @a index.
         * @see hasModel()
         */
        ModelState &model(int index);

        /// @copydoc model()
        ModelState const &model(int index) const;

        /**
         * Advances the animation state.
         *
         * @param elapsed  Duration of elapsed time.
         */
        void advanceTime(timespan_t elapsed);

    public:
        DENG2_PRIVATE(d)
    };

public:
    /**
     * Create a new SkyDrawable for visualization of the given @a sky.
     *
     * @param sky  Sky to visualize, if any (may be @c nullptr to configure layer).
     */
    explicit SkyDrawable(Sky const *sky = nullptr);

    /**
     * Reconfigure the drawable for visualizing the given @a sky.
     *
     * @return Reference to this drawable, for caller convenience.
     */
    SkyDrawable &configure(Sky const *sky = nullptr);

    /**
     * Returns a pointer to the configured sky, if any (may be @c nullptr).
     */
    Sky const *sky() const;

    /**
     * Render the sky.
     */
    void draw(Animator const *animator = nullptr) const;

    /**
     * Cache all the assets needed for visualizing the sky.
     */
    void cacheAssets();

    /**
     * Returns the definition for a configured model, if any (may be @a nullptr).
     *
     * @param modelIndex  Unique index of the model.
     */
    ModelDef *modelDef(int modelIndex) const;

public:
    static de::MaterialVariantSpec const &layerMaterialSpec(bool masked);

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_SKYDRAWABLE_H
