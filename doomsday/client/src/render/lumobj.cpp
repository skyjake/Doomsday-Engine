/** @file lumobj.cpp Luminous object.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "de_console.h"
#include "world/map.h"

#include "render/lumobj.h"

using namespace de;

static int radiusMax     = 256; ///< Absolute maximum lumobj radius (cvar).
static float radiusScale = 3;   ///< Radius scale factor (cvar).

DENG2_PIMPL_NOREF(Lumobj)
{
    Vector3d origin;    ///< Position in map space.
    BspLeaf *bspLeaf;   ///< BSP leaf at @ref origin in the map (not owned).
    double maxDistance; ///< Used when rendering to limit the number drawn lumobjs.
    Vector3f color;     ///< Light color/intensity.
    double radius;      ///< Radius in map space units.
    double zOffset;     ///< Z-axis origin offset in map space units.

    /// Custom lightmaps (if any, not owned):
    Texture *sideTex;
    Texture *downTex;
    Texture *upTex;

    Instance()
        : bspLeaf    (0),
          maxDistance(0),
          color      (Vector3f(1, 1, 1)),
          radius     (256),
          zOffset    (0),
          sideTex    (0),
          downTex    (0),
          upTex      (0)
    {}

    Instance(Instance const &other)
        : origin     (other.origin),
          bspLeaf    (other.bspLeaf),
          maxDistance(other.maxDistance),
          color      (other.color),
          radius     (other.radius),
          zOffset    (other.zOffset),
          sideTex    (other.sideTex),
          downTex    (other.downTex),
          upTex      (other.upTex)
    {}
};

Lumobj::Lumobj(Vector3d const &origin, double radius, Vector3f const &color, double maxDistance)
    : MapObject(), d(new Instance())
{
    setOrigin     (origin);
    setRadius     (radius);
    setColor      (color);
    setMaxDistance(maxDistance);
}

Lumobj::Lumobj(Lumobj const &other) : MapObject(), d(new Instance(*other.d))
{}

Vector3d const &Lumobj::origin() const
{
    return d->origin;
}

Lumobj &Lumobj::setOrigin(Vector3d const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        // When moving on the XY plane; invalidate the BSP leaf.
        if(!de::fequal(d->origin.x, newOrigin.x) ||
           !de::fequal(d->origin.y, newOrigin.y))
        {
            d->bspLeaf = 0;
        }

        d->origin = newOrigin;
    }
    return *this;
}

void Lumobj::move(Vector3d const &delta)
{
    setOrigin(d->origin + delta);
}

BspLeaf &Lumobj::bspLeafAtOrigin() const
{
    if(!d->bspLeaf)
    {
        // Determine this now.
        d->bspLeaf = &map().bspLeafAt(d->origin);
    }
    return *d->bspLeaf;
}

de::Vector3f const &Lumobj::color() const
{
    return d->color;
}

Lumobj &Lumobj::setColor(Vector3f const &newColor)
{
    if(d->color != newColor)
    {
        d->color = newColor;
    }
    return *this;
}

double Lumobj::radius() const
{
    return d->radius;
}

Lumobj &Lumobj::setRadius(double newRadius)
{
    // Apply the global scale factor.
    newRadius *= 40 * ::radiusScale;

    // Normalize.
    newRadius = de::clamp<double>(.0001, de::abs(newRadius), ::radiusMax);

    if(d->radius != newRadius)
    {
        d->radius = newRadius;
    }
    return *this;
}

double Lumobj::zOffset() const
{
    return d->zOffset;
}

Lumobj &Lumobj::setZOffset(double newZOffset)
{
    d->zOffset = newZOffset;
    return *this;
}

double Lumobj::maxDistance() const
{
    return d->maxDistance;
}

Lumobj &Lumobj::setMaxDistance(double newMaxDistance)
{
    d->maxDistance = newMaxDistance;
    return *this;
}

Texture *Lumobj::lightmap(LightmapSemantic semantic) const
{
    switch(semantic)
    {
    case Side: return d->sideTex;
    case Down: return d->downTex;
    case Up:   return d->upTex;
    };
    DENG_ASSERT(false);
    return d->sideTex;
}

Lumobj &Lumobj::setLightmap(LightmapSemantic semantic, Texture *newTexture)
{
    switch(semantic)
    {
    case Side: d->sideTex = newTexture; break;
    case Down: d->downTex = newTexture; break;
    case Up:   d->upTex   = newTexture; break;
    };
    return *this;
}

float Lumobj::attenuation(double distance) const
{
    if(distance > 0 && d->maxDistance > 0)
    {
        if(distance > d->maxDistance) return 0;
        if(distance > .67 * d->maxDistance)
            return (d->maxDistance - distance) / (.33 * d->maxDistance);
    }
    return 1;
}

void Lumobj::consoleRegister() // static
{
    C_VAR_INT  ("rend-light-radius-max",   &::radiusMax,   0, 64,  512);
    C_VAR_FLOAT("rend-light-radius-scale", &::radiusScale, 0, .1f, 10);
}

int Lumobj::radiusMax() // static
{
    return ::radiusMax;
}

float Lumobj::radiusFactor() // static
{
    return ::radiusScale;
}
