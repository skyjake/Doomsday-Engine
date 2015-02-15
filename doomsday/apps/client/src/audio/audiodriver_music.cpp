/** @file audiodriver_music.cpp  Low-level music interface of the audio driver.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "audio/audiodriver_music.h"

#include <de/memory.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/filesys/fs_util.h>
#include "dd_main.h"

using namespace de;

#define BUFFERED_MUSIC_FILE      "dd-buffered-song"

static bool needBufFileSwitch;

static AutoStr *composeBufferedMusicFilename(int id, char const *ext)
{
    if(ext && ext[0])
    {
        return Str_Appendf(AutoStr_NewStd(), "%s%i%s", BUFFERED_MUSIC_FILE, id, ext);
    }
    return Str_Appendf(AutoStr_NewStd(), "%s%i", BUFFERED_MUSIC_FILE, id);
}

static void musicSet(audiointerface_music_t *iMusic, int property, void const *ptr)
{
    audiodriver_t *d = AudioDriver_Interface(iMusic);
    if(!d || !d->Set) return;
    d->Set(property, ptr);
}

static int musicPlayNativeFile(audiointerface_music_t *iMusic, char const *fileName, bool looped)
{
    DENG2_ASSERT(iMusic);
    if(!iMusic->PlayFile) return 0;
    return iMusic->PlayFile(fileName, looped);
}

static int musicPlayLump(audiointerface_music_t *iMusic, lumpnum_t lumpNum, bool looped)
{
    DENG2_ASSERT(iMusic);

    if(!iMusic->Play || !iMusic->SongBuffer)
    {
        // Music interface does not offer buffer playback.
        // Write this lump to disk and play from there.
        AutoStr *musicFile = AudioDriver_Music_ComposeTempBufferFilename(nullptr);
        if(!F_DumpFile(App_FileSystem().lump(lumpNum), Str_Text(musicFile)))
        {
            // Failed to write the lump...
            return 0;
        }
        return musicPlayNativeFile(iMusic, Str_Text(musicFile), looped);
    }

    // Buffer the data using the driver's facilities.
    try
    {
        std::unique_ptr<FileHandle> hndl(&App_FileSystem().openLump(App_FileSystem().lump(lumpNum)));
        size_t const length  = hndl->length();
        hndl->read((uint8_t *) iMusic->SongBuffer(length), length);

        App_FileSystem().releaseFile(hndl->file());

        return iMusic->Play(looped);
    }
    catch(LumpIndex::NotFoundError const &)
    {} // Ignore error.

    return 0;
}

static int musicPlayFile(audiointerface_music_t *iMusic, char const *virtualOrNativePath, bool looped)
{
    DENG2_ASSERT(iMusic);
    try
    {
        // Relative paths are relative to the native working directory.
        String path  = (NativePath::workPath() / NativePath(virtualOrNativePath).expand()).withSeparators('/');
        std::unique_ptr<FileHandle> hndl(&App_FileSystem().openFile(path, "rb"));
        size_t const len = hndl->length();

        if(!iMusic->Play || !iMusic->SongBuffer)
        {
            // Music interface does not offer buffer playback.
            // Write to disk and play from there.
            AutoStr *fileName = AudioDriver_Music_ComposeTempBufferFilename(nullptr);
            uint8_t *buf      = (uint8_t *)M_Malloc(len);

            hndl->read(buf, len);
            F_Dump(buf, len, Str_Text(fileName));
            M_Free(buf); buf = nullptr;

            App_FileSystem().releaseFile(hndl->file());

            // Music maestro, if you please!
            return musicPlayNativeFile(iMusic, Str_Text(fileName), looped);
        }

        // Music interface offers buffered playback. Use it.
        hndl->read((uint8_t *) iMusic->SongBuffer(len), len);
        App_FileSystem().releaseFile(hndl->file());

        return iMusic->Play(looped);
    }
    catch(FS1::NotFoundError const &)
    {} // Ignore error.
    return 0;
}

static int musicPlayCDTrack(audiointerface_cd_t *iCD, int track, bool looped)
{
    DENG2_ASSERT(iCD);
    return iCD->Play(track, looped);
}

static dd_bool musicIsPlaying(audiointerface_music_t *iMusic)
{
    DENG2_ASSERT(iMusic);
    return iMusic->gen.Get(MUSIP_PLAYING, 0);
}

void AudioDriver_Music_SwitchBufferFilenames()
{
    needBufFileSwitch = true;
}

AutoStr *AudioDriver_Music_ComposeTempBufferFilename(char const *ext)
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

void AudioDriver_Music_Set(int property, void const *ptr)
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int const count = AudioDriver_FindInterfaces(AUDIO_IMUSIC, ifs);
    for(int i = 0; i < count; ++i)
    {
        musicSet((audiointerface_music_t *) ifs[i], property, ptr);
    }

    if(property == AUDIOP_SOUNDFONT_FILENAME)
    {
        char const *fn = (char const *) ptr;
        if(!fn || !fn[0]) return; // No path.

        if(F_FileExists(fn))
        {
            LOG_AUDIO_MSG("Current soundfont set to: \"%s\"") << fn;
        }
        else
        {
            LOG_AUDIO_WARNING("Soundfont \"%s\" not found") << fn;
        }
    }
}

int AudioDriver_Music_PlayNativeFile(char const *fileName, dd_bool looped)
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int const count = AudioDriver_FindInterfaces(AUDIO_IMUSIC, ifs);
    for(int i = 0; i < count; ++i)
    {
        if(musicPlayNativeFile((audiointerface_music_t *) ifs[i], fileName, looped))
            return true;
    }
    return false;
}

int AudioDriver_Music_PlayLump(lumpnum_t lump, dd_bool looped)
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int i, count = AudioDriver_FindInterfaces(AUDIO_IMUSIC, ifs);
    for(i = 0; i < count; ++i)
    {
        if(musicPlayLump((audiointerface_music_t *) ifs[i], lump, looped))
            return true;
    }
    return false;
}

int AudioDriver_Music_PlayFile(char const *virtualOrNativePath, dd_bool looped)
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int const count = AudioDriver_FindInterfaces(AUDIO_IMUSIC, ifs);
    for(int i = 0; i < count; ++i)
    {
        if(musicPlayFile((audiointerface_music_t *) ifs[i], virtualOrNativePath, looped))
            return true;
    }
    return false;
}

int AudioDriver_Music_PlayCDTrack(int track, dd_bool looped)
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int const count = AudioDriver_FindInterfaces(AUDIO_ICD, ifs);
    for(int i = 0; i < count; ++i)
    {
        if(musicPlayCDTrack((audiointerface_cd_t *) ifs[i], track, looped))
            return true;
    }
    return false;
}

dd_bool AudioDriver_Music_IsPlaying()
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int const count = AudioDriver_FindInterfaces(AUDIO_IMUSIC_OR_ICD, ifs);
    for(int i = 0; i < count; ++i)
    {
        if(musicIsPlaying((audiointerface_music_t *) ifs[i]))
            return true;
    }
    return false;
}
