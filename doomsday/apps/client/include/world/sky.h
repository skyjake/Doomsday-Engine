/** @file sky.h
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#pragma once

#include <doomsday/world/sky.h>

class Sky : public world::Sky
    , DE_OBSERVES(world::Sky::Layer, ActiveChange)
    , DE_OBSERVES(world::Sky::Layer, MaterialChange)
    , DE_OBSERVES(world::Sky::Layer, MaskedChange)
{
public:
    explicit Sky(const defn::Sky *definition = nullptr);

    void configure(const defn::Sky *def) override;

    /**
     * Returns the ambient color of the sky. The ambient color is automatically calculated
     * by averaging the color information in the configured layer material textures.
     *
     * Alternatively, this color can be overridden manually by calling @ref setAmbientColor().
     */
    const de::Vec3f &ambientColor() const;

    /**
     * Override the automatically calculated ambient color.
     *
     * @param newColor  New ambient color to apply (will be normalized).
     *
     * @see ambientColor()
     */
    void setAmbientColor(const de::Vec3f &newColor);

private:
    void skyLayerActiveChanged(Layer &) override;
    void skyLayerMaterialChanged(Layer &layer) override;
    void skyLayerMaskedChanged(Layer &layer) override;

    void updateAmbientLightIfNeeded();

    /*
     * Ambient lighting characteristics.
     */
    struct AmbientLight
    {
        bool custom     = false;  /// @c true= defined in a MapInfo def.
        bool needUpdate = true;   /// @c true= update if not custom.
        de::Vec3f color;

        void setColor(const de::Vec3f &newColor, bool isCustom = true);
        void reset();
    } ambientLight;
};
