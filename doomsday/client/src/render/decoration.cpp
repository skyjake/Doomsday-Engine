/** @file decoration.cpp World surface decoration.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "MaterialSnapshot"
#include "Surface"

using namespace de;

DENG2_PIMPL_NOREF(Decoration)
{
    MaterialSnapshotDecoration *source;
    Surface *surface;

    Instance(MaterialSnapshotDecoration &source) : source(&source), surface(0)
    {}
};

Decoration::Decoration(MaterialSnapshotDecoration &source, Vector3d const &origin)
    : MapObject(origin), d(new Instance(source))
{}

Decoration::~Decoration()
{}

MaterialSnapshotDecoration &Decoration::source()
{
    return *d->source;
}

MaterialSnapshotDecoration const &Decoration::source() const
{
    return *d->source;
}

bool Decoration::hasSurface() const
{
    return d->surface != 0;
}

Surface &Decoration::surface()
{
    if(d->surface != 0)
    {
        return *d->surface;
    }
    /// @throw MissingSurfaceError Attempted with no surface attributed.
    throw MissingSurfaceError("Decoration::surface", "No surface is attributed");
}

Surface const &Decoration::surface() const
{
    if(d->surface != 0)
    {
        return *d->surface;
    }
    /// @throw MissingSurfaceError Attempted with no surface attributed.
    throw MissingSurfaceError("Decoration::surface", "No surface is attributed");
}

void Decoration::setSurface(Surface *newSurface)
{
    d->surface = newSurface;
}
