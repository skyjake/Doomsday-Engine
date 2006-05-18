/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * s_wav.c: WAV Files
 *
 *  A 'bare necessities' WAV loader.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"

#include "m_misc.h"

// MACROS ------------------------------------------------------------------

#define WAVE_FORMAT_PCM     1

// TYPES -------------------------------------------------------------------

typedef unsigned short WORD;

typedef struct riff_hdr_s {
    char    id[4];              // identifier string = "RIFF"
    uint    len;                // remaining length after this header
} riff_hdr_t;

typedef struct chunk_hdr_s {    // CHUNK 8-byte header
    char    id[4];              // identifier, e.g. "fmt " or "data"
    uint    len;                // remaining chunk length after header
} chunk_hdr_t;                  // data bytes follow chunk header

typedef struct wav_format_s {
    WORD    wFormatTag;         // Format category
    WORD    wChannels;          // Number of channels
    uint    dwSamplesPerSec;    // Sampling rate
    uint    dwAvgBytesPerSec;   // For buffer estimation
    WORD    wBlockAlign;        // Data block size
    WORD    wBitsPerSample;     // Sample size
} wav_format_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void WRead(void **ptr, void **dest, int length)
{
    *dest = *ptr;
    *(char**) ptr += length;
}

/*
 * The returned buffer contains the wave data. If NULL is returned, the
 * loading obviously failed. The caller must free the sample data using
 * Z_Free when it's no longer needed. The WAV file must have only one
 * channel! All parameters must be passed, no NULLs are allowed.
 */
void   *WAV_MemoryLoad(byte *data, int datalength, int *bits, int *rate,
                       int *samples)
{
    byte   *end = data + datalength;
    byte   *sampledata = NULL;
    riff_hdr_t *riff_header;
    chunk_hdr_t *riff_chunk;
    wav_format_t *wave_format = NULL;

    // Read the RIFF header.
    WRead((void **) &data, (void **) &riff_header, sizeof(*riff_header));
    if(strncmp(riff_header->id, "RIFF", 4))
    {
        Con_Message("WAV_MemoryLoad: Not RIFF data.\n");
        return NULL;
    }

    // Check that it's really a WAVE file.
    if(strncmp(data, "WAVE", 4))
    {
        Con_Message("WAV_MemoryLoad: Not WAVE data.\n");
        return NULL;
    }
    data += 4;

    // Start readin' the chunks, baby!
    while(data < end)
    {
        // Read next chunk header.
        WRead((void **) &data, (void **) &riff_chunk, sizeof(*riff_chunk));

        // What have we got here?
        if(!strncmp(riff_chunk->id, "fmt ", 4))
        {
            // Read format chunk.
            WRead((void **) &data, (void **) &wave_format,
                  sizeof(*wave_format));
            // Check that it's a format we know how to read.
            if(wave_format->wFormatTag != WAVE_FORMAT_PCM)
            {
                Con_Message("WAV_MemoryLoad: Unsupported format.\n");
                return NULL;
            }
            if(wave_format->wChannels != 1)
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
            if(wave_format->wBitsPerSample != 8 &&
               wave_format->wBitsPerSample != 16)
            {
                Con_Message("WAV_MemoryLoad: Not a 8/16 bit WAVE.\n");
                return NULL;
            }
            // Now we know some information about the sample.
            *bits = wave_format->wBitsPerSample;
            *rate = wave_format->dwSamplesPerSec;
        }
        else if(!strncmp(riff_chunk->id, "data", 4))
        {
            if(!wave_format)
            {
                Con_Message("WAV_MemoryLoad: Malformed WAVE data.\n");
                return NULL;
            }
            // Read data chunk.
            *samples = riff_chunk->len / wave_format->wBlockAlign;
            // Allocate the sample buffer.
            sampledata = Z_Malloc(riff_chunk->len, PU_STATIC, 0);
            memcpy(sampledata, data, riff_chunk->len);
            // We're satisfied with this! Let's get out of here.
            break;
        }
        else
        {
            // Unknown chunk, just skip it.
            data += riff_chunk->len;
        }
    }
    return sampledata;
}

void   *WAV_Load(const char *filename, int *bits, int *rate, int *samples)
{
    DFILE  *file;
    byte   *data;
    int     size;
    void   *sampledata;

    // Try to open the file.
    if((file = F_Open(filename, "b")) == NULL)
        return NULL;

    // Read in the whole thing.
    F_Seek(file, 0, SEEK_END);
    size = F_Tell(file);
    F_Rewind(file);
    data = M_Malloc(size);
    F_Read(data, size, file);
    F_Close(file);

    // Parse the RIFF data.
    sampledata = WAV_MemoryLoad(data, size, bits, rate, samples);
    if(!sampledata)
    {
        Con_Message("WAV_Load: Failed to load %s.\n", filename);
    }
    M_Free(data);
    return sampledata;
}

/*
 * @return: int         (true) if the "RIFF" and "WAVE" strings are found.
 */
int WAV_CheckFormat(char *data)
{
    return !strncmp(data, "RIFF", 4) && !strncmp(data + 8, "WAVE", 4);
}
