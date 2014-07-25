/** @file thinker.cpp  Base for all thinkers.
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

#include "doomsday/world/thinker.h"

#include <de/math.h>
#include <de/memory.h>
#include <de/memoryzone.h>

using namespace de;

DENG2_PIMPL_NOREF(Thinker)
{
    dsize size;
    thinker_s *base;    // owned
    IData *data;        // owned, optional

    Instance(AllocMethod alloc, dsize sizeInBytes, IData *data_)
        : size(max<dsize>(sizeInBytes, sizeof(thinker_s)))
        , base(0)
        , data(data_)
    {
        if(alloc == AllocateStandard)
        {
            base = reinterpret_cast<thinker_s *>(M_Calloc(size));
            base->_flags = THINKF_STD_MALLOC;
        }
        else // using memory zone
        {
            base = reinterpret_cast<thinker_s *>(Z_Calloc(size, PU_MAP, NULL));
        }
    }

    Instance(Instance const &other)
        : size(other.size)
        , base(reinterpret_cast<thinker_s *>(other.base->_flags & THINKF_STD_MALLOC?
                                                 M_MemDup(other.base, size) :
                                                 Z_MemDup(other.base, size)))
        , data(other.data? other.data->duplicate() : 0)
    {
        base->d = data;
    }

    Instance(thinker_s *podThinkerToTake, dsize sizeInBytes)
        : size(sizeInBytes)
        , base(podThinkerToTake)
        , data(reinterpret_cast<IData *>(podThinkerToTake->d)) // also take ownership of the private data
    {}

    ~Instance()
    {
        release();
    }

    void release()
    {
        if(base)
        {
            if(isStandardAllocated())
            {
                M_Free(base);
            }
            else
            {
                Z_Free(base);
            }
        }

        // Get rid of the private data, too.
        delete data;
    }

    bool isStandardAllocated() const
    {
        return base && (base->_flags & THINKF_STD_MALLOC);
    }

    void relinquish()
    {
        base = 0;
        data = 0;
        size = 0;
    }
};

#define STRUCT_MEMBER_ACCESSORS() \
      prev    (*this, offsetof(thinker_s, prev    )) \
    , next    (*this, offsetof(thinker_s, next    )) \
    , function(*this, offsetof(thinker_s, function)) \
    , id      (*this, offsetof(thinker_s, id      ))

Thinker::Thinker(dsize sizeInBytes, IData *data)
    : d(new Instance(AllocateStandard, sizeInBytes, data))
    , STRUCT_MEMBER_ACCESSORS()
{}

Thinker::Thinker(AllocMethod alloc, dsize sizeInBytes, Thinker::IData *data)
    : d(new Instance(alloc, sizeInBytes, data))
    , STRUCT_MEMBER_ACCESSORS()
{}

Thinker::Thinker(Thinker const &other)
    : d(new Instance(*other.d))
    , STRUCT_MEMBER_ACCESSORS()
{}

Thinker::Thinker(thinker_s const &podThinker, dsize sizeInBytes, AllocMethod alloc)
    : d(new Instance(alloc, sizeInBytes, 0))
    , STRUCT_MEMBER_ACCESSORS()
{
    DENG2_ASSERT(d->size == sizeInBytes);
    memcpy(d->base, &podThinker, sizeInBytes);

    // Retain the original allocation flag, though.
    d->base->_flags &= ~THINKF_STD_MALLOC;
    if(alloc == AllocateStandard) d->base->_flags |= THINKF_STD_MALLOC;

    if(podThinker.d)
    {
        setData(reinterpret_cast<IData *>(podThinker.d)->duplicate());
    }
}

Thinker::Thinker(thinker_s *podThinkerToTake, de::dsize sizeInBytes)
    : d(new Instance(podThinkerToTake, sizeInBytes))
    , STRUCT_MEMBER_ACCESSORS()
{}

Thinker &Thinker::operator = (Thinker const &other)
{
    d.reset(new Instance(*other.d));
    return *this;
}

void Thinker::enable(bool yes)
{
    applyFlagOperation(d->base->_flags, duint32(THINKF_DISABLED), yes? SetFlags : UnsetFlags);
}

void Thinker::zap()
{
    delete d->data;
    d->data = 0;

    memset(d->base, 0, d->size);
}

bool Thinker::isDisabled() const
{
    return (d->base->_flags & THINKF_DISABLED) != 0;
}

thinker_s &Thinker::base()
{
    return *d->base;
}

thinker_s const &Thinker::base() const
{
    return *d->base;
}

bool Thinker::hasData() const
{
    return d->data != 0;
}

Thinker::IData &Thinker::data()
{
    DENG2_ASSERT(hasData());
    return *d->data;
}

Thinker::IData const &Thinker::data() const
{
    DENG2_ASSERT(hasData());
    return *d->data;
}

dsize Thinker::sizeInBytes() const
{
    return d->size;
}

thinker_s *Thinker::take()
{
    DENG2_ASSERT(d->base->d == d->data);

    thinker_s *th = d->base;
    d->relinquish();
    return th;
}

void Thinker::putInto(thinker_s &dest)
{
    delete reinterpret_cast<IData *>(dest.d);
    memcpy(&dest, d->base, d->size);

    // Not valid any more.
    d->relinquish();
}

void Thinker::destroy(thinker_s *thinkerBase)
{
    DENG2_ASSERT(thinkerBase != 0);

    release(*thinkerBase);

    if(thinkerBase->_flags & THINKF_STD_MALLOC)
    {
        M_Free(thinkerBase);
    }
    else
    {
        Z_Free(thinkerBase);
    }
}

void Thinker::release(thinker_s &thinkerBase)
{
    delete reinterpret_cast<IData *>(thinkerBase.d);
    thinkerBase.d = 0;
}

void Thinker::zap(thinker_s &thinkerBase, dsize sizeInBytes)
{
    delete reinterpret_cast<IData *>(thinkerBase.d);
    memset(&thinkerBase, 0, sizeInBytes);
}

void Thinker::setData(Thinker::IData *data)
{
    if(d->data) delete d->data;

    d->data    = data;
    d->base->d = data;
}

dd_bool Thinker_InStasis(thinker_s const *thinker)
{
    if(!thinker) return false;
    return (thinker->_flags & THINKF_DISABLED) != 0;
}

void Thinker_SetStasis(thinker_t *thinker, dd_bool on)
{
    if(thinker)
    {
        applyFlagOperation(thinker->_flags, duint32(THINKF_DISABLED),
                           on? SetFlags : UnsetFlags);
    }
}

