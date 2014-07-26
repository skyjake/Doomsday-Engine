/** @file thinkerdata.h  Base class for thinker private data.
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

#ifndef LIBDOOMSDAY_THINKERDATA_H
#define LIBDOOMSDAY_THINKERDATA_H

#include "thinker.h"

#include <de/Record>
#include <de/Id>

/**
 * Base class for thinker private data.
 *
 * Contains internal functionality common to all thinkers regardless of their type.
 */
class LIBDOOMSDAY_PUBLIC ThinkerData : public Thinker::IData
{
public:
    ThinkerData(thinker_s *self);
    ThinkerData(ThinkerData const &other);

    IData *duplicate() const;

    thinker_s &thinker();
    thinker_s const &thinker() const;

    de::Record &info();
    de::Record const &info() const;

private:
    DENG2_PRIVATE(d)

#ifdef DENG2_DEBUG
public:
    struct DebugCounter {
        de::Id id;
        static duint32 total;

        DebugCounter()  { total++; }
        ~DebugCounter() { total--; }
    };
    DebugCounter _debugCounter;

    struct DebugValidator {
        DebugValidator()  { DENG2_ASSERT(DebugCounter::total == 0); }
        ~DebugValidator() { DENG2_ASSERT(DebugCounter::total == 0); }
    };
#endif
};

#endif // LIBDOOMSDAY_THINKERDATA_H
