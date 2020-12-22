/** @file polyobjdata.h  Private data for a polyobj
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include "thinker.h"
#include "polyobj.h"

#include <de/list.h>
#include <de/vector.h>

namespace world {

/**
 * Private data for a polyobj.
 *
 * Stored in the Polyobj's thinker.d (polyobjs are not normal thinkers).
 */
class LIBDOOMSDAY_PUBLIC PolyobjData : public Thinker::IData
{
public:
    /// Used to store the original/previous vertex coordinates.
    typedef de::List<de::Vec2d> VertexCoords;

public:
    PolyobjData();
    ~PolyobjData();

    void setThinker(thinker_s *thinker);
    void think();
    IData *duplicate() const;

public:
    int          indexInMap = world::MapElement::NoIndex;
    unsigned int origIndex  = world::MapElement::NoIndex;

    mesh::Mesh *mesh = nullptr;
    de::List<Line *> lines;
    de::List<Vertex *> uniqueVertexes;
    VertexCoords originalPts;  ///< Used as the base for the rotations.
    VertexCoords prevPts;      ///< Use to restore the old point values.
    
private:
    polyobj_s *_polyobj = nullptr;
};

} // namespace world


