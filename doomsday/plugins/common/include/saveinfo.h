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

#include "doomsday.h"
#include "common.h"

#ifdef __cplusplus
/**
 * Saved game session info.
 */
class SaveInfo
{
public: /// @todo make private:
    Str _description;
    uint _gameId;
    int _magic;
    int _version;
    gamemode_t _gameMode;
    uint _episode;
    uint _map;
    Uri *_mapUri; ///< Not currently saved.
#if !__JHEXEN__
    int _mapTime;
    byte _players[MAXPLAYERS];
#endif
    GameRuleset _gameRules;

public:
    SaveInfo();
    SaveInfo(SaveInfo const &other);
    ~SaveInfo();

    SaveInfo &operator = (SaveInfo const &other);

    /**
     * Update the metadata associated with the save using values derived from the
     * current game session. Note that this does @em not affect the copy of this save
     * on disk.
     */
    void applyCurrentSessionMetadata();

    /**
     * Determines whether the saved game session is compatibile with the current
     * game session (and @em should therefore be loadable).
     */
    bool isLoadable();

    /**
     * Returns the logical version of the serialized game session state.
     */
    int version() const;

    /**
     * Returns the textual description of the game session (provided by the user).
     */
    Str const *description() const;
    void setDescription(Str const *newDesc);

    /**
     * @see SV_GenerateGameId()
     */
    uint gameId() const;
    void setGameId(uint newGameId);

    /**
     * Returns the URI of the @em current map of the game session.
     */
    Uri const *mapUri() const;

#if !__JHEXEN__
    /**
     * Returns the expired time in tics since the @em current map of the game session began.
     */
    int mapTime() const;
#endif

    /**
     * Returns the game ruleset for the game session.
     */
    GameRuleset const &gameRules() const;

    /**
     * Serializes the game session info using @a writer.
     */
    void write(Writer *writer) const;

    /**
     * Deserializes the game session info using @a reader.
     */
    void read(Reader *reader);

    /**
     * Hexen-specific version for deserializing legacy v.9 game session info.
     */
#if __JHEXEN__
    void read_Hx_v9(Reader *reader);
#endif

public: /// @todo refactor away:
    int magic() const;
};

#endif // __cplusplus

// C wrapper API ---------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#else
typedef void *SaveInfo;
#endif

SaveInfo *SaveInfo_New(void);
SaveInfo *SaveInfo_Dup(SaveInfo const *other);

void SaveInfo_Delete(SaveInfo *info);

SaveInfo *SaveInfo_Copy(SaveInfo *info, SaveInfo const *other);

dd_bool SaveInfo_IsLoadable(SaveInfo *info);

Str const *SaveInfo_Description(SaveInfo const *info);
void SaveInfo_SetDescription(SaveInfo *info, Str const *newName);

uint SaveInfo_GameId(SaveInfo const *info);
void SaveInfo_SetGameId(SaveInfo *info, uint newGameId);

void SaveInfo_Write(SaveInfo *info, Writer *writer);

void SaveInfo_Read(SaveInfo *info, Reader *reader);

#if __JHEXEN__
void SaveInfo_Read_Hx_v9(SaveInfo *info, Reader *reader);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_SAVEINFO
