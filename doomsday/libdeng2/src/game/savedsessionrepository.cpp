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
, DENG2_OBSERVES(App, GameUnload)
//, DENG2_OBSERVES(App, GameChange)
{
    Folder *folder; ///< Root of the saved session repository file structure.

    typedef std::map<String, SavedSession *> Sessions;
    Sessions sessions;

    struct ReaderInfo {
        String formatName;
        MapStateReaderMakeFunc newReader;

        ReaderInfo(String formatName, MapStateReaderMakeFunc newReader)
            : formatName(formatName), newReader(newReader)
        {
            DENG2_ASSERT(!formatName.isEmpty() && newReader != 0);
        }
    };
    typedef QList<ReaderInfo> ReaderInfos;
    ReaderInfos readers;

    Instance(Public *i)
        : Base(i)
        , folder(0)
    {
        App::app().audienceForGameUnload() += this;
        //App::app().audienceForGameChange() += this;
    }

    ~Instance()
    {
        App::app().audienceForGameUnload() += this;
        //App::app().audienceForGameChange() += this;

        clearSessions();
    }

    void clearSessions()
    {
        qDebug() << "Clearing saved sessions in the repository";
        DENG2_FOR_EACH(Sessions, i, sessions) { delete i->second; }
        sessions.clear();
    }

    ReaderInfo const *infoForMapStateFormat(String const &name) const
    {
        foreach(ReaderInfo const &rdrInfo, readers)
        {
            if(!rdrInfo.formatName.compareWithoutCase(name))
            {
                return &rdrInfo;
            }
        }
        return 0;
    }

    void aboutToUnloadGame(game::Game const & /*gameBeingUnloaded*/)
    {
        // Remove the saved sessions (not serialized state files).
        /// @note Once the game state file is read with an engine-side file reader, clearing
        /// the sessions at this time is not necessary.
        clearSessions();

        // Clear the registered game state readers too.
        readers.clear();
    }
};

SavedSessionRepository::SavedSessionRepository() : d(new Instance(this))
{}

Folder const &SavedSessionRepository::folder() const
{
    DENG2_ASSERT(d->folder != 0);
    return *d->folder;
}

Folder &SavedSessionRepository::folder()
{
    DENG2_ASSERT(d->folder != 0);
    return *d->folder;
}

void SavedSessionRepository::setLocation(Path const &location)
{
    qDebug() << "(Re)Initializing saved session repository file structure at" << location.asText();

    // Clear the saved session db, we're starting over.
    d->clearSessions();

    if(!location.isEmpty())
    {
        // Initialize the file structure.
        try
        {
            d->folder = &App::fileSystem().makeFolder(location, FileSystem::PopulateNewFolder);
            /// @todo scan the indexed files and populate the saved session db.
            return;
        }
        catch(Error const &)
        {}
    }

    LOG_RES_ERROR("\"%s\" could not be accessed. Perhaps it could not be created (insufficient permissions?)."
                  " Saving will not be possible.") << NativePath(location).pretty();
}

bool SavedSessionRepository::contains(String fileName) const
{
    return d->sessions.find(fileName) != d->sessions.end();
}

void SavedSessionRepository::add(String fileName, SavedSession *session)
{
    // Ensure the session identifier is unique.
    Instance::Sessions::iterator existing = d->sessions.find(fileName);
    if(existing != d->sessions.end())
    {
        if(existing->second != session)
        {
            delete existing->second;
            existing->second = session;
        }
    }
    else
    {
        d->sessions[fileName] = session;
    }
}

SavedSession &SavedSessionRepository::session(String fileName) const
{
    Instance::Sessions::iterator found = d->sessions.find(fileName);
    if(found != d->sessions.end())
    {
        DENG2_ASSERT(found->second != 0);
        return *found->second;
    }
    /// @throw MissingSessionError An unknown session was referenced.
    throw MissingSessionError("SavedSessionRepository::session", "Unknown session '" + fileName + "'");
}

SavedSession *SavedSessionRepository::sessionByUserDescription(String description) const
{
    if(!description.isEmpty())
    {
        DENG2_FOR_EACH_CONST(Instance::Sessions, i, d->sessions)
        {
            SessionMetadata const &metadata = i->second->metadata();
            if(!metadata.gets("userDescription").compareWithoutCase(description))
            {
                return i->second;
            }
        }
    }
    return 0; // Not found.
}

void SavedSessionRepository::declareReader(String formatName, MapStateReaderMakeFunc maker)
{
    d->readers << Instance::ReaderInfo(formatName, maker);
}

MapStateReader *SavedSessionRepository::makeReader(SavedSession const &session) const
{
    if(session.isLoadable())
    {
        /// @todo Recognize the map state file to determine the format.
        Instance::ReaderInfo const *rdrInfo = d->infoForMapStateFormat("Native");
        DENG2_ASSERT(rdrInfo != 0);
        return rdrInfo->newReader(session);
    }
    /// @throw UnloadableSessionError The saved session is not loadable.
    throw UnloadableSessionError("SavedSessionRepository::makeReader", "SavedSession '" + session.fileName() + "' is not loadable");
}

} // namespace game
} // namespace de
