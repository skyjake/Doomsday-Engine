/** @file thinker.cpp  Base for all thinkers.
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

#include "doomsday/world/thinker.h"

#include <de/math.h>
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>

using namespace de;

DE_PIMPL_NOREF(Thinker)
{
    dsize size;
    thinker_s *base;    // owned
    IData *data;        // owned, optional

    Impl(AllocMethod alloc, dsize sizeInBytes, IData *data_)
        : size(max<dsize>(sizeInBytes, sizeof(thinker_s)))
        , base(0)
        , data(data_)
    {
        if (alloc == AllocateStandard)
        {
            base = reinterpret_cast<thinker_s *>(M_Calloc(size));
            base->_flags = THINKF_STD_MALLOC;
        }
        else // using memory zone
        {
            base = reinterpret_cast<thinker_s *>(Z_Calloc(size, PU_MAP, NULL));
        }

        if (data) data->setThinker(base);
    }

    Impl(const Impl &other)
        : size(other.size)
        , base(reinterpret_cast<thinker_s *>(other.base->_flags & THINKF_STD_MALLOC?
                                                 M_MemDup(other.base, size) :
                                                 Z_MemDup(other.base, size)))
        , data(other.data? other.data->duplicate() : 0)
    {
        base->d = data;
        if (data) data->setThinker(base);
    }

    Impl(thinker_s *podThinkerToTake, dsize sizeInBytes)
        : size(sizeInBytes)
        , base(podThinkerToTake)
        , data(reinterpret_cast<IData *>(podThinkerToTake->d)) // also take ownership of the private data
    {}

    ~Impl()
    {
        release();
    }

    void release()
    {
        if (base)
        {
            if (isStandardAllocated())
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

    static void clearBaseToZero(thinker_s *base, dsize size)
    {
        const bool stdAlloc = CPP_BOOL(base->_flags & THINKF_STD_MALLOC);
        memset(base, 0, size);
        if (stdAlloc) base->_flags |= THINKF_STD_MALLOC;
    }
};

#define STRUCT_MEMBER_ACCESSORS() \
      prev    (*this, offsetof(thinker_s, prev    )) \
    , next    (*this, offsetof(thinker_s, next    )) \
    , function(*this, offsetof(thinker_s, function)) \
    , id      (*this, offsetof(thinker_s, id      ))

Thinker::Thinker(dsize sizeInBytes, IData *data)
    : d(new Impl(AllocateStandard, sizeInBytes, data))
    , STRUCT_MEMBER_ACCESSORS()
{
    // Default to no public thinker callback.
    function = Thinker_NoOperation;
}

Thinker::Thinker(AllocMethod alloc, dsize sizeInBytes, Thinker::IData *data)
    : d(new Impl(alloc, sizeInBytes, data))
    , STRUCT_MEMBER_ACCESSORS()
{
    // Default to no public thinker callback.
    function = Thinker_NoOperation;
}

Thinker::Thinker(const Thinker &other)
    : d(new Impl(*other.d))
    , STRUCT_MEMBER_ACCESSORS()
{}

Thinker::Thinker(const thinker_s &podThinker, dsize sizeInBytes, AllocMethod alloc)
    : d(new Impl(alloc, sizeInBytes, 0))
    , STRUCT_MEMBER_ACCESSORS()
{
    DE_ASSERT(d->size == sizeInBytes);
    memcpy(d->base, &podThinker, sizeInBytes);

    // Retain the original allocation flag, though.
    d->base->_flags &= ~THINKF_STD_MALLOC;
    if (alloc == AllocateStandard) d->base->_flags |= THINKF_STD_MALLOC;

    if (podThinker.d)
    {
        setData(reinterpret_cast<IData *>(podThinker.d)->duplicate());
    }
}

Thinker::Thinker(thinker_s *podThinkerToTake, de::dsize sizeInBytes)
    : d(new Impl(podThinkerToTake, sizeInBytes))
    , STRUCT_MEMBER_ACCESSORS()
{}

Thinker &Thinker::operator = (const Thinker &other)
{
    d.reset(new Impl(*other.d));
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

    Impl::clearBaseToZero(d->base, d->size);
}

bool Thinker::isDisabled() const
{
    return (d->base->_flags & THINKF_DISABLED) != 0;
}

thinker_s &Thinker::base()
{
    return *d->base;
}

const thinker_s &Thinker::base() const
{
    return *d->base;
}

bool Thinker::hasData() const
{
    return d->data != 0;
}

Thinker::IData &Thinker::data()
{
    DE_ASSERT(hasData());
    return *d->data;
}

const Thinker::IData &Thinker::data() const
{
    DE_ASSERT(hasData());
    return *d->data;
}

dsize Thinker::sizeInBytes() const
{
    return d->size;
}

thinker_s *Thinker::take()
{
    DE_ASSERT(d->base->d == d->data);

    thinker_s *th = d->base;
    d->relinquish();
    return th;
}

void Thinker::destroy(thinker_s *thinkerBase)
{
    DE_ASSERT(thinkerBase != 0);

    release(*thinkerBase);

    if (thinkerBase->_flags & THINKF_STD_MALLOC)
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
    if (thinkerBase.d)
    {
        delete reinterpret_cast<IData *>(thinkerBase.d);
        thinkerBase.d = 0;
    }
}

void Thinker::zap(thinker_s &thinkerBase, dsize sizeInBytes)
{
    delete reinterpret_cast<IData *>(thinkerBase.d);
    Impl::clearBaseToZero(&thinkerBase, sizeInBytes);
}

void Thinker::setData(Thinker::IData *data)
{
    if (d->data) delete d->data;

    d->data    = data;
    d->base->d = data;

    if (data)
    {
        data->setThinker(*this);
    }
}

dd_bool Thinker_InStasis(const thinker_s *thinker)
{
    if (!thinker) return false;
    return (thinker->_flags & THINKF_DISABLED) != 0;
}

void Thinker_SetStasis(thinker_t *thinker, dd_bool on)
{
    if (thinker)
    {
        applyFlagOperation(thinker->_flags, duint32(THINKF_DISABLED),
                           on? SetFlags : UnsetFlags);
    }
}

void Thinker_NoOperation(void *)
{
    // do nothing
}
