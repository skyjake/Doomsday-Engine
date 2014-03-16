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
#include "de/game/Game"
#include "de/game/MapStateReader"
#include "de/Log"
#include "de/NativePath"
#include "de/ZipArchive"

#include <QList>
#include <map>

namespace de {
namespace game {

DENG2_PIMPL(SavedSessionRepository)
{
    All sessions;
    bool availabilityUpdateDisabled;

    struct FormatInfo {
        String name;
        MapStateReaderMakeFunc newMapStateReader;

        FormatInfo(String name, MapStateReaderMakeFunc newMapStateReader)
            : name(name), newMapStateReader(newMapStateReader)
        {
            DENG2_ASSERT(!name.isEmpty() && newMapStateReader != 0);
        }
    };
    typedef QList<FormatInfo> FormatInfos;
    FormatInfos formats;

    Instance(Public *i)
        : Base(i)
        , availabilityUpdateDisabled(false)
    {}

    ~Instance()
    {
        self.clear();
    }

    void notifyAvailabilityUpdate()
    {
        if(availabilityUpdateDisabled) return;
        DENG2_FOR_PUBLIC_AUDIENCE2(AvailabilityUpdate, i) i->repositoryAvailabilityUpdate(self);
    }

    FormatInfo const *infoForMapStateFormat(String const &name)
    {
        foreach(FormatInfo const &fmtInfo, formats)
        {
            if(!fmtInfo.name.compareWithoutCase(name))
            {
                return &fmtInfo;
            }
        }
        return 0;
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

    // Clear the session db, we're starting over.
    qDebug() << "Clearing saved sessions in the repository";
    DENG2_FOR_EACH(All, i, d->sessions) { delete i->second; }
    d->sessions.clear();

    // Now perform the availability notifications.
    d->availabilityUpdateDisabled = false;
    d->notifyAvailabilityUpdate();
}

bool SavedSessionRepository::has(String path) const
{
    return d->sessions.find(path.toLower()) != d->sessions.end();
}

void SavedSessionRepository::add(String path, SavedSession *session)
{
    path = path.toLower();

    // Ensure the session identifier (i.e., the relative path) is unique.
    All::iterator existing = d->sessions.find(path);
    if(existing != d->sessions.end())
    {
        if(existing->second != session)
        {
            delete existing->second;
            existing->second = session;
            d->notifyAvailabilityUpdate();
        }
    }
    else
    {
        d->sessions[path] = session;
        d->notifyAvailabilityUpdate();
    }
}

SavedSession &SavedSessionRepository::find(String path) const
{
    All::iterator found = d->sessions.find(path.toLower());
    if(found != d->sessions.end())
    {
        return *found->second;
    }
    /// @throw MissingSessionError An unknown session was referenced.
    throw MissingSessionError("SavedSessionRepository::find", "Unknown session '" + path + "'");
}

SavedSession *SavedSessionRepository::findByUserDescription(String description) const
{
    if(!description.isEmpty())
    {
        DENG2_FOR_EACH_CONST(All, i, d->sessions)
        {
            SessionMetadata const &metadata = i->second->metadata();
            if(!metadata.gets("userDescription", "").compareWithoutCase(description))
            {
                return i->second;
            }
        }
    }
    return 0; // Not found.
}

SavedSessionRepository::All const &SavedSessionRepository::all() const
{
    return d->sessions;
}

void SavedSessionRepository::declareMapStateReader(String formatName, MapStateReaderMakeFunc maker)
{
    d->formats << Instance::FormatInfo(formatName, maker);
}

void SavedSessionRepository::clearMapStateReaders()
{
    d->formats.clear();
}

MapStateReader *SavedSessionRepository::makeMapStateReader(SavedSession const &session) const
{
    /// @todo Recognize the map state file to determine the format.
    Instance::FormatInfo const *fmtInfo = d->infoForMapStateFormat("Native");
    DENG2_ASSERT(fmtInfo != 0);
    return fmtInfo->newMapStateReader(session);
}

} // namespace game
} // namespace de
