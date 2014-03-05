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
#include "de/game/IGameStateReader"
#include "de/Log"
#include "de/NativePath"

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
        GameStateRecognizeFunc recognize;
        GameStateReaderMakeFunc newReader;

        ReaderInfo(GameStateRecognizeFunc recognize, GameStateReaderMakeFunc newReader)
            : recognize(recognize), newReader(newReader)
        {
            DENG2_ASSERT(recognize != 0 && newReader != 0);
        }
    };
    typedef QList<ReaderInfo> ReaderInfos;
    ReaderInfos readers;

    Instance(Public *i)
        : Base(i)
        , folder(0)
    {
        App::app().audienceForGameUnload += this;
        //App::app().audienceForGameChange += this;
    }

    ~Instance()
    {
        App::app().audienceForGameUnload += this;
        //App::app().audienceForGameChange += this;

        clearSessions();
    }

    void clearSessions()
    {
        qDebug() << "Clearing saved sessions in the repository";
        DENG2_FOR_EACH(Sessions, i, sessions) { delete i->second; }
        sessions.clear();
    }

    ReaderInfo const *readSessionMetadata(SavedSession &session) const
    {
        foreach(ReaderInfo const &rdrInfo, readers)
        {
            Path stateFilePath = self.folder().path() / session.fileName();

            // Attempt to recognize the game state and deserialize the metadata.
            QScopedPointer<SessionMetadata> metadata(new SessionMetadata);
            if(rdrInfo.recognize(stateFilePath, *metadata))
            {
                session.replaceMetadata(metadata.take());
                return &rdrInfo;
            }
        }
        return 0; // Unrecognized
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
    qDebug() << "(Re)Initializing saved session repository file structure";

    // Clear the saved session db, we're starting over.
    d->clearSessions();

    if(!location.isEmpty())
    {
        // Initialize the file structure.
        try
        {
            d->folder = &App::fileSystem().makeFolder(location);
            /// @todo attach a feed to this folder.
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

void SavedSessionRepository::declareReader(GameStateRecognizeFunc recognizer, GameStateReaderMakeFunc maker)
{
    d->readers.append(Instance::ReaderInfo(recognizer, maker));
}

bool SavedSessionRepository::recognize(SavedSession &session) const
{
    return d->readSessionMetadata(session) != 0;
}

IGameStateReader *SavedSessionRepository::recognizeAndMakeReader(SavedSession &session) const
{
    if(Instance::ReaderInfo const *rdrInfo = d->readSessionMetadata(session))
    {
        return rdrInfo->newReader();
    }
    return 0; // Unrecognized
}

} // namespace game
} // namespace de
