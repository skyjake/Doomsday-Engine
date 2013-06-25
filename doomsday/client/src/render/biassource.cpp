/** @file biassource.cpp Shadow Bias (light) source.
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

#include "dd_main.h"
#include "def_data.h"

#include "world/world.h"
#include "world/map.h"
#include "BspLeaf"
#include "Sector"

#include "render/rend_bias.h" // BiasTracker

#include "render/biassource.h"

using namespace de;

DENG2_PIMPL(BiasSource)
{
    /// Origin of the source in the map coordinate space.
    Vector3d origin;

    /// BSP leaf at this origin.
    BspLeaf *bspLeaf;

    /// Intensity of the emitted light.
    float primaryIntensity;

    /// Effective intensity of the light scaled by the ambient level threshold.
    float intensity;

    /// Color strength factors of the emitted light.
    Vector3f color;

    /// Ambient light level threshold.
    float minLight, maxLight;

    /// Time in milliseconds of the last update.
    uint lastUpdateTime;

    /// Flags:
    bool changed;

    Instance(Public *i, Vector3d const &origin, float intensity,
             Vector3f const &color, float minLight, float maxLight)
        : Base(i),
          origin(origin),
          bspLeaf(0),
          primaryIntensity(intensity),
          intensity(intensity),
          color(color),
          minLight(minLight),
          maxLight(maxLight),
          lastUpdateTime(0), // Force an update (vertices are STILL_UNSEEN).
          changed(true)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i),
          origin(other.origin),
          bspLeaf(other.bspLeaf),
          primaryIntensity(other.primaryIntensity),
          intensity(other.intensity),
          color(other.color),
          minLight(other.minLight),
          maxLight(other.maxLight),
          lastUpdateTime(0), // Force an update (vertices are STILL_UNSEEN).
          changed(true)
    {}

    void notifyOriginChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OriginChange, i)
        {
            i->grabbableOriginChanged(self);
        }
    }

    void notifyIntensityChanged(float oldIntensity)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(IntensityChange, i)
        {
            i->biasSourceIntensityChanged(self, oldIntensity);
        }
    }

    void notifyColorChanged(Vector3f const &oldColor)
    {
        // Predetermine which components have changed.
        int changedComponents = 0;
        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(color[i], oldColor[i]))
                changedComponents |= (1 << i);
        }

        DENG2_FOR_PUBLIC_AUDIENCE(ColorChange, i)
        {
            i->biasSourceColorChanged(self, oldColor, changedComponents);
        }
    }
};

BiasSource::BiasSource(Vector3d const &origin, float intensity, Vector3f const &color,
    float minLight, float maxLight)
    : Grabbable(), ISerializable(),
      d(new Instance(this, origin, intensity, color, minLight, maxLight))
{}

BiasSource::BiasSource(BiasSource const &other)
    : Grabbable() /*grabbable state is not copied*/, ISerializable(),
      d(new Instance(this, *other.d))
{}

BiasSource BiasSource::fromDef(ded_light_t const &def) //static
{
    return BiasSource(Vector3f(def.offset), def.size, Vector3f(def.color),
                      def.lightLevel[0], def.lightLevel[1]);
}

Vector3d const &BiasSource::origin() const
{
    return d->origin;
}

void BiasSource::setOrigin(Vector3d const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        //qDebug() << "BiasSource" << App_World().map().toIndex(*this) << "new origin" << newOrigin.asText();

        d->changed = true;
        d->origin = newOrigin;
        d->bspLeaf = 0;

        // Notify interested parties of the change.
        d->notifyOriginChanged();
    }
}

BspLeaf &BiasSource::bspLeafAtOrigin() const
{
    if(!d->bspLeaf)
    {
        /// @todo Do not assume the current map.
        d->bspLeaf = &App_World().map().bspLeafAt(d->origin);
    }
    return *d->bspLeaf;
}

float BiasSource::intensity() const
{
    return d->primaryIntensity;
}

BiasSource &BiasSource::setIntensity(float newIntensity)
{
    if(!de::fequal(d->intensity, newIntensity))
    {
        float oldIntensity = d->primaryIntensity;

        d->primaryIntensity = d->intensity = newIntensity;
        d->changed = true;

        // Notify interested parties of the change.
        d->notifyIntensityChanged(oldIntensity);
    }
    return *this;
}

Vector3f const &BiasSource::color() const
{
    return d->color;
}

BiasSource &BiasSource::setColor(Vector3f const &newColor)
{
    // Amplify the new color (but replace black with white).
    float largest = newColor[newColor.maxAxis()];
    Vector3f newColorAmplified = (largest > 0? newColor / largest : Vector3f(1, 1, 1));

    // Clamp.
    for(int i = 0; i < 3; ++i)
    {
        newColorAmplified[i] = de::clamp(0.f, newColorAmplified[i], 1.f);
    }

    if(d->color != newColorAmplified)
    {
        Vector3f oldColor = d->color;

        d->color = newColorAmplified;
        d->changed = true;

        // Notify interested parties of the change.
        d->notifyColorChanged(oldColor);
    }
    return *this;
}

void BiasSource::lightLevels(float &minLight, float &maxLight) const
{
    minLight = d->minLight;
    maxLight = d->maxLight;
}

BiasSource &BiasSource::setLightLevels(float newMinLight, float newMaxLight)
{
    float newMinLightClamped = de::clamp(0.f, newMinLight, 1.f);
    float newMaxLightClamped = de::clamp(0.f, newMaxLight, 1.f);
    if(!de::fequal(d->minLight, newMinLightClamped))
    {
        d->minLight = newMinLightClamped;
        d->changed  = true;
    }
    if(!de::fequal(d->maxLight, newMaxLightClamped))
    {
        d->maxLight = newMaxLightClamped;
        d->changed  = true;
    }
    return *this;
}

uint BiasSource::lastUpdateTime() const
{
    return d->lastUpdateTime;
}

void BiasSource::forceUpdate()
{
    d->changed = true;
}

bool BiasSource::trackChanges(BiasTracker &changes, uint indexInTracker, uint currentTime)
{
    if(d->maxLight > 0 || d->minLight > 0)
    {
        float const oldIntensity = d->intensity;

        /// @todo Should observe Sector::LightLevelChange
        Sector const &sector = bspLeafAtOrigin().sector();

        // Lower intensities are useless for light emission.
        if(sector.lightLevel() >= d->maxLight)
        {
            d->intensity = d->primaryIntensity;
        }

        if(sector.lightLevel() >= d->minLight && d->minLight != d->maxLight)
        {
            d->intensity = d->primaryIntensity *
                (sector.lightLevel() - d->minLight) / (d->maxLight - d->minLight);
        }
        else
        {
            d->intensity = 0;
        }

        if(d->intensity != oldIntensity)
            d->changed = true;
    }

    if(!d->changed) return false;

    d->changed = false;
    d->lastUpdateTime = currentTime; // Used for interpolation.

    changes.mark(indexInTracker);

    return true; // Changes were applied.
}

void BiasSource::operator >> (de::Writer &to) const
{
    to << d->origin << d->primaryIntensity << d->color << d->minLight << d->maxLight;
}

void BiasSource::operator << (de::Reader &from)
{
    Vector3d newOrigin; from >> newOrigin;
    setOrigin(newOrigin);

    float newIntensity; from >> newIntensity;
    setIntensity(newIntensity);

    Vector3f newColor; from >> newColor;
    setColor(newColor);

    float minLight, maxLight; from >> minLight >> maxLight;
    setLightLevels(minLight, maxLight);
}
