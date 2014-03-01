/** @file savedsessionrepository.cpp  Saved (game) session repository.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "savedsessionrepository.h"

#include "g_common.h"
#include "gamestatereader.h"
#include "p_saveio.h"
#include "p_savedef.h"
#include <de/Log>
#include <de/NativePath>
#include <cstring>
#include <map>

namespace internal
{
    static bool usingSeparateMapSessionFiles()
    {
#ifdef __JHEXEN__
        return true;
#else
        return false;
#endif
    }
}

using namespace de;
using namespace internal;

DENG2_PIMPL(SavedSessionRepository::SessionRecord)
{
    SavedSessionRepository *repo; ///< The owning repository (if any).

    String fileName; ///< Name of the game session file.

    SessionMetadata meta;
    SessionStatus status;
    bool needUpdateStatus;

    Instance(Public *i)
        : Base(i)
        , repo            (0)
        , status          (Unused)
        , needUpdateStatus(true)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , repo            (other.repo)
        , fileName        (other.fileName)
        , meta            (other.meta)
        , status          (other.status)
        , needUpdateStatus(other.needUpdateStatus)
    {}

    void updateStatusIfNeeded()
    {
        if(!needUpdateStatus) return;

        needUpdateStatus = false;
        LOGDEV_XVERBOSE("Updating SaveInfo %p status") << thisPublic;

        SessionStatus const oldStatus = status;

        status = Unused;
        if(self.haveGameSession())
        {
            status = Incompatible;
            // Game identity key missmatch?
            if(!meta.gameIdentityKey.compareWithoutCase(G_IdentityKey()))
            {
                /// @todo Validate loaded add-ons and checksum the definition database.
                status = Loadable; // It's good!
            }
        }

        if(status != oldStatus)
        {
            DENG2_FOR_PUBLIC_AUDIENCE(SessionStatusChange, i)
            {
                i->sessionRecordStatusChanged(self);
            }
        }
    }
};

SavedSessionRepository::SessionRecord::SessionRecord(String const &fileName)
    : d(new Instance(this))
{
    d->fileName = fileName;
}

SavedSessionRepository::SessionRecord::SessionRecord(SessionRecord const &other)
    : d(new Instance(this, *other.d))
{}

SessionRecord &SavedSessionRepository::SessionRecord::operator = (SessionRecord const &other)
{
    d.reset(new Instance(this, *other.d));
    return *this;
}

SavedSessionRepository &SavedSessionRepository::SessionRecord::repository() const
{
    DENG2_ASSERT(d->repo != 0);
    return *d->repo;
}

void SavedSessionRepository::SessionRecord::setRepository(SavedSessionRepository *newRepository)
{
    d->repo = newRepository;
}

SessionRecord::SessionStatus SavedSessionRepository::SessionRecord::status() const
{
    d->updateStatusIfNeeded();
    return d->status;
}

String SavedSessionRepository::SessionRecord::statusAsText() const
{
    static String const statusTexts[] = {
        "Loadable",
        "Incompatible",
        "Unused",
    };
    return statusTexts[int(status())];
}

String SavedSessionRepository::SessionRecord::description() const
{
    return meta().asText() + "\n" +
           String(_E(l) "Source file: " _E(.)_E(i) "\"%1\"\n" _E(.)
                  _E(D) "Status: " _E(.) "%2")
               .arg(NativePath(repository().savePath() / fileName()).pretty())
               .arg(statusAsText());
}

String SavedSessionRepository::SessionRecord::fileName() const
{
    return d->fileName + "." + repository().saveFileExtension();
}

void SavedSessionRepository::SessionRecord::setFileName(String newName)
{
    if(d->fileName != newName)
    {
        d->fileName         = newName;
        d->needUpdateStatus = true;
    }
}

String SavedSessionRepository::SessionRecord::fileNameForMap(Uri const *mapUri) const
{
    DENG2_ASSERT(mapUri != 0);
    uint map = G_MapNumberFor(mapUri);
    return d->fileName + String("%1.").arg(int(map + 1), 2, 10, QChar('0'))
                       + repository().saveFileExtension();
}

bool SavedSessionRepository::SessionRecord::haveGameSession() const
{
    return SV_ExistingFile(repository().savePath() / fileName());
}

bool SavedSessionRepository::SessionRecord::haveMapSession(Uri const *mapUri) const
{
    if(usingSeparateMapSessionFiles())
    {
        return SV_ExistingFile(repository().savePath() / fileNameForMap(mapUri));
    }
    return haveGameSession();
}

void SavedSessionRepository::SessionRecord::updateFromFile()
{
    LOGDEV_VERBOSE("Updating saved game SessionRecord %p from source file") << this;

    // Is this a recognized game state?
    if(G_GameStateReaderFactory().recognize(*this))
    {
        // Ensure we have a valid description.
        if(d->meta.userDescription.isEmpty())
        {
            setUserDescription("UNNAMED");
        }
    }
    else
    {
        // Unrecognized or the file could not be accessed (perhaps its a network path?).
        // Clear the info.
        setUserDescription("");
        setSessionId(0);
    }

    d->updateStatusIfNeeded();
}

SessionMetadata const &SavedSessionRepository::SessionRecord::meta() const
{
    return d->meta;
}

void SavedSessionRepository::SessionRecord::readMeta(reader_s *reader)
{
    d->meta.read(reader);
    d->needUpdateStatus = true;
}

void SavedSessionRepository::SessionRecord::applyCurrentSessionMetadata()
{
    d->meta.magic   = IS_NETWORK_CLIENT? MY_CLIENT_SAVE_MAGIC : MY_SAVE_MAGIC;
    d->meta.version = MY_SAVE_VERSION;
    G_ApplyCurrentSessionMetadata(d->meta);

    d->needUpdateStatus = true;
}

void SavedSessionRepository::SessionRecord::setGameIdentityKey(String newGameIdentityKey)
{
    if(d->meta.gameIdentityKey != newGameIdentityKey)
    {
        d->meta.gameIdentityKey  = newGameIdentityKey;
        d->needUpdateStatus = true;
    }
}

void SavedSessionRepository::SessionRecord::setGameRules(GameRuleset const &newRules)
{
    d->meta.gameRules = newRules; // Make a copy
    d->needUpdateStatus = true;
}

void SavedSessionRepository::SessionRecord::setMagic(int newMagic)
{
    if(d->meta.magic != newMagic)
    {
        d->meta.magic = newMagic;
        d->needUpdateStatus = true;
    }
}

void SavedSessionRepository::SessionRecord::setVersion(int newVersion)
{
    if(d->meta.version != newVersion)
    {
        d->meta.version = newVersion;
        d->needUpdateStatus = true;
    }
}

void SavedSessionRepository::SessionRecord::setUserDescription(String newUserDescription)
{
    if(d->meta.userDescription != newUserDescription)
    {
        d->meta.userDescription = newUserDescription;
        DENG2_FOR_AUDIENCE(UserDescriptionChange, i)
        {
            i->sessionRecordUserDescriptionChanged(*this);
        }
    }
}

void SavedSessionRepository::SessionRecord::setSessionId(uint newSessionId)
{
    if(d->meta.sessionId != newSessionId)
    {
        d->meta.sessionId = newSessionId;
        d->needUpdateStatus = true;
    }
}

void SavedSessionRepository::SessionRecord::setMapUri(Uri const *newMapUri)
{
    DENG2_ASSERT(newMapUri != 0);
    Uri_Copy(d->meta.mapUri, newMapUri);
}

#if !__JHEXEN__
void SavedSessionRepository::SessionRecord::setMapTime(int newMapTime)
{
    d->meta.mapTime = newMapTime;
}

void SavedSessionRepository::SessionRecord::setPlayers(SessionMetadata::Players const &newPlayers)
{
    std::memcpy(d->meta.players, newPlayers, sizeof(d->meta.players));
}
#endif // !__JHEXEN__

DENG2_PIMPL_NOREF(SavedSessionRepository)
{
    Path savePath;            ///< e.g., "savegame"
    Path clientSavePath;      ///< e.g., "savegame/client"
    String saveFileExtension;

    typedef std::map<String, SessionRecord *> Records;
    typedef std::pair<String, SessionRecord *> RecordPair;
    Records records;

    ~Instance()
    {
        DENG2_FOR_EACH(Records, i, records) { delete i->second; }
    }

    SessionRecord *recordFor(String sessionFileName)
    {
        Records::iterator found = records.find(sessionFileName);
        if(found != records.end())
        {
            return found->second;
        }
        return 0; // Not found.
    }
};

SavedSessionRepository::SavedSessionRepository() : d(new Instance)
{}

void SavedSessionRepository::setupSaveDirectory(Path newRootSaveDir, String saveFileExtension)
{
    LOG_AS("SavedSessionRepository");

    d->saveFileExtension = saveFileExtension;

    if(!newRootSaveDir.isEmpty())
    {
        d->savePath       = newRootSaveDir;
        d->clientSavePath = newRootSaveDir / "client";

        // Ensure that these paths exist.
        bool savePathExists = F_MakePath(NativePath(d->savePath).expand().toUtf8().constData());

#if !__JHEXEN__
        if(!F_MakePath(NativePath(d->clientSavePath).expand().toUtf8().constData()))
        {
            savePathExists = false;
        }
#endif

        if(savePathExists)
        {
            return;
        }
    }

    d->savePath.clear();
    d->clientSavePath.clear();

    LOG_RES_ERROR("\"%s\" could not be accessed. Perhaps it could not be created (insufficient permissions?)."
                  " Saving will not be possible.") << NativePath(d->savePath).pretty();
}

Path const &SavedSessionRepository::savePath() const
{
    return d->savePath;
}

Path const &SavedSessionRepository::clientSavePath() const
{
    return d->clientSavePath;
}

String const &SavedSessionRepository::saveFileExtension() const
{
    return d->saveFileExtension;
}

void SavedSessionRepository::addRecord(String fileName)
{
    // Ensure the session identifier is unique.
    if(d->recordFor(fileName)) return;

    // Insert an empty record for the new session.
    d->records.insert(Instance::RecordPair(fileName, (SessionRecord *)0));
}

bool SavedSessionRepository::hasRecord(String fileName) const
{
    return d->recordFor(fileName) != 0;
}

SessionRecord &SavedSessionRepository::record(String fileName) const
{
    if(SessionRecord *record = d->recordFor(fileName))
    {
        return *record;
    }
    /// @throw UnknownSessionError An unknown savegame was referenced.
    throw UnknownSessionError("SavedSessionRepository::record", "Unknown session '" + fileName + "'");
}

void SavedSessionRepository::replaceRecord(String fileName, SessionRecord *newRecord)
{
    DENG2_ASSERT(newRecord != 0);

    Instance::Records::iterator found = d->records.find(fileName);
    if(found != d->records.end())
    {
        if(newRecord != found->second)
        {
            delete found->second;
            found->second = newRecord;
        }
        return;
    }
    /// @throw UnknownSessionError An unknown savegame was referenced.
    throw UnknownSessionError("SavedSessionRepository::replaceRecord", "Unknown session '" + fileName + "'");
}

SavedSessionRepository::SessionRecord *SavedSessionRepository::newRecord(
    String const &fileName, String const &userDescription) // static
{
    SessionRecord *info = new SessionRecord(fileName);
    info->setRepository(this);
    info->setUserDescription(userDescription);
    return info;
}
