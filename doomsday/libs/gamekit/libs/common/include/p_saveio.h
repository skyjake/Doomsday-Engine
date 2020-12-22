/** @file p_saveio.h  Game save file IO.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/file.h>
#include <de/reader.h>
#include <de/writer.h>
#include <de/legacy/reader.h>
#include <de/legacy/writer.h>

/*
 * File management
 */

void SV_CloseFile();
bool SV_OpenFileForRead(const de::File &file);
bool SV_OpenFileForWrite(de::IByteArray &block);

Writer1 *SV_NewWriter();

/// Provides access to the wrapped de::Writer instance used for serialization.
de::Writer &SV_RawWriter();

Reader1 *SV_NewReader();

/// Provides access to the wrapped de::Reader instance used for deserialization.
de::Reader &SV_RawReader();

#endif // LIBCOMMON_SAVESTATE_INPUT_OUTPUT_H
