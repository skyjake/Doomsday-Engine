/** @file biassource.h Shadow Bias (light) source.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_SHADOWBIAS_SOURCE_H
#define DENG_RENDER_SHADOWBIAS_SOURCE_H

#include <de/ISerializable>
#include <de/Observers>
#include <de/Reader>
#include <de/Writer>
#include <de/Vector>

#include "Grabbable"

class BiasTracker;
class BspLeaf;
struct ded_light_s; // def_data.h

/**
 * Infinite point light source in the Shadow Bias lighting model.
 *
 * Color and intensity change notifications are intended for "off-line" usage.
 */
class BiasSource : public Grabbable, public de::ISerializable
{
public:
    /*
     * Notified when the bias source intensity changes.
     */
    DENG2_DEFINE_AUDIENCE(IntensityChange,
        void biasSourceIntensityChanged(BiasSource &biasSource, float oldIntensity))

    /*
     * Notified when the bias source color changes.
     */
    DENG2_DEFINE_AUDIENCE(ColorChange,
        void biasSourceColorChanged(BiasSource &biasSource, de::Vector3f const &oldColor,
                                    int changedComponents /*bit-field (0x1=Red, 0x2=Green, 0x4=Blue)*/))

public:
    /**
     * Construct a bias source (locked automatically).
     *
     * @param origin     Origin for the source in the map coordinate space.
     * @param intensity  Light intensity (strength) multiplier.
     * @param color      Light color strength factors.
     * @param minLight   Minimum ambient light level [0..1].
     * @param maxLight   Maximum ambient light level [0..1].
     */
    BiasSource(de::Vector3d const &origin = de::Vector3d(),
               float intensity            = 200,
               de::Vector3f const &color  = de::Vector3f(1, 1, 1),
               float minLight             = 0,
               float maxLight             = 0);

    /**
     * Construct a bias source by duplicating @a other.
     */
    BiasSource(BiasSource const &other);

    /**
     * Construct a bias source initialized from a legacy light definition.
     */
    static BiasSource fromDef(struct ded_light_s const &def);

    /**
     * Returns the origin of the source in the map coordinate space. The
     * OriginChange audience is notified whenever the origin changes.
     *
     * @see setOrigin()
     */
    de::Vector3d const &origin() const;

    /**
     * Change the origin of the source in the map coordinate space. The
     * OriginChange audience is notified whenever the origin changes.
     *
     * @param newOrigin  New origin coordinates to apply.
     *
     * @see origin()
     */
    void setOrigin(de::Vector3d const &newOrigin);

    /**
     * Returns the map BSP leaf at the origin of the source (result cached).
     */
    BspLeaf &bspLeafAtOrigin() const;

    /**
     * Returns the light intensity multiplier for the source. The
     * IntensityChange audience is notified whenever the intensity changes.
     *
     * @see setIntensity()
     */
    float intensity() const;

    /**
     * Change the light intensity multiplier for the source. If changed the
     * source is marked and any affected surfaces will be updated at the
     * beginning of the @em next render frame. The IntensityChange audience is
     * notified whenever the intensity changes.
     *
     * @param newIntensity  New intensity multiplier.
     *
     * @see intensity()
     */
    BiasSource &setIntensity(float newIntensity);

    /**
     * Returns the light color strength factors for the source. The ColorChange
     * audience is notified whenever the color changes.
     *
     * @see setColor()
     */
    de::Vector3f const &color() const;

    /**
     * Change the light color strength factors for the source. If changed the
     * source is marked and any affected surfaces will be updated at the
     * beginning of the @em next render frame. The ColorChange audience is
     * notified whenever the color changes.
     *
     * @param newColor  New color strength factors to apply. Note that this
     * value is first applified and then clamped so that all components are in
     * the range [0..1].
     *
     * @see color()
     */
    BiasSource &setColor(de::Vector3f const &newColor);

    /**
     * Returns the ambient light level threshold for the source.
     *
     * @param minLight  The minimal light level is written here.
     * @param maxLight  The maximal light level is written here.
     *
     * @see setLightLevels()
     */
    void lightLevels(float &minLight, float &maxLight) const;

    /**
     * Change the ambient light level threshold for the source. Note that both
     * values are first clamped to the range [0..1].
     *
     * @param newMinLight  New minimal light level to apply.
     * @param newMaxLight  New maximal light level to apply.
     *
     * @see lightLevels()
     */
    BiasSource &setLightLevels(float newMinLight, float newMaxLight);

    /**
     * Returns the time in milliseconds when the source was last updated.
     */
    uint lastUpdateTime() const;

    /**
     * Manually mark the source as needing a full update. Note that the actual
     * update job is deferred until the beginning of the @em next render frame.
     *
     * To be called when a surface which is affected by this source has moved.
     */
    void forceUpdate();

    /**
     * Analyze the bias source to determine whether the lighting contribution
     * to any surfaces require updating.
     *
     * @param changes         Tracker in which to populate (mark) any changes.
     * @param indexInTracker  Index to use when writing to the tracker.
     * @param currentTime     Current time in milliseconds. Will be used to
     *                        mark the bias source (if changes are found) so
     *                        that interpolation can be performed later (by
     *                        the surface(s)).
     */
    bool trackChanges(BiasTracker &changes, uint indexInTracker, uint currentTime);

    // Implements ISerializable.
    void operator >> (de::Writer &to) const;
    void operator << (de::Reader &from);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RENDER_SHADOWBIAS_SOURCE_H
