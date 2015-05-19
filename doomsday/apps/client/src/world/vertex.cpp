/** @file vertex.cpp  World map vertex.
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

#include "de_base.h"
#include "world/vertex.h"

#include "Line"
#include "world/lineowner.h" /// @todo remove me
#include "Sector"

#include <de/Vector>

using namespace de;

DENG2_PIMPL_NOREF(Vertex)
{
    Vector2d origin; ///< In map space.
    Instance(Vector2d const &origin) : origin(origin)
    {}
};

Vertex::Vertex(Mesh &mesh, Vector2d const &origin)
    : MapElement(DMU_VERTEX)
    , MeshElement(mesh)
    , _lineOwners(0)
    , _numLineOwners(0)
    , _onesOwnerCount(0)
    , _twosOwnerCount(0)
    , d(new Instance(origin))
{}

Vector2d const &Vertex::origin() const
{
    return d->origin;
}

void Vertex::setOrigin(Vector2d const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        d->origin = newOrigin;
        DENG2_FOR_AUDIENCE(OriginChange, i)
        {
            i->vertexOriginChanged(*this);
        }
    }
}

int Vertex::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_X:
        args.setValue(DMT_VERTEX_ORIGIN, &d->origin.x, 0);
        break;
    case DMU_Y:
        args.setValue(DMT_VERTEX_ORIGIN, &d->origin.y, 0);
        break;
    case DMU_XY:
        args.setValue(DMT_VERTEX_ORIGIN, &d->origin.x, 0);
        args.setValue(DMT_VERTEX_ORIGIN, &d->origin.y, 1);
        break;
    default:
        return MapElement::property(args);
    }
    return false; // Continue iteration.
}

// ---------------------------------------------------------------------------

uint Vertex::lineOwnerCount() const
{
    return _numLineOwners;
}

void Vertex::countLineOwners()
{
    _onesOwnerCount = _twosOwnerCount = 0;

    if(LineOwner const *firstOwn = firstLineOwner())
    {
        LineOwner const *own = firstOwn;
        do
        {
            if(!own->line().hasFrontSector() || !own->line().hasBackSector())
            {
                _onesOwnerCount += 1;
            }
            else
            {
                _twosOwnerCount += 1;
            }
        } while((own = &own->next()) != firstOwn);
    }
}

LineOwner *Vertex::firstLineOwner() const
{
    return _lineOwners;
}
