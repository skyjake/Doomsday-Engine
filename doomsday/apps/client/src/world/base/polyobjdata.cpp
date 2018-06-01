/** @file polyobjdata.cpp  Private data for polyobj.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/polyobjdata.h"

#ifdef __CLIENT__
#  include "client/clpolymover.h"
#endif

#include "world/map.h"
#include "world/thinkers.h"

using namespace de;
using namespace world;

PolyobjData::PolyobjData()
{
    mesh       = new Mesh;
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

#ifdef __CLIENT__
void PolyobjData::addMover(ClPolyMover &mover)
{
    if(_mover)
    {
        Thinker_Map(_mover->thinker()).thinkers().remove(_mover->thinker());
        DE_ASSERT(!_mover);
    }

    _mover = &mover;
}

void PolyobjData::removeMover(ClPolyMover &mover)
{
    if(_mover == &mover)
    {
        _mover = 0;
    }
}

ClPolyMover *PolyobjData::mover() const
{
    return _mover;
}
#endif
