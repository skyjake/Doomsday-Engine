/** @file thinkerdata.cpp  Base class for thinker private data.
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

#include "doomsday/world/thinkerdata.h"
#include "doomsday/world/map.h"

#include <de/hash.h>

using namespace de;

static std::unordered_multimap<Id::Type, ThinkerData *> thinkerLookup;

DE_PIMPL(ThinkerData)
{
    thinker_s *think;
    Id id; ///< Internal unique ID.
    Record names;

    Impl(Public *i, const Id &id)
        : Base(i)
        , think(0)
        , id(id)
    {}

    Impl(Public *i, const Impl &other)
        : Base(i)
        , think(other.think)
        , id(other.id)
        , names(other.names)
    {}

    ~Impl()
    {
        multiRemove(thinkerLookup, id, &self());

        DE_NOTIFY_PUBLIC(Deletion, i)
        {
            i->thinkerBeingDeleted(*think);
        }
    }

    DE_PIMPL_AUDIENCE(Deletion)
};

DE_AUDIENCE_METHOD(ThinkerData, Deletion)

ThinkerData::ThinkerData(const Id &id)
    : d(new Impl(this, id))
{
    if (d->id)
    {
        thinkerLookup.insert(std::make_pair(d->id, this));
    }
}

ThinkerData::ThinkerData(const ThinkerData &other) : d(new Impl(this, *other.d))
{
    if (d->id)
    {
        thinkerLookup.insert(std::make_pair(d->id, this));
    }
}

const Id &ThinkerData::id() const
{
    return d->id;
}

void ThinkerData::setId(const Id &id)
{
    multiRemove(thinkerLookup, d->id, this);
    thinkerLookup.insert(std::make_pair(id, this));

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
    DE_ASSERT(d->think != 0);
    return *d->think;
}

const thinker_s &ThinkerData::thinker() const
{
    DE_ASSERT(d->think != 0);
    return *d->think;
}

Record &ThinkerData::objectNamespace()
{
    return d->names;
}

const Record &ThinkerData::objectNamespace() const
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
    multiRemove(thinkerLookup, d->id, this);

    world::InternalSerialId sid;
    from >> sid;

    switch (sid)
    {
    case world::THINKER_DATA:
        from >> d->id >> d->names;
        break;

    default:
        throw DeserializationError("ThinkerData::operator <<",
                                   "Invalid serial identifier " + String::asText(sid));
    }

    // The thinker has a new ID.
    thinkerLookup.insert(std::make_pair(d->id, this));
}

ThinkerData *ThinkerData::find(const Id &id)
{
    auto found = thinkerLookup.find(id);
    if (found != thinkerLookup.end())
    {
        return found->second;
    }
    return nullptr;
}

#ifdef DE_DEBUG
duint32 ThinkerData::DebugCounter::total = 0;
static ThinkerData::DebugValidator ensureAllPrivateDataIsReleased;
#endif
