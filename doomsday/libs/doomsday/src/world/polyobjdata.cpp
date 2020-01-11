/** @file polyobjdata.cpp  Private data for polyobj.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/world/polyobjdata.h"
#include "doomsday/world/map.h"

namespace world {

using namespace de;

PolyobjData::PolyobjData()
{
    mesh       = new mesh::Mesh;
    indexInMap = MapElement::NoIndex;
}

PolyobjData::~PolyobjData()
{
    delete mesh;
}

void PolyobjData::setThinker(thinker_s *thinker)
{
    _polyobj = (Polyobj *) thinker;
}

void PolyobjData::think()
{
    // nothing to do; public thinker does all
}

Thinker::IData *PolyobjData::duplicate() const
{
    return new PolyobjData(*this);
}

} // namespace world
