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

#if !__JHEXEN__
    // Info data about players present (or not) in the game session.
    typedef byte Players[MAXPLAYERS];
#endif

public:
    SaveInfo(de::String const &fileName = "");
    SaveInfo(SaveInfo const &other);

    static SaveInfo *newWithCurrentSessionMetadata(de::String const &fileName = "",
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
    void applyCurrentSessionMetadata();

    /**
     * Returns the unique "identity key" of the game session.
     */
    de::String const &gameIdentityKey() const;
    void setGameIdentityKey(de::String newGameIdentityKey);

    /**
     * Returns the logical version of the serialized game session state.
     */
    int version() const;
    void setVersion(int newVersion);

    /**
     * Returns the textual description of the saved game session provided by the user. The
     * UserDescriptionChange audience is notified whenever the description changes.
     */
    de::String const &userDescription() const;
    void setUserDescription(de::String newUserDescription);

    /**
     * @see G_GenerateSessionId()
     */
    uint sessionId() const;
    void setSessionId(uint newSessionId);

    /**
     * Returns the URI of the @em current map of the game session.
     */
    Uri const *mapUri() const;
    void setMapUri(Uri const *newMapUri);

#if !__JHEXEN__

    /**
     * Returns the expired time in tics since the @em current map of the game session began.
     */
    int mapTime() const;
    void setMapTime(int newMapTime);

    /**
     * Returns the player info data for the game session.
     */
    Players const &players() const;
    void setPlayers(Players const &newPlayers);

#endif // !__JHEXEN__

    /**
     * Returns the game ruleset for the game session.
     */
    GameRuleset const &gameRules() const;
    void setGameRules(GameRuleset const &newRules);

    /**
     * Serializes the game session header using @a writer.
     */
    void write(Writer *writer) const;

    /**
     * Deserializes the game session header using @a reader.
     */
    void read(Reader *reader);

public: /// @todo refactor away:
    int magic() const;
    void setMagic(int newMagic);
    static SaveInfo *fromReader(Reader *reader);

private:
    DENG2_PRIVATE(d)
};

#endif // LIBCOMMON_SAVEINFO
