/** @file saveinfo.h  Saved game session info.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SAVEINFO
#define LIBCOMMON_SAVEINFO

#include "common.h"
#include <de/Observers>
#include <de/String>

/**
 * Game session metadata (serialized to savegames).
 * @ingroup libcommon
 */
struct SessionMetadata
{
#if !__JHEXEN__
    // Info data about players present (or not) in the game session.
    typedef byte Players[MAXPLAYERS];
#endif

    de::String userDescription;
    uint sessionId;
    int magic;
    int version;
    de::String gameIdentityKey;
    Uri *mapUri;
    GameRuleset gameRules;
#if !__JHEXEN__
    int mapTime;
    Players players;
#endif

    SessionMetadata();
    SessionMetadata(SessionMetadata const &other);
    ~SessionMetadata();

    void write(Writer *writer) const;
    void read(Reader *reader);
};

/**
 * Represents a saved game session state.
 *
 * @ingroup libcommon
 */
class SaveInfo
{
public:
    /// Notified whenever the status of the saved game session changes.
    DENG2_DEFINE_AUDIENCE(SessionStatusChange, void saveInfoSessionStatusChanged(SaveInfo &saveInfo))

    /// Notified whenever the user description of saved game session changes.
    DENG2_DEFINE_AUDIENCE(UserDescriptionChange, void saveInfoUserDescriptionChanged(SaveInfo &saveInfo))

    /// Logical game session status:
    enum SessionStatus {
        Loadable,
        Incompatible,
        Unused
    };

public:
    SaveInfo(de::String const &fileName = "");
    SaveInfo(SaveInfo const &other);

    static SaveInfo *newWithCurrentSessionMeta(de::String const &fileName = "",
                                               de::String const &userDescription = "");

    SaveInfo &operator = (SaveInfo const &other);

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
     * @param mapUri   Unique map identifier. If @c 0 the Uri for the @em current map is used.
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
     * @param mapUri   Unique map identifier. If @c 0 the Uri for the @em current map is used.
     *
     * @see fileName()
     */
    de::String fileNameForMap(Uri const *mapUri = 0) const;

    /**
     * Update the metadata associated with the save using values derived from the current game
     * session. Note that this does @em not affect the copy of this save on disk.
     */
    void applyCurrentSessionMeta();

    /**
     * Provides read-only access to a copy of the deserialized game session metadata.
     */
    SessionMetadata const &meta() const;

    /**
     * Deserializes the game session metadata using @a reader.
     */
    void readMeta(Reader *reader);

    // Metadata manipulation:
    void setGameIdentityKey(de::String newGameIdentityKey);
    void setVersion(int newVersion);
    void setUserDescription(de::String newUserDescription);
    void setSessionId(uint newSessionId);
    void setMapUri(Uri const *newMapUri);
#if !__JHEXEN__
    void setMapTime(int newMapTime);
    void setPlayers(SessionMetadata::Players const &newPlayers);
#endif // !__JHEXEN__
    void setGameRules(GameRuleset const &newRules);

public: /// @todo refactor away:
    void setMagic(int newMagic);
    static SaveInfo *fromReader(Reader *reader);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_SAVEINFO
