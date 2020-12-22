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

#include <doomsday/world/polyobjdata.h>

class ClPolyMover;

class PolyobjData : public world::PolyobjData
{
public:
    PolyobjData() = default;
    
    void addMover(ClPolyMover &mover);
    void removeMover(ClPolyMover &mover);
    ClPolyMover *mover() const;

private:
    ClPolyMover *_mover = nullptr;
};
