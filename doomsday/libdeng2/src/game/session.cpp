/** @file session.cpp  Logical game session base class.
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

#include "de/game/session.h"
#include "de/App"
#include "de/game/Game"
#include "de/game/SavedSession"
#include "de/Log"
#include "de/Writer"

namespace de {
namespace game {

static Session::Profile currentProfile;
static Session::SavedIndex sharedSavedIndex;

Session::Profile &Session::profile() //static
{
    /// @todo None current profiles should be stored persistently when the game changes.
    return currentProfile;
}

void Session::removeSaved(String const &path) //static
{
    if(App::rootFolder().has(path))
    {
        savedIndex().remove(path);
        App::rootFolder().removeFile(path);
    }
}

void Session::copySaved(String const &destPath, String const &sourcePath) //static
{
    if(!destPath.compareWithoutCase(sourcePath)) return;

    LOG_AS("GameSession");

    removeSaved(destPath);

    SavedSession const &original = App::rootFolder().locate<SavedSession>(sourcePath);
    SavedSession &copied = App::fileSystem().copySerialized(sourcePath, destPath).as<SavedSession>();
    copied.cacheMetadata(original.metadata()); // Avoid immediately opening the .save package.
    savedIndex().add(copied);
}

DENG2_PIMPL(Session::SavedIndex)
{
    All entries;
    bool availabilityUpdateDisabled;

    Instance(Public *i) : Base(i), availabilityUpdateDisabled(false) {}

    void notifyAvailabilityUpdate()
    {
        if(availabilityUpdateDisabled) return;
        DENG2_FOR_PUBLIC_AUDIENCE2(AvailabilityUpdate, i) i->savedIndexAvailabilityUpdate(self);
    }

    DENG2_PIMPL_AUDIENCE(AvailabilityUpdate)
};

DENG2_AUDIENCE_METHOD(Session::SavedIndex, AvailabilityUpdate)

Session::SavedIndex::SavedIndex() : d(new Instance(this))
{}

void Session::SavedIndex::clear()
{
    // Disable updates for now, we'll do that when we're done.
    d->availabilityUpdateDisabled = true;

    // Clear the index we're starting over.
    qDebug() << "Clearing saved game session index";
    d->entries.clear();

    // Now perform the availability notifications.
    d->availabilityUpdateDisabled = false;
    d->notifyAvailabilityUpdate();
}

void Session::SavedIndex::add(SavedSession &saved)
{
    d->entries[saved.path().toLower()] = &saved;
    d->notifyAvailabilityUpdate();
}

void Session::SavedIndex::remove(String path)
{
    if(d->entries.remove(path.toLower()))
    {
        d->notifyAvailabilityUpdate();
    }
}

SavedSession *Session::SavedIndex::find(String path) const
{
    All::iterator found = d->entries.find(path.toLower());
    if(found != d->entries.end())
    {
        return found.value();
    }
    return 0; // Not found.
}

Session::SavedIndex::All const &Session::SavedIndex::all() const
{
    return d->entries;
}

Session::SavedIndex &Session::savedIndex() //static
{
    return sharedSavedIndex;
}

} // namespace game
} // namespace de
