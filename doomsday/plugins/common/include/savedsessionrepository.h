/** @file savedsessionrepository.h  Saved (game) session repository.
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

#ifndef LIBCOMMON_SAVEDSESSIONREPOSITORY_H
#define LIBCOMMON_SAVEDSESSIONREPOSITORY_H

#include "sessionmetadata.h"
#include <de/Error>
#include <de/Observers>
#include <de/Path>
#include <de/String>

/**
 * @todo Move to ResourceSystem on engine side.
 */
class SavedSessionRepository
{
public:
    /// Required/referenced savegame is missing. @ingroup errors
    DENG2_ERROR(UnknownSessionError);

    /**
     * Logical component representing a saved game session.
     *
     * @ingroup libcommon
     */
    class SessionRecord
    {
    public:
        /// Notified whenever the status of the saved game session changes.
        DENG2_DEFINE_AUDIENCE(SessionStatusChange, void sessionRecordStatusChanged(SessionRecord &record))

        /// Notified whenever the user description of saved game session changes.
        DENG2_DEFINE_AUDIENCE(UserDescriptionChange, void sessionRecordUserDescriptionChanged(SessionRecord &record))

        /// Logical game session status:
        enum SessionStatus {
            Loadable,
            Incompatible,
            Unused
        };

    public:
        SessionRecord(de::String const &fileName = "");
        SessionRecord(SessionRecord const &other);

        SessionRecord &operator = (SessionRecord const &other);

        /**
         * Returns the saved game repository which owns the record.
         */
        SavedSessionRepository &repository() const;
        void setRepository(SavedSessionRepository *newRepository);

        /**
         * Returns the logical status of the saved game session. The SessionStatusChange audience
         * is notified whenever the status changes.
         *
         * @see statusAsText()
         */
        SessionStatus status() const;

        /**
         * Returns a textual representation of the current status of the saved game session.
         *
         * @see status()
         */
        de::String statusAsText() const;

        /**
         * Composes a human-friendly, styled, textual description of the saved game session.
         */
        de::String description() const;

        /**
         * Determines whether a saved game session exists. However, it may not be compatible with
         * the current game session.
         *
         * @see gameSessionIsLoadable()
         */
        bool haveGameSession() const;

        /**
         * Determines whether a saved game session exists and is compatibile with the current game
         * session (and @em should therefore be loadable).
         */
        inline bool gameSessionIsLoadable() const { return status() == Loadable; }

        /**
         * Determines whether a saved map session exists.
         *
         * @param mapUri   Unique map identifier.
         */
        bool haveMapSession(Uri const *mapUri) const;

        /**
         * Attempt to update the save info from the named saved game session file. If the save path
         * is invalid, unreachable, or the game state is not recognized -- the save info is returned
         * to a valid but non-loadable state.
         *
         * @see gameSessionIsLoadable()
         */
        void updateFromFile();

        /**
         * Returns the name of the resource file (with extension) containing the game session header.
         */
        de::String fileName() const;
        void setFileName(de::String newName);

        /**
         * Returns the name of the resource file (with extension) containing the map session state.
         *
         * @param mapUri   Unique map identifier.
         *
         * @see fileName()
         */
        de::String fileNameForMap(Uri const *mapUri) const;

        /**
         * Provides read-only access to a copy of the deserialized game session metadata.
         */
        SessionMetadata const &meta() const;

        /**
         * Deserializes the game session metadata using @a reader.
         */
        void readMeta(Reader *reader);

        void applyCurrentSessionMetadata();

        // Metadata manipulation:
        void setMagic(int newMagic);
        void setVersion(int newVersion);
        void setSessionId(uint newSessionId);
        void setGameIdentityKey(de::String newGameIdentityKey);
        void setGameRules(GameRuleset const &newRules);
        void setUserDescription(de::String newUserDescription);
        void setMapUri(Uri const *newMapUri);
#if !__JHEXEN__
        void setMapTime(int newMapTime);
        void setPlayers(SessionMetadata::Players const &newPlayers);
#endif // !__JHEXEN__

    private:
        DENG2_PRIVATE(d)
    };

public:
    SavedSessionRepository();

    /**
     * Create the saved game directories.
     */
    void setupSaveDirectory(de::Path newRootSaveDir, de::String saveFileExtension);

    de::Path const &savePath() const;

    de::Path const &clientSavePath() const;

    de::String const &saveFileExtension() const;

    /**
     * Add an empty record for a saved game session to the repository.
     *
     * @param fileName  Name of the game session file to associate with.
     */
    void addRecord(de::String fileName);

    /**
     * Determines whether saved game session record exists for @a fileName.
     *
     * @see info()
     */
    bool hasRecord(de::String fileName) const;

    /**
     * Lookup the SaveInfo for @a fileName.
     *
     * @see hasInfo()
     */
    SessionRecord &record(de::String fileName) const;

    /**
     * Replace the existing save info with @a newInfo.
     *
     * @param fileName  File name to replace the info of.
     * @param newInfo   New SaveInfo to replace with. Ownership is given.
     */
    void replaceRecord(de::String fileName, SessionRecord *newInfo);

public: /// @todo refactor away
    SessionRecord *newRecord(de::String const &fileName = "",
        de::String const &userDescription = "");

private:
    DENG2_PRIVATE(d)
};

typedef SavedSessionRepository::SessionRecord SessionRecord;

#endif // LIBCOMMON_SAVEDSESSIONREPOSITORY_H
