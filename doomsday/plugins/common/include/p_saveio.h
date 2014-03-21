/** @file p_saveio.h  Game save file IO.
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

#ifndef LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
#define LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H

//#include <de/game/SavedSession>
//#include <de/Path>
#include <de/File>
#include <de/Reader>
#include <de/Writer>
#include <de/reader.h>
#include <de/writer.h>

typedef enum savestatesegment_e {
    ASEG_MAP_HEADER = 102,  // Hexen only
    ASEG_MAP_ELEMENTS,
    ASEG_POLYOBJS,          // Hexen only
    ASEG_MOBJS,             // Hexen < ver 4 only
    ASEG_THINKERS,
    ASEG_SCRIPTS,           // Hexen only
    ASEG_PLAYERS,
    ASEG_SOUNDS,            // Hexen only
    ASEG_MISC,              // Hexen only
    ASEG_END,               // = 111
    ASEG_MATERIAL_ARCHIVE,
    ASEG_MAP_HEADER2,
    ASEG_PLAYER_HEADER,
    ASEG_WORLDSCRIPTDATA   // Hexen only
} savestatesegment_t;

/*
 * File management
 */

void SV_CloseFile();
bool SV_OpenFileForRead(de::File const &file);
bool SV_OpenFileForWrite(de::IByteArray &block);

#if 0
bool SV_OpenFile_LZSS(de::Path filePath);
void SV_CloseFile_LZSS();

/**
 * Exit with a fatal error if the value at the current location in the
 * game-save file does not match that associated with the segment id.
 *
 * @param segmentId  Identifier of the segment to check alignment of.
 */
void SV_AssertSegment(int segmentId);

void SV_BeginSegment(int segmentId);

void SV_EndSegment();

void SV_WriteSessionMetadata(de::game::SessionMetadata const &metadata, Writer *writer);

void SV_WriteConsistencyBytes();

void SV_ReadConsistencyBytes();
#endif

Writer *SV_NewWriter();

/// Provides access to the wrapped de::Writer instance used for serialization.
de::Writer &SV_RawWriter();

Reader *SV_NewReader();

/// Provides access to the wrapped de::Reader instance used for deserialization.
de::Reader &SV_RawReader();

#endif // LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
