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
#include <de/Vector>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/sky.h>
#include "MaterialVariantSpec"
#include "ModelDef"

class Sky;

/**
 * Sky drawable.
 *
 * This version supports only two sky layers. (More would be a waste of resources?)
 *
 * @ingroup render
 */
class SkyDrawable
{
public:
    /// Required model is missing. @ingroup errors
    DENG2_ERROR(MissingModelError);

    struct ModelData
    {
        de::Record const *def; // Sky model def
        ModelDef *model;
        int frame;
        int timer;
        int maxTimer;
        float yaw;
    };

    /**
     * Sky sphere and model animator.
     *
     * Animates a sky according to the configured definition.
     */
    class Animator
    {
    public:
        /// Required layer is missing. @ingroup errors
        DENG2_ERROR(MissingLayerError);

        struct LayerData
        {
            bool active;
            bool masked;
            float offset;
            Material *material;
            float fadeOutLimit;
        };

    public:
        Animator();
        Animator(SkyDrawable &sky);
        virtual ~Animator();

        void setSky(SkyDrawable *sky);
        SkyDrawable &sky() const;

        /**
         * @param sky  Sky to configure animation state for.
         * @return  Reference to this animator, for caller convenience.
         */
        Animator &configure(Sky const &sky);

        float height() const;
        float horizonOffset() const;

        /**
         * Determines whether the specified sky layer @a index is valid.
         * @see layer()
         */
        bool hasLayer(int index) const;

        /**
         * Lookup a sky layer by it's unique @a index.
         * @see hasLayer()
         */
        LayerData &layer(int index);

        /// @copydoc layer()
        LayerData const &layer(int index) const;

        int firstActiveLayer() const;

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
    SkyDrawable();

    /**
     * Models are set up according to the given @a skyDef.
     */
    void setupModels(defn::Sky const *skyDef = 0);

    /**
     * Cache all assets needed for visualizing the sky.
     */
    void cacheDrawableAssets(Sky const *sky = 0);

    /**
     * Render the sky.
     */
    void draw(Animator const *animator = 0) const;

    /**
     * Determines whether the specified sky model @a index is valid.
     * @see model()
     */
    bool hasModel(int index) const;

    /**
     * Lookup a sky model by it's unique @a index.
     * @see hasModel()
     */
    ModelData &model(int index);

    /// @copydoc model()
    ModelData const &model(int index) const;

public:
    static de::MaterialVariantSpec const &layerMaterialSpec(bool masked);

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_RENDER_SKYDRAWABLE_H
