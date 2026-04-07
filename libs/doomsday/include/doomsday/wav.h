/** @file wav.h  WAV loader (obsolete). @ingroup audio
 *
 * @deprecated There is a better/newer WAV loader (de::Waveform).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_RESOURCE_WAV_H
#define LIBDOOMSDAY_RESOURCE_WAV_H

#include "libdoomsday.h"
#include <de/legacy/types.h>

/**
 * Verifies that the data in the buffer @a data looks like WAV.
 * @return @c true, if the "RIFF" and "WAVE" strings are found.
 */
LIBDOOMSDAY_PUBLIC int WAV_CheckFormat(const char* data);

/**
 * Loads a WAV sample from a memory buffer. All parameters must be passed, no
 * NULLs are allowed.
 *
 * @note The WAV file must contain a mono sound: only one channel.
 *
 * @param data        Source data.
 * @param datalength  Length of the source data.
 * @param bits        Bits per sample is written here.
 * @param rate        Sample rate is written here.
 * @param samples     Number of samples is written here.
 *
 * @return Buffer that contains the wave data. The caller must free the sample
 * data using Z_Free() when it's no longer needed.
 */
LIBDOOMSDAY_PUBLIC void* WAV_MemoryLoad(const byte* data, size_t datalength, int* bits, int* rate, int* samples);


/**
 * Loads a WAV sample from a file. All parameters must be passed, no NULLs are
 * allowed.
 *
 * @note The WAV file must contain a mono sound: only one channel.
 *
 * @param filename    File path of the WAV file.
 * @param bits        Bits per sample is written here.
 * @param rate        Sample rate is written here.
 * @param samples     Number of samples is written here.
 *
 * @return Buffer that contains the wave data. The caller must free the sample
 * data using Z_Free() when it's no longer needed.
 */
LIBDOOMSDAY_PUBLIC void* WAV_Load(const char* filename, int* bits, int* rate, int* samples);

#endif // LIBDOOMSDAY_RESOURCE_WAV_H
