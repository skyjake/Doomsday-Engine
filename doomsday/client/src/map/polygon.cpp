/** @file map/face.cpp World Map Geometry Polygon.
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

#include "HEdge"
#include "Face"

#include "map/polygon.h"

namespace de {

DENG2_PIMPL(Polygon)
{
    /// Face geometry.
    Face face;

    Instance(Public *i)
        : Base(i),
          face(self)
    {}

    ~Instance()
    {
        // Clear all the half-edges.
        if(HEdge *hedge = face.hedge())
        {
            if(!hedge->hasNext() || &hedge->next() == hedge)
            {
                delete hedge;
            }
            else
            {
                // Break the ring, if linked.
                if(hedge->hasPrev())
                {
                    hedge->prev().setNext(0);
                }

                while(hedge)
                {
                    HEdge *next = hedge->hasNext()? &hedge->next() : 0;
                    delete hedge;
                    hedge = next;
                }
            }
        }
    }
};

Polygon::Polygon()
    : _hedgeCount(0), d(new Instance(this))
{}

Face *Polygon::firstFace() const
{
    return &d->face;
}

int Polygon::hedgeCount() const
{
    return _hedgeCount;
}

} // namespace de
