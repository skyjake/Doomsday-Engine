/** @file decoration.cpp  World surface decoration.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "render/decoration.h"

#include "world/surface.h"
#include "world/clientworld.h"
#include <doomsday/world/materialmanifest.h>

using namespace de;

DE_PIMPL_NOREF(Decoration)
{
    const MaterialAnimator::Decoration *source = nullptr;
    Surface *surface = nullptr;
};

Decoration::Decoration(const MaterialAnimator::Decoration &source, const Vec3d &origin)
    : MapObject(origin)
    , d(new Impl)
{
    d->source = &source;
}

Decoration::~Decoration()
{}

String Decoration::description() const
{
    auto desc = Stringf(    _E(l) "Origin: "   _E(.)_E(i) "%s" _E(.)
                               " " _E(l) "Material: " _E(.)_E(i) "%s" _E(.)
                               " " _E(l) "Surface: "  _E(.)_E(i) "[%p]" _E(.),
                  origin().asText().c_str(),
                  source().decor().material().manifest().composeUri().asText().c_str(),
                  &surface());

#ifdef DE_DEBUG
    desc.prepend(Stringf(_E(b) "Decoration " _E(.) "[%p]\n", this));
#endif
    return desc;
}

const MaterialAnimator::Decoration &Decoration::source() const
{
    DE_ASSERT(d->source);
    return *d->source;
}

bool Decoration::hasSurface() const
{
    return d->surface != nullptr;
}

Surface &Decoration::surface()
{
    if(hasSurface()) return *d->surface;
    /// @throw MissingSurfaceError  Attempted with no surface attributed.
    throw MissingSurfaceError("Decoration::surface", "No surface is attributed");
}

const Surface &Decoration::surface() const
{
    if(hasSurface()) return *d->surface;
    /// @throw MissingSurfaceError Attempted with no surface attributed.
    throw MissingSurfaceError("Decoration::surface", "No surface is attributed");
}

void Decoration::setSurface(Surface *newSurface)
{
    d->surface = newSurface;
}
