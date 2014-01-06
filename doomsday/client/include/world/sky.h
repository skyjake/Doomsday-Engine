/** @file sky.h  Sky sphere and 3D models.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_WORLD_SKY_H
#define DENG_CLIENT_WORLD_SKY_H

#include "MapElement"
#include "Material"
#include <de/libdeng2.h>
#include <de/Error>
#include <de/Observers>
#include <de/Vector>
#include <QFlags>

#define MAX_SKY_LAYERS                   ( 2 )
#define MAX_SKY_MODELS                   ( 32 )

#define DEFAULT_SKY_HEIGHT               ( .666667f )
#define DEFAULT_SKY_HORIZON_OFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_XOFFSET       ( 0 )
#define DEFAULT_SKY_SPHERE_MATERIAL      ( "Textures:SKY1" )
#define DEFAULT_SKY_SPHERE_FADEOUT_LIMIT ( .3f )

/**
 * Logical sky.
 *
 * This version supports only two sky layers. (More would be a waste of resources?)
 *
 * @ingroup render
 */
class Sky : public de::MapElement
{
    DENG2_NO_COPY  (Sky)
    DENG2_NO_ASSIGN(Sky)

public:
    /// Required layer is missing. @ingroup errors
    DENG2_ERROR(MissingLayerError);

    /**
     * Multiple layers can be used for parallax effects.
     */
    class Layer
    {
    public:
        /// Notified whenever the layer material changes.
        DENG2_DEFINE_AUDIENCE(MaterialChange, void skyLayerMaterialChanged(Layer &layer))

        /// Notified whenever the active-state changes.
        DENG2_DEFINE_AUDIENCE(ActiveChange, void skyLayerActiveChanged(Layer &layer))

        /// Notified whenever the masked-state changes.
        DENG2_DEFINE_AUDIENCE(MaskedChange, void skyLayerMaskedChanged(Layer &layer))

        /// @todo make private:
        enum Flag {
            Active = 0x1, ///< Layer is active and will be rendered.
            Masked = 0x2, ///< Mask the layer's texture.

            DefaultFlags = 0
        };
        Q_DECLARE_FLAGS(Flags, Flag)

    public:
        /**
         * Construct a new sky layer.
         */
        Layer(Material *material = 0);

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
        Layer &setActive(bool yes);

        inline void enable() { setActive(true); }
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
        Layer &setMasked(bool yes);

        /**
         * Returns the material currently assigned to the layer (if any).
         */
        Material *material() const;

        /**
         * Change the material of the layer. The MaterialChange audience is notified
         * whenever the material changes.
         */
        Layer &setMaterial(Material *newMaterial);

        /**
         * Returns the horizontal offset for the layer.
         */
        float offset() const;

        /**
         * Change the horizontal offset for the layer.
         *
         * @param newOffset  New offset to apply.
         */
        Layer &setOffset(float newOffset);

        /**
         * Returns the fadeout limit for the layer.
         */
        float fadeoutLimit() const;

        /**
         * Change the fadeout limit for the layer.
         *
         * @param newLimit  New fadeout limit to apply.
         */
        Layer &setFadeoutLimit(float newLimit);

    private:
        Layer &setFlags(Flags flags, de::FlagOp operation);

        Flags _flags;
        Material *_material;
        float _offset;
        float _fadeoutLimit;
    };

public:
    Sky();

    /**
     * Reconfigure the sky, returning all values to their defaults.
     */
    void configureDefault();

    /**
     * Reconfigure the sky according the specified @a definition if not @c NULL,
     * otherwise, setup using suitable defaults.
     */
    void configure(ded_sky_t *sky);

    /**
     * Animate the sky.
     */
    void runTick(timespan_t elapsed);

    /**
     * Determines whether the specified sky layer @a index is valid.
     *
     * @see layer(), layerPtr()
     */
    bool hasLayer(int index) const;

    /**
     * Lookup a sky layer by it's unique @a index.
     *
     * @see hasLayer()
     */
    Layer &layer(int index);

    /// @copydoc layer()
    Layer const &layer(int index) const;

    /**
     * Returns a pointer to the referenced sky layer; otherwise @c 0.
     *
     * @see hasLayer(), layer()
     */
    inline Layer *layerPtr(int index) { return hasLayer(index)? &layer(index) : 0; }

    /**
     * Returns the unique identifier of the sky's first active layer.
     *
     * @see Layer::isActive()
     */
    int firstActiveLayer() const;

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
     * Returns the ambient color of the sky.
     *
     * On client side, the ambient color will be automatically calculated if none is
     * defined, by averaging  the color information in the configured layer material
     * textures. Alternatively, this color can be overridden manually by calling
     * @ref setAmbientColor().
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

#ifdef __CLIENT__

    /**
     * Cache all assets needed for visualizing the sky.
     */
    void cacheDrawableAssets();

    /**
     * Render the sky.
     *
     * @todo Extract drawing into a new render-domain component.
     */
    void draw();

#endif // __CLIENT__

protected:
    int property(DmuArgs &args) const;
    int setProperty(DmuArgs const &args);

public:
    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};
Q_DECLARE_OPERATORS_FOR_FLAGS(Sky::Layer::Flags)

typedef Sky::Layer SkyLayer;

#endif // DENG_CLIENT_WORLD_SKY_H
