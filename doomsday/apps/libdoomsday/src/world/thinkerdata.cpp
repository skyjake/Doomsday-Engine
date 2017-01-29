/** @file thinkerdata.cpp  Base class for thinker private data.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/thinkerdata.h"
#include "doomsday/world/map.h"

#include <QMultiHash>

using namespace de;

//static QMultiHash<Id::Type, ThinkerData *> thinkerLookup;

DENG2_PIMPL(ThinkerData)
{
    thinker_s *think;
    Id id; ///< Internal unique ID.
    Record names;

    Impl(Public *i, Id const &id)
        : Base(i)
        , think(0)
        , id(id)
    {}

    Impl(Public *i, Impl const &other)
        : Base(i)
        , think(other.think)
        , id(other.id)
        , names(other.names)
    {}

    ~Impl()
    {
        //thinkerLookup.remove(id, &self());

        DENG2_FOR_PUBLIC_AUDIENCE2(Deletion, i)
        {
            i->thinkerBeingDeleted(*think);
        }
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
};

DENG2_AUDIENCE_METHOD(ThinkerData, Deletion)

ThinkerData::ThinkerData(Id const &id)
    : d(new Impl(this, id))
{
    //DENG2_ASSERT(!d->id.isNone());
    //thinkerLookup.insert(d->id, this);
}

ThinkerData::ThinkerData(ThinkerData const &other) : d(new Impl(this, *other.d))
{
    //DENG2_ASSERT(!d->id.isNone());
    //thinkerLookup.insert(d->id, this);
}

Id const &ThinkerData::id() const
{
    return d->id;
}

void ThinkerData::setId(Id const &id)
{
    //thinkerLookup.remove(d->id, this);
    //thinkerLookup.insert(id, this);

    d->id = id;
}

void ThinkerData::setThinker(thinker_s *thinker)
{
    d->think = thinker;
}

void ThinkerData::think()
{
    /// @todo If there is a think function in the Record, call it now. -jk
}

Thinker::IData *ThinkerData::duplicate() const
{
    return new ThinkerData(*this);
}

thinker_s &ThinkerData::thinker()
{
    DENG2_ASSERT(d->think != 0);
    return *d->think;
}

thinker_s const &ThinkerData::thinker() const
{
    DENG2_ASSERT(d->think != 0);
    return *d->think;
}

Record &ThinkerData::objectNamespace()
{
    return d->names;
}

Record const &ThinkerData::objectNamespace() const
{
    return d->names;
}

void ThinkerData::initBindings()
{}

void ThinkerData::operator >> (Writer &to) const
{
    to << world::InternalSerialId(world::THINKER_DATA)
       << d->id
       << Record(d->names, Record::IgnoreDoubleUnderscoreMembers);
}

void ThinkerData::operator << (Reader &from)
{
    //thinkerLookup.remove(d->id, this);

    world::InternalSerialId sid;
    from >> sid;

    switch (sid)
    {
    case world::THINKER_DATA:
        from >> d->id >> d->names;
        break;

    default:
        throw DeserializationError("ThinkerData::operator <<",
                                   "Invalid serial identifier " +
                                   String::number(sid));
    }

    // The thinker has a new ID.
    //thinkerLookup.insert(d->id, this);
}

/*ThinkerData *ThinkerData::find(Id const &id)
{
    auto found = thinkerLookup.constFind(id);
    if (found != thinkerLookup.constEnd())
    {
        return found.value();
    }
    return nullptr;
}*/

#ifdef DENG2_DEBUG
duint32 ThinkerData::DebugCounter::total = 0;
ThinkerData::DebugValidator ensureAllPrivateDataIsReleased;
#endif
