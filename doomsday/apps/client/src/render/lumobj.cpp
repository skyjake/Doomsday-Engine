/** @file lumobj.cpp  Luminous object.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "render/lumobj.h"

#include <de/legacy/vector1.h>
#include <doomsday/console/var.h>

#include "render/rend_halo.h"
#include "render/vissprite.h"

#include "world/map.h"

using namespace de;

static dint radiusMax     = 320;    ///< Absolute maximum lumobj radius (cvar).
static dfloat radiusScale = 5.2f;  ///< Radius scale factor (cvar).

dfloat Lumobj::Source::occlusion(const Vec3d & /*eye*/) const
{
    return 1;  // Fully visible.
}

DE_PIMPL_NOREF(Lumobj)
{
    const Source *source = nullptr;      ///< Source of the lumobj (if any, not owned).
    const mobj_t *sourceMobj = nullptr;  ///< Mobj associated with the lumobj (if any).
    ddouble maxDistance = 0;             ///< Used when rendering to limit the number drawn lumobjs.
    Vec3f color = Vec3f(1);  ///< Light color/intensity.
    ddouble radius = 256;                ///< Radius in map space units.
    ddouble zOffset = 0;                 ///< Z-axis origin offset in map space units.
    dfloat flareSize = 0;                ///< Scale factor.
    DGLuint flareTex = 0;                ///< Custom flare texture (@todo should be Texture ptr).

    /// Custom lightmaps (if any, not owned):
    ClientTexture *sideTex = nullptr;
    ClientTexture *downTex = nullptr;
    ClientTexture *upTex   = nullptr;

    Impl() {}

    Impl(const Impl &other)
        : source     (other.source)
        , sourceMobj (other.sourceMobj)
        , maxDistance(other.maxDistance)
        , color      (other.color)
        , radius     (other.radius)
        , zOffset    (other.zOffset)
        , flareSize  (other.flareSize)
        , flareTex   (other.flareTex)
        , sideTex    (other.sideTex)
        , downTex    (other.downTex)
        , upTex      (other.upTex)
    {}
};

Lumobj::Lumobj(const Vec3d &origin, ddouble radius, const Vec3f &color)
    : MapObject(origin), d(new Impl())
{
    setRadius(radius);
    setColor(color);
}

Lumobj::Lumobj(const Lumobj &other)
    : MapObject(other.origin()), d(new Impl(*other.d))
{}

void Lumobj::setSource(const Source *newSource)
{
    d->source = newSource;
}

void Lumobj::setSourceMobj(const mobj_t *mo)
{
    d->sourceMobj = mo;
}

const mobj_t *Lumobj::sourceMobj() const
{
    return d->sourceMobj;
}

const Vec3f &Lumobj::color() const
{
    return d->color;
}

Lumobj &Lumobj::setColor(const Vec3f &newColor)
{
    if(d->color != newColor)
    {
        d->color = newColor;
    }
    return *this;
}

ddouble Lumobj::radius() const
{
    return d->radius;
}

Lumobj &Lumobj::setRadius(ddouble newRadius)
{
    // Apply the global scale factor.
    newRadius *= 40 * ::radiusScale;

    // Normalize.
    newRadius = de::clamp<ddouble>(.0001, de::abs(newRadius), ::radiusMax);

    if(d->radius != newRadius)
    {
        d->radius = newRadius;
    }
    return *this;
}

AABoxd Lumobj::bounds() const
{
    return AABoxd(origin().x - d->radius, origin().y - d->radius,
                  origin().x + d->radius, origin().y + d->radius);
}

ddouble Lumobj::zOffset() const
{
    return d->zOffset;
}

Lumobj &Lumobj::setZOffset(ddouble newZOffset)
{
    d->zOffset = newZOffset;
    return *this;
}

ddouble Lumobj::maxDistance() const
{
    return d->maxDistance;
}

Lumobj &Lumobj::setMaxDistance(ddouble newMaxDistance)
{
    d->maxDistance = newMaxDistance;
    return *this;
}

ClientTexture *Lumobj::lightmap(LightmapSemantic semantic) const
{
    switch(semantic)
    {
    case Side: return d->sideTex;
    case Down: return d->downTex;
    case Up:   return d->upTex;
    };
    DE_ASSERT(false);
    return d->sideTex;
}

Lumobj &Lumobj::setLightmap(LightmapSemantic semantic, ClientTexture *newTexture)
{
    switch(semantic)
    {
    case Side: d->sideTex = newTexture; break;
    case Down: d->downTex = newTexture; break;
    case Up:   d->upTex   = newTexture; break;
    };
    return *this;
}

dfloat Lumobj::flareSize() const
{
    return d->flareSize;
}

Lumobj &Lumobj::setFlareSize(dfloat newFlareSize)
{
    d->flareSize = de::max(0.f, newFlareSize);
    return *this;
}

DGLuint Lumobj::flareTexture() const
{
    return d->flareTex;
}

Lumobj &Lumobj::setFlareTexture(DGLuint newTexture)
{
    d->flareTex = newTexture;
    return *this;
}

dfloat Lumobj::attenuation(ddouble distFromEye) const
{
    if(distFromEye > 0 && d->maxDistance > 0)
    {
        if(distFromEye > d->maxDistance) return 0;
        if(distFromEye > .67 * d->maxDistance)
            return (d->maxDistance - distFromEye) / (.33 * d->maxDistance);
    }
    return 1;
}

void Lumobj::generateFlare(const Vec3d &eye, ddouble distFromEye)
{
    // Is the point in range?
    if(d->maxDistance > 0 && distFromEye > d->maxDistance)
        return;

    /// @todo Remove this limitation.
    if(!d->source) return;

    vissprite_t *vis = R_NewVisSprite(VSPR_FLARE);

    vis->pose.origin             = origin();
    vis->pose.distance           = distFromEye;
    V3f_Set(vis->data.flare.color, d->color.x, d->color.y, d->color.z);
    vis->data.flare.mul          = d->source->occlusion(eye) * attenuation(distFromEye);
    vis->data.flare.size         = d->flareSize > 0? de::max(1.f, d->flareSize * 60 * (50 + haloSize) / 100.0f) : 0;
    vis->data.flare.tex          = d->flareTex;
    vis->data.flare.lumIdx       = indexInMap();
    vis->data.flare.isDecoration = true;
}

dint Lumobj::radiusMax()  // static
{
    return ::radiusMax;
}

dfloat Lumobj::radiusFactor()  // static
{
    return ::radiusScale;
}

void Lumobj::consoleRegister()  // static
{
    C_VAR_INT  ("rend-light-radius-max",   &::radiusMax,   0, 64,  512);
    C_VAR_FLOAT("rend-light-radius-scale", &::radiusScale, 0, .1f, 10);
}
