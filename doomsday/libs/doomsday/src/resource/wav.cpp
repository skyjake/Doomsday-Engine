/** @file wav.cpp WAV loader. @ingroup audio
 *
 * @todo This is obsolete code! Use de::Waveform instead.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/wav.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"

#include <de/logbuffer.h>
#include <de/nativepath.h>
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include "dd_share.h"

#ifdef __GNUC__
/*
 * Something in here is confusing GCC: when compiled with -O2, WAV_MemoryLoad()
 * reads corrupt data. It is likely that the optimizer gets the manipulation of
 * the 'data' pointer wrong.
 */
#  if GCC_VERSION >= 40400
void* WAV_MemoryLoad(const byte* data, size_t datalength, int* bits, int* rate,
                     int* samples) __attribute__(( optimize(0) ));
#  endif
#endif

#define WAVE_FORMAT_PCM     1

#pragma pack(1)
typedef struct riff_hdr_s {
    char            id[4];              ///< Identifier string = "RIFF"
    uint32_t        len;                ///< Remaining length after this header
} riff_hdr_t;

typedef struct chunk_hdr_s {            ///< CHUNK 8-byte header
    char            id[4];              ///< Identifier, e.g. "fmt " or "data"
    uint32_t        len;                ///< Remaining chunk length after header
} chunk_hdr_t;                          ///< data bytes follow chunk header

typedef struct wav_format_s {
    uint16_t        wFormatTag;         ///< Format category
    uint16_t        wChannels;          ///< Number of channels
    uint32_t        dwSamplesPerSec;    ///< Sampling rate
    uint32_t        dwAvgBytesPerSec;   ///< For buffer estimation
    uint16_t        wBlockAlign;        ///< Data block size
    uint16_t        wBitsPerSample;     ///< Sample size
} wav_format_t;
#pragma pack()

#define WReadAndAdvance(pByte, pDest, length) \
    { memcpy(pDest, pByte, length); pByte += length; }

int WAV_CheckFormat(const char* data)
{
    return !strncmp(data, "RIFF", 4) && !strncmp(data + 8, "WAVE", 4);
}

void* WAV_MemoryLoad(const byte* data, size_t datalength, int* bits, int* rate, int* samples)
{
    const byte* end = data + datalength;
    byte* sampledata = NULL;
    chunk_hdr_t riff_chunk;
    wav_format_t wave_format;

    LOG_AS("WAV_MemoryLoad");

    if (!WAV_CheckFormat((const char*)data))
    {
        LOG_RES_WARNING("Not WAV format data");
        return NULL;
    }

    // Read the RIFF header.
    data += sizeof(riff_hdr_t);
    data += 4; // "WAVE" already verified above

#ifdef _DEBUG
    assert(sizeof(wave_format) == 16);
    assert(sizeof(riff_chunk) == 8);
#endif

    // Initialize the format info.
    memset(&wave_format, 0, sizeof(wave_format));
    wave_format.wBlockAlign = 1;

    // Start readin' the chunks, baby!
    while (data < end)
    {
        // Read next chunk header.
        WReadAndAdvance(data, &riff_chunk, sizeof(riff_chunk));

        // Correct endianness.
        riff_chunk.len = DD_ULONG(riff_chunk.len);

        // What have we got here?
        if (!strncmp(riff_chunk.id, "fmt ", 4))
        {
            // Read format chunk.
            WReadAndAdvance(data, &wave_format, sizeof(wave_format));

            // Correct endianness.
            wave_format.wFormatTag       = DD_USHORT(wave_format.wFormatTag      );
            wave_format.wChannels        = DD_USHORT(wave_format.wChannels       );
            wave_format.dwSamplesPerSec  = DD_ULONG (wave_format.dwSamplesPerSec );
            wave_format.dwAvgBytesPerSec = DD_ULONG (wave_format.dwAvgBytesPerSec);
            wave_format.wBlockAlign      = DD_USHORT(wave_format.wBlockAlign     );
            wave_format.wBitsPerSample   = DD_USHORT(wave_format.wBitsPerSample  );

            assert(wave_format.wFormatTag == WAVE_FORMAT_PCM); // linear PCM

            // Check that it's a format we know how to read.
            if (wave_format.wFormatTag != WAVE_FORMAT_PCM)
            {
                LOG_RES_WARNING("Unsupported format (%i)") << wave_format.wFormatTag;
                return NULL;
            }
            if (wave_format.wChannels != 1)
            {
                LOG_RES_WARNING("Too many channels (only mono supported)");
                return NULL;
            }
            // Read the extra format information.
            //WReadAndAdvance(&data, &wave_format2, sizeof(*wave_format2));
            /*if (wave_format->wBitsPerSample == 0)
               {
               // We'll have to guess...
               *bits = 8*wave_format->dwAvgBytesPerSec/
               wave_format->dwSamplesPerSec;
               }
               else
               { */
            if (wave_format.wBitsPerSample != 8 &&
               wave_format.wBitsPerSample != 16)
            {
                LOG_RES_WARNING("Must have 8 or 16 bits per sample");
                return NULL;
            }
            // Now we know some information about the sample.
            *bits = wave_format.wBitsPerSample;
            *rate = wave_format.dwSamplesPerSec;
        }
        else if (!strncmp(riff_chunk.id, "data", 4))
        {
            if (!wave_format.wFormatTag)
            {
                LOG_RES_WARNING("Malformed WAV data");
                return NULL;
            }
            // Read data chunk.
            *samples = riff_chunk.len / wave_format.wBlockAlign;
            // Allocate the sample buffer.
            sampledata = (byte *) Z_Malloc(riff_chunk.len, PU_APPSTATIC, 0);
            memcpy(sampledata, data, riff_chunk.len);
#ifdef __BIG_ENDIAN__
            // Correct endianness.
            /*if (wave_format->wBitsPerSample == 16)
            {
                ushort* sample = sampledata;
                for (; sample < ((short*)sampledata) + *samples; ++sample)
                    *sample = DD_USHORT(*sample);
            }*/
#endif
            // We're satisfied with this! Let's get out of here.
            break;
        }
        else
        {
            // Unknown chunk, just skip it.
            data += riff_chunk.len;
        }
    }

    return sampledata;
}

void *WAV_Load(const char *filename, int *bits, int *rate, int *samples)
{
    try
    {
        // Relative paths are relative to the native working directory.
        de::String path = (de::NativePath::workPath() / de::NativePath(filename).expand()).withSeparators('/');
        std::unique_ptr<res::FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));

        // Read in the whole thing.
        size_t size = hndl->length();

        LOG_AS("WAV_Load");
        LOGDEV_RES_XVERBOSE("Loading from \"%s\" (size %i, fpos %i)",
                               de::NativePath(hndl->file().composePath()).pretty()
                            << size
                            << hndl->tell());

        uint8_t *data = (uint8_t *) M_Malloc(size);

        hndl->read(data, size);
        App_FileSystem().releaseFile(hndl->file());

        // Parse the RIFF data.
        void *sampledata = WAV_MemoryLoad((const byte *) data, size, bits, rate, samples);
        if (!sampledata)
        {
            LOG_RES_WARNING("Failed to load \"%s\"") << filename;
        }

        M_Free(data);
        return sampledata;
    }
    catch (const res::FS1::NotFoundError &)
    {} // Ignore.
    return 0;
}
