/**
 * @file audiodriver_music.c
 * Low-level music interface of the audio driver. @ingroup audio
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "audiodriver_music.h"

#define BUFFERED_MUSIC_FILE      "dd-buffered-song"

static boolean needBufFileSwitch = false;

void AudioDriver_Music_SwitchBufferFilenames(void)
{
    needBufFileSwitch = true;
}

static AutoStr* composeBufferedMusicFilename(int id, const char* ext)
{
    if(ext && ext[0])
    {
        return Str_Appendf(AutoStr_New(), "%s%i%s", BUFFERED_MUSIC_FILE, id, ext);
    }
    else
    {
        return Str_Appendf(AutoStr_New(), "%s%i", BUFFERED_MUSIC_FILE, id);
    }
}

AutoStr* AudioDriver_Music_ComposeTempBufferFilename(const char* ext)
{
    static int currentBufFile = 0;

    // Switch the name of the buffered song file?
    if(needBufFileSwitch)
    {
        currentBufFile ^= 1;
        needBufFileSwitch = false;
    }

    return composeBufferedMusicFilename(currentBufFile, ext);
}

int AudioDriver_Music_PlayNativeFile(const char* fileName, boolean looped)
{
    if(!AudioDriver_Music() || !AudioDriver_Music()->PlayFile)
        return 0;

    return AudioDriver_Music()->PlayFile(fileName, looped);
}

int AudioDriver_Music_PlayLump(lumpnum_t lump, boolean looped)
{
    abstractfile_t* fsObject;
    size_t lumpLength;
    int lumpIdx;

    if(!AudioDriver_Music()) return 0;

    fsObject = F_FindFileForLumpNum2(lump, &lumpIdx);
    lumpLength = F_LumpLength(lump);

    if(!AudioDriver_Music()->Play || !AudioDriver_Music()->SongBuffer)
    {
        // Music interface does not offer buffer playback.
        // Write this lump to disk and play from there.
        AutoStr* musicFile = AudioDriver_Music_ComposeTempBufferFilename(0);
        if(!F_DumpLump(lump, Str_Text(musicFile)))
        {
            // Failed to write the lump...
            return 0;
        }
        return AudioDriver_Music_PlayNativeFile(Str_Text(musicFile), looped);
    }

    // Buffer the data using the driver's facilities.
    F_ReadLumpSection(fsObject, lumpIdx,
                      (uint8_t*) AudioDriver_Music()->SongBuffer(lumpLength),
                      0, lumpLength);

    return AudioDriver_Music()->Play(looped);
}

int AudioDriver_Music_PlayFile(const char* virtualOrNativePath, boolean looped)
{
    size_t len;
    DFile* file = F_Open(virtualOrNativePath, "rb");

    if(!file) return 0;

    len = DFile_Length(file);

    if(!AudioDriver_Music()->Play || !AudioDriver_Music()->SongBuffer)
    {
        // Music interface does not offer buffer playback.
        // Write to disk and play from there.
        AutoStr* fileName = AudioDriver_Music_ComposeTempBufferFilename(NULL);
        uint8_t* buf = (uint8_t*)malloc(len);
        if(!buf)
        {
            F_Delete(file);
            Con_Message("Warning: Failed on allocation of %lu bytes for temporary song write buffer.\n", (unsigned long) len);
            return false;
        }
        DFile_Read(file, buf, len);
        F_Dump(buf, len, Str_Text(fileName));
        free(buf);

        F_Delete(file);

        // Music maestro, if you please!
        return AudioDriver_Music_PlayNativeFile(Str_Text(fileName), looped);
    }
    else
    {
        // Music interface offers buffered playback. Use it.
        DFile_Read(file, (uint8_t*) AudioDriver_Music()->SongBuffer(len), len);

        F_Delete(file);

        return AudioDriver_Music()->Play(looped);
    }
}
