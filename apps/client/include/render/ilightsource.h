/** @file ilightsource.h  Interface for a point light source.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_ILIGHTSOURCE_H
#define DE_CLIENT_ILIGHTSOURCE_H

#include <de/libcore.h>
#include <de/vector.h>

/**
 * Interface for a light source.
 *
 * All sources of light should implement this interface. Through it, various
 * parts of the rendering subsystem can know where and what kind of light this
 * is.
 *
 * @ingroup render
 */
class ILightSource
{
public:
    /**
     * Unique identifier of the source. This can be used to uniquely identify a
     * source of light across multiple frames.
     */
    typedef de::duint32 LightId;

    /**
     * RGB color of the emitted light.
     */
    typedef de::Vec3f Colorf;

public:
    virtual ~ILightSource() = default;

    virtual LightId lightSourceId() const = 0;

    /**
     * Returns the color of the emitted light. The intensity of the light must
     * not be factored into the color values, but is instead returned separately
     * by lightSourceIntensity().
     */
    virtual Colorf lightSourceColorf() const = 0;

    /**
     * Returns the intensity of the light.
     *
     * @param viewPoint  World point from where the light is being observed if
     *                   the intensity may vary depending on the relative direction
     *                   and/or position of the viewer.
     */
    virtual float lightSourceIntensity(const de::Vec3d &viewPoint) const = 0;
};

/**
 * Interface for a point light source.
 *
 * @ingroup render
 */
class IPointLightSource : public ILightSource
{
public:
    typedef de::Vec3d Origin;

public:
    virtual ~IPointLightSource() = default;

    /**
     * Returns the position of the light source, in map units.
     */
    virtual Origin lightSourceOrigin() const = 0;

    /**
     * Returns the radius of the emitter itself, in map units. A radius of
     * zero would mean that the light emitter is an infinitely small point.
     */
    virtual float lightSourceRadius() const = 0;
};

#endif // DE_CLIENT_ILIGHTSOURCE_H
