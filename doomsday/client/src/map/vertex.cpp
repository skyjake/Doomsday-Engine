/** @file map/vertex.cpp World Map Vertex.
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

#include <de/Vector>

#include "Line"
#include "map/lineowner.h" /// @todo remove me

#include "map/vertex.h"

using namespace de;

DENG2_PIMPL(Vertex)
{
    /// Position in the map coordinate space.
    Vector2d origin;

    Instance(Public *i, Vector2d const &origin)
        : Base(i), origin(origin)
    {}

    void notifyOriginChanged(Vector2d const &oldOrigin, int changedComponents)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OriginChange, i)
        {
            i->vertexOriginChanged(self, oldOrigin, changedComponents);
        }
    }

    void notifyOriginChanged(Vector2d const &oldOrigin)
    {
        // Predetermine which axes have changed.
        int changedAxes = 0;
        for(int i = 0; i < 2; ++i)
        {
            if(!de::fequal(origin[i], oldOrigin[i]))
                changedAxes |= (1 << i);
        }

        notifyOriginChanged(oldOrigin, changedAxes);
    }
};

Vertex::Vertex(Vector2d const &origin)
    : MapElement(DMU_VERTEX), d(new Instance(this, origin))
{
    _lineOwners = 0;
    _numLineOwners = 0;
}

Vector2d const &Vertex::origin() const
{
    return d->origin;
}

void Vertex::setOrigin(Vector2d const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        Vector2d oldOrigin = d->origin;

        d->origin = newOrigin;

        // Notify interested parties of the change.
        d->notifyOriginChanged(oldOrigin);
    }
}

void Vertex::setOriginComponent(int component, coord_t newPosition)
{
    if(!de::fequal(d->origin[component], newPosition))
    {
        Vector2d oldOrigin = d->origin;

        d->origin[component] = newPosition;

        // Notify interested parties of the change.
        d->notifyOriginChanged(oldOrigin, (1 << component));
    }
}

int Vertex::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_X:
        DMU_GetValue(DMT_VERTEX_ORIGIN, &d->origin.x, &args, 0);
        break;
    case DMU_Y:
        DMU_GetValue(DMT_VERTEX_ORIGIN, &d->origin.y, &args, 0);
        break;
    case DMU_XY:
        DMU_GetValue(DMT_VERTEX_ORIGIN, &d->origin.x, &args, 0);
        DMU_GetValue(DMT_VERTEX_ORIGIN, &d->origin.y, &args, 1);
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

void Vertex::countLineOwners(uint *oneSided, uint *twoSided) const
{
    if(!oneSided && !twoSided) return;

    uint ones = 0, twos = 0;

    if(LineOwner const *firstOwn = firstLineOwner())
    {
        LineOwner const *own = firstOwn;
        do
        {
            if(!own->line().hasFrontSections() || !own->line().hasBackSections())
            {
                ++ones;
            }
            else
            {
                ++twos;
            }
        } while((own = &own->next()) != firstOwn);
    }

    if(oneSided) *oneSided += ones;
    if(twoSided) *twoSided += twos;
}

LineOwner *Vertex::firstLineOwner() const
{
    return _lineOwners;
}
