/**
 * @file saveinfo.h
 * Save state info.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_SAVEINFO_H
#define LIBCOMMON_SAVEINFO_H

#include "p_savedef.h"

typedef struct saveheader_s {
    int magic;
    int version;
    gamemode_t gameMode;
    char name[SAVESTRINGSIZE];
    byte skill;
    byte episode;
    byte map;
    byte deathmatch;
    byte noMonsters;
#if __JHEXEN__
    byte randomClasses;
#else
    byte respawnMonsters;
    int mapTime;
    byte players[MAXPLAYERS];
#endif
    unsigned int gameId;
} saveheader_t;

typedef struct saveinfo_s {
    ddstring_t filePath;
    ddstring_t name;
    saveheader_t header;
} saveinfo_t;

void SaveInfo_SetFilePath(saveinfo_t* info, ddstring_t* newFilePath);

void SaveInfo_SetGameId(saveinfo_t* info, uint newGameId);

void SaveInfo_SetName(saveinfo_t* info, const char* newName);

void SaveInfo_Configure(saveinfo_t* info);

void SaveInfo_Update(saveinfo_t* info);

/**
 * Serializes the save info using @a writer.
 *
 * @param info  SaveInfo instance.
 * @param writer  Writer instance.
 */
void SaveInfo_Write(saveinfo_t* info, Writer* writer);

/**
 * Deserializes the save info using @a reader.
 *
 * @param info  SaveInfo instance.
 * @param reader  Reader instance.
 */
void SaveInfo_Read(saveinfo_t* info, Reader* reader);

#if __JHEXEN__
/**
 * @brief libhexen specific version of @see SaveInfo_Read() for deserializing
 * legacy version 9 save state info.
 */
void SaveInfo_Read_Hx_v9(saveinfo_t* info, Reader* reader);
#endif

#endif /* LIBCOMMON_SAVEINFO_H */
