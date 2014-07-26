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

using namespace de;

DENG2_PIMPL(ThinkerData)
{
    thinker_s *think;
    Record info;

    Instance(Public *i) : Base(i), think(0) {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , think(other.think)
        , info(other.info)
    {}

    ~Instance()
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(Deletion, i)
        {
            i->thinkerBeingDeleted(*think);
        }
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
};

DENG2_AUDIENCE_METHOD(ThinkerData, Deletion)

ThinkerData::ThinkerData(thinker_s *self) : d(new Instance(this))
{
    d->think = self;
}

ThinkerData::ThinkerData(ThinkerData const &other) : d(new Instance(this, *other.d))
{}

Thinker::IData *ThinkerData::duplicate() const
{
    return new ThinkerData(*this);
}

thinker_s &ThinkerData::thinker()
{
    return *d->think;
}

thinker_s const &ThinkerData::thinker() const
{
    return *d->think;
}

Record &ThinkerData::info()
{
    return d->info;
}

Record const &ThinkerData::info() const
{
    return d->info;
}

#ifdef DENG2_DEBUG
duint32 ThinkerData::DebugCounter::total = 0;
ThinkerData::DebugValidator ensureAllPrivateDataIsReleased;
#endif
