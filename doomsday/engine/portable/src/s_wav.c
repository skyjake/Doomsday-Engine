/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * s_wav.c: WAV Files
 *
 * A 'bare necessities' WAV loader.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"

#include "m_misc.h"

// MACROS ------------------------------------------------------------------

#ifdef __GNUC__
/*
 * Something in here is confusing GCC: when compiled with -O2, WAV_MemoryLoad()
 * reads corrupt data. It is likely that the optimizer gets the manipulation of
 * the 'data' pointer wrong.
 */
void* WAV_MemoryLoad(const byte* data, size_t datalength, int* bits, int* rate, 
                     int* samples) __attribute__(( optimize(0) ));
#endif

#define WAVE_FORMAT_PCM     1

// TYPES -------------------------------------------------------------------

#pragma pack(1)
typedef struct riff_hdr_s {
    char            id[4]; // Identifier string = "RIFF"
    uint32_t        len; // Remaining length after this header
} riff_hdr_t;

typedef struct chunk_hdr_s { // CHUNK 8-byte header
    char            id[4]; // Identifier, e.g. "fmt " or "data"
    uint32_t        len; // Remaining chunk length after header
} chunk_hdr_t;           // data bytes follow chunk header

typedef struct wav_format_s {
    uint16_t        wFormatTag; // Format category
    uint16_t        wChannels; // Number of channels
    uint32_t        dwSamplesPerSec; // Sampling rate
    uint32_t        dwAvgBytesPerSec; // For buffer estimation
    uint16_t        wBlockAlign; // Data block size
    uint16_t        wBitsPerSample; // Sample size
} wav_format_t;
#pragma pack()

// CODE --------------------------------------------------------------------

#define WRead(ptr, dest, length) \
    { memcpy(dest, *ptr, length); *(char**) ptr += length; }

/**
 * @return              Non-zero if the "RIFF" and "WAVE" strings are found.
 */
int WAV_CheckFormat(const char* data)
{
    return !strncmp(data, "RIFF", 4) && !strncmp(data + 8, "WAVE", 4);
}

/**
 * The returned buffer contains the wave data. If NULL is returned, the
 * loading obviously failed. The caller must free the sample data using
 * Z_Free when it's no longer needed. The WAV file must have only one
 * channel! All parameters must be passed, no NULLs are allowed.
 */
void* WAV_MemoryLoad(const byte* data, size_t datalength, int* bits,
    int* rate, int* samples)
{
    const byte* end = data + datalength;
    byte* sampledata = NULL;
    chunk_hdr_t riff_chunk;
    wav_format_t wave_format;

    if(!WAV_CheckFormat((const char*)data))
    {
        Con_Message("WAV_MemoryLoad: Not a WAV file.\n");
        return NULL;
    }
    
    memset(&wave_format, 0, sizeof(wave_format));

    // Read the RIFF header.
    data += sizeof(riff_hdr_t);

    // Correct endianness.
    //riff_header->len = ULONG(riff_header->len);

    data += 4;

#ifdef _DEBUG    
    assert(sizeof(wave_format) == 16);
    assert(sizeof(riff_chunk) == 8);
#endif
    
    // Start readin' the chunks, baby!
    while(data < end)
    {
        // Read next chunk header.
        WRead((void **) &data, &riff_chunk, sizeof(riff_chunk));
        
        // Correct endianness.
        riff_chunk.len = ULONG(riff_chunk.len);

        // What have we got here?
        if(!strncmp(riff_chunk.id, "fmt ", 4))
        {
            assert(wave_format.wFormatTag == 0);

            // Read format chunk.
            WRead((void **) &data, &wave_format, sizeof(wave_format));
                                                            
            // Correct endianness.
            wave_format.wFormatTag = USHORT(wave_format.wFormatTag);
            wave_format.wChannels = USHORT(wave_format.wChannels);
            wave_format.dwSamplesPerSec = ULONG(wave_format.dwSamplesPerSec);
            wave_format.dwAvgBytesPerSec = ULONG(wave_format.dwAvgBytesPerSec);
            wave_format.wBlockAlign = USHORT(wave_format.wBlockAlign);
            wave_format.wBitsPerSample = USHORT(wave_format.wBitsPerSample);

            assert(wave_format.wFormatTag == WAVE_FORMAT_PCM); // linear PCM

            // Check that it's a format we know how to read.
            if(wave_format.wFormatTag != WAVE_FORMAT_PCM)
            {
                Con_Message("WAV_MemoryLoad: Unsupported format (%i).\n", wave_format.wFormatTag);
                return NULL;
            }
            if(wave_format.wChannels != 1)
            {
                Con_Message
                    ("WAV_MemoryLoad: Too many channels (only mono supported).\n");
                return NULL;
            }
            // Read the extra format information.
            //WRead(&data, &wave_format2, sizeof(*wave_format2));
            /*if(wave_format->wBitsPerSample == 0)
               {
               // We'll have to guess...
               *bits = 8*wave_format->dwAvgBytesPerSec/
               wave_format->dwSamplesPerSec;
               }
               else
               { */
            if(wave_format.wBitsPerSample != 8 &&
               wave_format.wBitsPerSample != 16)
            {
                Con_Message("WAV_MemoryLoad: Not a 8/16 bit WAVE.\n");
                return NULL;
            }
            // Now we know some information about the sample.
            *bits = wave_format.wBitsPerSample;
            *rate = wave_format.dwSamplesPerSec;
        }
        else if(!strncmp(riff_chunk.id, "data", 4))
        {
            if(!wave_format.wFormatTag)
            {
                Con_Message("WAV_MemoryLoad: Malformed WAVE data.\n");
                return NULL;
            }
            // Read data chunk.
            *samples = riff_chunk.len / wave_format.wBlockAlign;
            // Allocate the sample buffer.
            sampledata = Z_Malloc(riff_chunk.len, PU_APPSTATIC, 0);
            memcpy(sampledata, data, riff_chunk.len);
#ifdef __BIG_ENDIAN__
            // Correct endianness.
            /*if(wave_format->wBitsPerSample == 16)
            {
                ushort* sample = sampledata;
                for(; sample < ((short*)sampledata) + *samples; ++sample)
                    *sample = USHORT(*sample);
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

void* WAV_Load(const char* filename, int* bits, int* rate, int* samples)
{
    DFile* file = F_Open(filename, "b");
    void* sampledata;
    uint8_t* data;
    size_t size;

    if(!file) return NULL;

    // Read in the whole thing.
    size = DFile_Length(file);
 
    DEBUG_Message(("WAV_Load: Loading from %s (size %i, fpos %i)\n", Str_Text(AbstractFile_Path(DFile_File_Const(file))),
                   (int)size, (int)DFile_Tell(file)));

    data = (uint8_t*)malloc(size);
    if(!data) Con_Error("WAV_Load: Failed on allocation of %lu bytes for sample load buffer.",
                (unsigned long) size);

    DFile_Read(file, data, size);
    F_Delete(file);
    file = 0;

    // Parse the RIFF data.
    sampledata = WAV_MemoryLoad((const byte*) data, size, bits, rate, samples);
    if(!sampledata)
    {
        Con_Message("WAV_Load: Failed to load %s.\n", filename);
    }

    free(data);
    return sampledata;
}
