#if 0

/** @file biassource.cpp  Shadow Bias (light) source.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "render/biassource.h"

#include "dd_main.h"  // App_World()
#include "world/clientserverworld.h"
#include "world/map.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "client/clientsubsector.h"

using namespace de;

namespace world {

DENG2_PIMPL(BiasSource)
{
    Vector3d origin;            ///< Origin of the source in the map coordinate space.
    BspLeaf *bspLeaf;           ///< BSP leaf at the origin.
    bool inVoid;                ///< Set to @c true if the origin is in the void.

    dfloat primaryIntensity;    ///< Intensity of the emitted light.
    dfloat intensity;           ///< Effective intensity of the light scaled by the ambient level threshold.
    Vector3f color;             ///< Color strength factors of the emitted light.
    dfloat minLight, maxLight;  ///< Ambient light level threshold.

    duint lastUpdateTime;       ///< In milliseconds. Use @c 0 to force an update.
    bool changed;               ///< Set to @c true to force re-evaluation.

    Impl(Public *i, Vector3d const &origin, dfloat intensity,
             Vector3f const &color, dfloat minLight, dfloat maxLight)
        : Base(i)
        , origin          (origin)
        , bspLeaf         (nullptr)
        , inVoid          (true)
        , primaryIntensity(intensity)
        , intensity       (intensity)
        , color           (color)
        , minLight        (minLight)
        , maxLight        (maxLight)
        , changed         (true)
    {}

    Impl(Public *i, Impl const &other)
        : Base(i)
        , origin          (other.origin)
        , bspLeaf         (other.bspLeaf)
        , inVoid          (other.inVoid)
        , primaryIntensity(other.primaryIntensity)
        , intensity       (other.intensity)
        , color           (other.color)
        , minLight        (other.minLight)
        , maxLight        (other.maxLight)
        , lastUpdateTime  (0) // Force an update.
        , changed         (true)
    {}

    void updateBspLocation()
    {
        if(bspLeaf) return;
        /// @todo Do not assume the current map.
        bspLeaf = &App_World().map().bspLeafAt(origin);

        bool newInVoidState = !(bspLeaf->hasSubspace() && bspLeaf->subspace().contains(origin));
        if(inVoid != newInVoidState)
        {
            inVoid = newInVoidState;
            intensity = inVoid? 0 : primaryIntensity;
            changed = true;
        }
    }

    bool needToObserveSectorLightLevelChanges()
    {
        updateBspLocation();
        return !inVoid && (maxLight > 0 || minLight > 0);
    }

    void notifyOriginChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OriginChange, i)
        {
            i->grabbableOriginChanged(self());
        }
    }

    void notifyIntensityChanged(dfloat oldIntensity)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(IntensityChange, i)
        {
            i->biasSourceIntensityChanged(self(), oldIntensity);
        }
    }

    void notifyColorChanged(Vector3f const &oldColor)
    {
        // Predetermine which components have changed.
        dint changedComponents = 0;
        for(dint i = 0; i < 3; ++i)
        {
            if(!de::fequal(color[i], oldColor[i]))
                changedComponents |= (1 << i);
        }

        DENG2_FOR_PUBLIC_AUDIENCE(ColorChange, i)
        {
            i->biasSourceColorChanged(self(), oldColor, changedComponents);
        }
    }
};

BiasSource::BiasSource(Vector3d const &origin, dfloat intensity, Vector3f const &color,
    dfloat minLight, dfloat maxLight)
    : Grabbable()
    , ISerializable()
    , d(new Impl(this, origin, intensity, color, minLight, maxLight))
{}

BiasSource::BiasSource(BiasSource const &other)
    : Grabbable() /*grabbable state is not copied*/
    , ISerializable()
    , d(new Impl(this, *other.d))
{}

BiasSource BiasSource::fromDef(ded_light_t const &def) //static
{
    return BiasSource(Vector3f(def.offset), def.size, Vector3f(def.color),
                      def.lightLevel[0], def.lightLevel[1]);
}

BiasSource::~BiasSource()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->biasSourceBeingDeleted(*this);
}

Vector3d const &BiasSource::origin() const
{
    return d->origin;
}

void BiasSource::setOrigin(Vector3d const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        d->changed = true;
        d->origin  = newOrigin;
        d->bspLeaf = 0;

        // Notify interested parties of the change.
        d->notifyOriginChanged();
    }
}

BspLeaf &BiasSource::bspLeafAtOrigin() const
{
    d->updateBspLocation();
    return *d->bspLeaf;
}

void BiasSource::lightLevels(dfloat &minLight, dfloat &maxLight) const
{
    minLight = d->minLight;
    maxLight = d->maxLight;
}

BiasSource &BiasSource::setLightLevels(dfloat newMinLight, dfloat newMaxLight)
{
    dfloat newMinLightClamped = de::clamp(0.f, newMinLight, 1.f);
    dfloat newMaxLightClamped = de::clamp(0.f, newMaxLight, 1.f);
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

Vector3f const &BiasSource::color() const
{
    return d->color;
}

BiasSource &BiasSource::setColor(Vector3f const &newColor)
{
    // Amplify the new color (but replace black with white).
    dfloat largest = newColor[newColor.maxAxis()];
    Vector3f newColorAmplified = (largest > 0? newColor / largest : Vector3f(1, 1, 1));

    // Clamp.
    for(dint i = 0; i < 3; ++i)
    {
        newColorAmplified[i] = de::clamp(0.f, newColorAmplified[i], 1.f);
    }

    if(d->color != newColorAmplified)
    {
        Vector3f oldColor = d->color;

        d->color   = newColorAmplified;
        d->changed = true;

        // Notify interested parties of the change.
        d->notifyColorChanged(oldColor);
    }
    return *this;
}

dfloat BiasSource::intensity() const
{
    return d->primaryIntensity;
}

BiasSource &BiasSource::setIntensity(dfloat newIntensity)
{
    if(!de::fequal(d->primaryIntensity, newIntensity))
    {
        dfloat oldIntensity = d->primaryIntensity;

        d->primaryIntensity = newIntensity;

        if(!d->inVoid)
        {
            d->intensity = d->primaryIntensity;
            d->changed   = true;
        }

        // Notify interested parties of the change.
        d->notifyIntensityChanged(oldIntensity);
    }
    return *this;
}

dfloat BiasSource::evaluateIntensity() const
{
    return d->intensity;
}

bool BiasSource::trackChanges(QBitArray &changes, duint digestIndex, duint currentTime)
{
    if(d->needToObserveSectorLightLevelChanges())
    {
        /// @todo Should observe Sector::LightLevelChange

        dfloat const oldIntensity = intensity();
        dfloat newIntensity = 0;

        if(ConvexSubspace *subspace = d->bspLeaf->subspacePtr())
        {
            auto &subsec = subspace->subsector().as<world::ClientSubsector>();

            // Lower intensities are useless for light emission.
            if(subsec.lightSourceIntensity() >= d->maxLight)
            {
                newIntensity = d->primaryIntensity;
            }

            if(subsec.lightSourceIntensity() >= d->minLight && d->minLight != d->maxLight)
            {
                newIntensity = d->primaryIntensity *
                    (subsec.lightSourceIntensity() - d->minLight) / (d->maxLight - d->minLight);
            }
        }

        if(newIntensity != oldIntensity)
        {
            d->intensity = newIntensity;
            d->changed   = true;
        }
    }

    if(!d->changed) return false;

    d->changed = false;
    d->lastUpdateTime = currentTime; // Used for interpolation.

    changes.setBit(digestIndex);

    return true;  // Changes were applied.
}

duint BiasSource::lastUpdateTime() const
{
    return d->lastUpdateTime;
}

void BiasSource::forceUpdate()
{
    d->changed = true;
}

void BiasSource::operator >> (de::Writer &to) const
{
    to << d->origin << d->primaryIntensity << d->color << d->minLight << d->maxLight;
}

void BiasSource::operator << (de::Reader &from)
{
    Vector3d newOrigin; from >> newOrigin;
    setOrigin(newOrigin);

    dfloat newIntensity; from >> newIntensity;
    setIntensity(newIntensity);

    Vector3f newColor; from >> newColor;
    setColor(newColor);

    dfloat minLight, maxLight; from >> minLight >> maxLight;
    setLightLevels(minLight, maxLight);
}

}  // namespace world

#endif
