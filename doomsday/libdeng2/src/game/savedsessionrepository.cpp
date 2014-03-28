/** @file savedsessionrepository.cpp  Saved (game) session repository.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/game/SavedSessionRepository"

#include "de/App"
#include "de/game/SavedSession"

namespace de {
namespace game {

DENG2_PIMPL(SavedSessionRepository)
{
    All sessions;
    bool availabilityUpdateDisabled;

    Instance(Public *i) : Base(i), availabilityUpdateDisabled(false) {}

    void notifyAvailabilityUpdate()
    {
        if(availabilityUpdateDisabled) return;
        DENG2_FOR_PUBLIC_AUDIENCE2(AvailabilityUpdate, i) i->repositoryAvailabilityUpdate(self);
    }

    DENG2_PIMPL_AUDIENCE(AvailabilityUpdate)
};

DENG2_AUDIENCE_METHOD(SavedSessionRepository, AvailabilityUpdate)

SavedSessionRepository::SavedSessionRepository() : d(new Instance(this))
{}

void SavedSessionRepository::clear()
{
    // Disable updates for now, we'll do that when we're done.
    d->availabilityUpdateDisabled = true;

    // Clear the index we're starting over.
    qDebug() << "Clearing SavedSession index";
    d->sessions.clear();

    // Now perform the availability notifications.
    d->availabilityUpdateDisabled = false;
    d->notifyAvailabilityUpdate();
}

void SavedSessionRepository::add(SavedSession &session)
{
    d->sessions[session.path().toLower()] = &session;
    d->notifyAvailabilityUpdate();
}

void SavedSessionRepository::remove(String path)
{
    if(d->sessions.remove(path.toLower()))
    {
        d->notifyAvailabilityUpdate();
    }
}

SavedSession *SavedSessionRepository::find(String path) const
{
    All::iterator found = d->sessions.find(path.toLower());
    if(found != d->sessions.end())
    {
        return found.value();
    }
    return 0; // Not found.
}

SavedSessionRepository::All const &SavedSessionRepository::all() const
{
    return d->sessions;
}

} // namespace game
} // namespace de
