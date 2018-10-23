/** @file mobjthinkerdata.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/mobjthinkerdata.h"

#include <de/ScriptSystem>
#include <de/RecordValue>

using namespace de;

static String const VAR_ID("__id__");

DENG2_PIMPL_NOREF(MobjThinkerData)
{};

MobjThinkerData::MobjThinkerData(de::Id const &id)
    : ThinkerData(id)
    , d(new Impl)
{}

MobjThinkerData::MobjThinkerData(MobjThinkerData const &other)
    : ThinkerData(other)
    , d(new Impl)
{}

void MobjThinkerData::think()
{
    auto &mo = *mobj();

    coord_t lastOrigin[3];
    memcpy(lastOrigin, mo.origin, sizeof(lastOrigin));
    mo.ddFlags &= ~DDMF_MOVEBLOCKED;

    ThinkerData::think();

    // Flag the mobj if it didn't change position (per axis).
    for (int axis = 0; axis < 3; ++axis)
    {
        if (fequal(lastOrigin[axis], mo.origin[axis]))
        {
            mo.ddFlags |= DDMF_MOVEBLOCKEDX << axis;
        }
    }
}

Thinker::IData *MobjThinkerData::duplicate() const
{
    return new MobjThinkerData(*this);
}

mobj_t *MobjThinkerData::mobj()
{
    return reinterpret_cast<mobj_t *>(&thinker());
}

mobj_t const *MobjThinkerData::mobj() const
{
    return reinterpret_cast<mobj_t const *>(&thinker());
}

void MobjThinkerData::initBindings()
{
    ThinkerData::initBindings();

    // World.Thing is the class for mobjs.
    objectNamespace().addSuperRecord(ScriptSystem::builtInClass(
            QStringLiteral("World"), QStringLiteral("Thing")));

    // The ID is important because this is how the object is identified in
    // script functions (relied upon by World.Thing).
    objectNamespace().addNumber(VAR_ID, mobj()->thinker.id).setReadOnly();
}

void MobjThinkerData::stateChanged(state_t const *)
{
    // overridden
}

void MobjThinkerData::damageReceived(int, mobj_t const *)
{}

void MobjThinkerData::operator << (Reader &from)
{
    ThinkerData::operator << (from);
    initBindings();
}
