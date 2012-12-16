/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_TRIPWIRED_H
#define LIBDENG2_TRIPWIRED_H

#include "../libdeng2.h"

#include <QThread>
#include <QMutex>
#include <QDebug>

namespace de {

/**
 * Debugging aid that can be used to detect incorrect threaded access to an
 * object. Use this to detect if an object should be Lockable.
 *
 * Causes an assert to fail if a thread tries to access the tripwired object
 * while another thread is already accessing it.
 *
 * @ingroup data
 */
class DENG2_PUBLIC Tripwired
{
public:
    Tripwired();
    virtual ~Tripwired();

    /// Arm the tripwire. Other threads will fail if they try access it.
    void arm() const;

    /// Disarm the tripwire.
    void disarm() const;

private:
    mutable QThread *_user;
    mutable QMutex _userMutex;
    mutable int _count;
};

#define DENG2_ARMED(varName) \
    de::TripwireArmer _tripwire_##varName(varName); \
    DENG2_UNUSED(_tripwire_##varName);

#define DENG2_ARMED_VALUE(varName) \
    de::TripwireArmer _tripwire_##varName(varName.value()); \
    DENG2_UNUSED(_tripwire_##varName);

class DENG2_PUBLIC TripwireArmer
{
public:
    /**
     * The target object is armed.
     */
    TripwireArmer(Tripwired const &target);

    /**
     * The target object is armed.
     */
    TripwireArmer(Tripwired const *target);

    /**
     * The target object is disarmed.
     */
    ~TripwireArmer();

private:
    Tripwired const *_target;
};

} // namespace de

#endif // LIBDENG2_TRIPWIRED_H
