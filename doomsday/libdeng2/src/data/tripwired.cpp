/** @file tripwired.cpp Debugging aid for threaded access.
 *
 * @todo Update the fields above as appropriate.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include "de/Tripwired"
#include <QDebug>

namespace de {

// Assertion that works in release builds as well.
static void assertion(bool ok, char const *msg)
{
    if(!ok)
    {
        qWarning("Assertion failed: %s", msg);
        qFatal("Assertion failed: %s", msg);
        DENG2_ASSERT(false);
        exit(-100);
    }
}

Tripwired::Tripwired() : _user(0), _userMutex(QMutex::Recursive), _count(0)
{}

Tripwired::~Tripwired()
{
    QThread *me = QThread::currentThread();
    DENG2_UNUSED(me);

    _userMutex.lock();
    QThread *u = _user;
    _userMutex.unlock();

    // Nobody should be using it.
    assertion(u == 0, "~Tripwired: user == 0");
    assertion(_count == 0, "~Tripwired: lock count == 0");
}

void Tripwired::arm() const
{
    QThread *me = QThread::currentThread();

    _userMutex.lock();
    if(!_user)
    {
        // We're using it now.
        _user = me;
    }
    else if(_user != me)
    {
        // Oh noes.
        assertion(false, "Tripwired: another thread has armed it");
    }
    _count++;
    _userMutex.unlock();
}

void Tripwired::disarm() const
{
    QThread *me = QThread::currentThread();

    _userMutex.lock();
    if(!_user)
    {
        assertion(false, "Tripwired: disarm without arming");
    }
    else if(_user == me)
    {
        if(!--_count) _user = 0;
    }
    else
    {
        assertion(false, "Tripwired: disarming another thread's armed tripwire");
    }
    _userMutex.unlock();
}

TripwireArmer::TripwireArmer(Tripwired const &target) : _target(&target)
{
    assertion(_target != 0, "TripwireArmer: no target");
    _target->arm();
}

TripwireArmer::TripwireArmer(Tripwired const *target) : _target(target)
{
    assertion(_target != 0, "TripwireArmer: no target");
    _target->arm();
}

TripwireArmer::~TripwireArmer()
{
    assertion(_target != 0, "~TripwireArmer: no target");
    _target->disarm();
}





} // namespace de
