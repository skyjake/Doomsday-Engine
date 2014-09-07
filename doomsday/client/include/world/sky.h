/** @file sky.h  Sky behavior logic for the world system.
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

#ifndef DENG_WORLD_SKY_H
#define DENG_WORLD_SKY_H

#include <QList>
#include <de/libcore.h>
#include <de/Error>
#include <de/Observers>
#include <de/Vector>
#include <doomsday/defs/ded.h>
#include <doomsday/defs/sky.h>
#include "MapElement"
#include "Material"
#include "ModelDef"

#define DEFAULT_SKY_HORIZON_OFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_MATERIAL      ( "Textures:SKY1" )

/**
 * Behavior logic for a sky in the world system.
 *
 * @ingroup world
 */
class Sky : public de::MapElement
{
public:
    /// Notified whenever the height changes.
    DENG2_DEFINE_AUDIENCE2(HeightChange, void skyHeightChanged(Sky &sky))

    /// Notified whenever the horizon offset changes.
    DENG2_DEFINE_AUDIENCE2(HorizonOffsetChange, void skyHorizonOffsetChanged(Sky &sky))

    /**
     * Multiple layers can be used for parallax effects.
     */
    class Layer
    {
    public:
        /// Notified whenever the active-state changes.
        DENG2_DEFINE_AUDIENCE2(ActiveChange, void skyLayerActiveChanged(Layer &layer))

        /// Notified whenever the masked-state changes.
        DENG2_DEFINE_AUDIENCE2(MaskedChange, void skyLayerMaskedChanged(Layer &layer))

        /// Notified whenever the layer material changes.
        DENG2_DEFINE_AUDIENCE2(MaterialChange, void skyLayerMaterialChanged(Layer &layer))

    public:
        /**
         * Construct a new sky layer.
         */
        Layer(Sky &sky, Material *material = 0);

        /**
         * Returns the sky of which this is a layer.
         */
        Sky &sky() const;

        /**
         * Returns @a true of the layer is currently active.
         *
         * @see setActive()
         */
        bool isActive() const;

        /**
         * Change the 'active' state of the layer. The ActiveChange audience is
         * notified whenever the 'active' state changes.
         *
         * @see isActive()
         */
        void setActive(bool yes);

        inline void enable()  { setActive(true);  }
        inline void disable() { setActive(false); }

        /**
         * Returns @c true if the layer's material will be masked.
         *
         * @see setMasked()
         */
        bool isMasked() const;

        /**
         * Change the 'masked' state of the layer. The MaskedChange audience is
         * notified whenever the 'masked' state changes.
         *
         * @see isMasked()
         */
        void setMasked(bool yes);

        /**
         * Returns the material currently assigned to the layer (if any).
         */
        Material *material() const;

        /**
         * Change the material of the layer. The MaterialChange audience is notified
         * whenever the material changes.
         */
        void setMaterial(Material *newMaterial);

        /**
         * Returns the horizontal offset for the layer.
         */
        float offset() const;

        /**
         * Change the horizontal offset for the layer.
         *
         * @param newOffset  New offset to apply.
         */
        void setOffset(float newOffset);

        /**
         * Returns the fadeout limit for the layer.
         */
        float fadeoutLimit() const;

        /**
         * Change the fadeout limit for the layer.
         *
         * @param newLimit  New fadeout limit to apply.
         */
        void setFadeoutLimit(float newLimit);

    private:
        DENG2_PRIVATE(d)
    };

    typedef QList<Layer *> Layers;

public:
    explicit Sky(defn::Sky const *definition = 0);

    /**
     * Reconfigure according to the specified @a definition if not @c NULL,
     * otherwise, reconfigure using the default values.
     *
     * @see configureDefault()
     */
    void configure(defn::Sky const *definition = 0);

    /**
     * Reconfigure the sky, returning all values to their defaults.
     *
     * @see configure()
     */
    inline void configureDefault() { configure(); }

    /**
     * Provides access to the list of sky layers, for efficient traversal.
     */
    Layers const &layers() const;

    /**
     * Convenient method of returning a sky layer by unique @a index.
     */
    inline Layer *layer(int index) const { return layers().at(index); }

    /**
     * Returns the total number of sky layers (both active and inactive).
     */
    inline int layerCount() const { return layers().count(); }

    /**
     * Returns the height of the sky as a scale factor [0..1] (@c 1 covers the view).
     *
     * @see setHeight()
     */
    float height() const;

    /**
     * Change the height scale factor for the sky.
     *
     * @param newHeight  New height scale factor to apply (will be normalized).
     *
     * @see height()
     */
    void setHeight(float newHeight);

    /**
     * Returns the horizon offset for the sky.
     *
     * @see setHorizonOffset()
     */
    float horizonOffset() const;

    /**
     * Change the horizon offset for the sky.
     *
     * @param newOffset  New horizon offset to apply.
     *
     * @see horizonOffset()
     */
    void setHorizonOffset(float newOffset);

    /**
     * Returns the unique identifier of the sky's first active layer.
     *
     * @see Layer::isActive()
     */
    int firstActiveLayer() const;

#ifdef __CLIENT__

    /**
     * Returns the ambient color of the sky. The ambient color is automatically
     * calculated by averaging the color information in the configured layer
     * material textures. Alternatively, this color can be overridden manually
     * by calling @ref setAmbientColor().
     */
    de::Vector3f const &ambientColor() const;

    /**
     * Override the automatically calculated ambient color.
     *
     * @param newColor  New ambient color to apply (will be normalized).
     *
     * @see ambientColor()
     */
    void setAmbientColor(de::Vector3f const &newColor);

#endif // __CLIENT__

protected:
    int property(DmuArgs &args) const;
    int setProperty(DmuArgs const &args);

private:
    DENG2_PRIVATE(d)
};

typedef Sky::Layer SkyLayer;

#endif // DENG_WORLD_SKY_H
