/** @file map/face.cpp World Map Geometry Face.
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

#include "map/face.h"

namespace de {

Face::Face()
    : _hedge(0), _hedgeCount(0)
{}

Face::~Face()
{
    // Clear the half-edges.
    if(_hedge)
    {
        HEdge *he = _hedge;
        if(he->_next == he)
        {
            delete he;
        }
        else
        {
            // Break the ring, if linked.
            if(he->_prev)
            {
                he->_prev->_next = 0;
            }

            while(he)
            {
                HEdge *next = he->_next;
                delete he;
                he = next;
            }
        }
    }
}

HEdge *Face::firstHEdge() const
{
    return _hedge;
}

int Face::hedgeCount() const
{
    return _hedgeCount;
}

} // namespace de
