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

typedef struct gamerules_s {
    skillmode_t skill;
#if !__JHEXEN__
    byte fast;
#endif
    byte deathmatch;
    byte noMonsters;
#if __JHEXEN__
    byte randomClasses;
#else
    byte respawnMonsters;
#endif
} gamerules_t;

typedef struct saveheader_s {
    int magic;
    int version;
    gamemode_t gameMode;
    gamerules_t gameRules;
    byte episode;
    byte map;
#if !__JHEXEN__
    int mapTime;
    byte players[MAXPLAYERS];
#endif
} saveheader_t;

#ifdef __cplusplus
/**
 * Saved game session info.
 */
class SaveInfo
{
public: /// @todo make private:
    Str _description;
    uint _gameId;
    saveheader_t _header;

public:
    SaveInfo();
    SaveInfo(SaveInfo const &other);
    ~SaveInfo();

    SaveInfo &operator = (SaveInfo const &other);

    void configure();

    /**
     * Returns @a true if the game session is compatibile with the current game session
     * and @em should therefore be loadable.
     */
    bool isLoadable();

    /**
     * Returns the logical version of the serialized game session state.
     */
    int version() const;
    int magic() const;

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

    uint episode() const;
    uint map() const;
    gamerules_t const &gameRules() const;

    /**
     * Serializes the game session info using @a writer.
     */
    void write(Writer *writer) const;

    /**
     * Deserializes the game session info using @a reader.
     */
    void read(Reader *reader);

#if __JHEXEN__
    /**
     * @brief libhexen specific version of @ref SaveInfo_Read() for deserializing
     * legacy version 9 game session info.
     */
    void read_Hx_v9(Reader *reader);
#endif

    /**
     * Provides readonly access to the game session header.
     * @todo refactor away.
     */
    saveheader_t const *header() const;
};

#endif // __cplusplus

// C wrapper API, for legacy modules -------------------------------------------

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

saveheader_t const *SaveInfo_Header(SaveInfo const *info);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_SAVEINFO
