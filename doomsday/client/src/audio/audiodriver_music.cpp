/** @file audiodriver_music.cpp Low-level music interface of the audio driver. 
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include <de/memory.h>

#include "de_filesys.h"
#include "audio/audiodriver_music.h"

#define BUFFERED_MUSIC_FILE      "dd-buffered-song"

static dd_bool needBufFileSwitch = false;

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

static int musicPlayNativeFile(audiointerface_music_t *iMusic, char const *fileName, dd_bool looped)
{
    if(!iMusic->PlayFile) return 0;
    return iMusic->PlayFile(fileName, looped);
}

static int musicPlayLump(audiointerface_music_t *iMusic, lumpnum_t lumpNum, dd_bool looped)
{
    if(!iMusic->Play || !iMusic->SongBuffer)
    {
        // Music interface does not offer buffer playback.
        // Write this lump to disk and play from there.
        AutoStr *musicFile = AudioDriver_Music_ComposeTempBufferFilename(0);
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
        de::FileHandle &hndl = App_FileSystem().openLump(App_FileSystem().lump(lumpNum));
        size_t const length  = hndl.length();
        hndl.read((uint8_t *) iMusic->SongBuffer(length), length);
        F_Delete(&hndl);

        return iMusic->Play(looped);
    }
    catch(de::LumpIndex::NotFoundError const &)
    {} // Ignore error.

    return 0;
}

static int musicPlayFile(audiointerface_music_t *iMusic, char const *virtualOrNativePath, dd_bool looped)
{
    try
    {
        // Relative paths are relative to the native working directory.
        de::String path      = (de::NativePath::workPath() / de::NativePath(virtualOrNativePath).expand()).withSeparators('/');
        de::FileHandle *file = &App_FileSystem().openFile(path, "rb");
        size_t const len     = file->length();

        if(!iMusic->Play || !iMusic->SongBuffer)
        {
            // Music interface does not offer buffer playback.
            // Write to disk and play from there.
            AutoStr *fileName = AudioDriver_Music_ComposeTempBufferFilename(NULL);
            uint8_t *buf      = (uint8_t *)M_Malloc(len);

            file->read(buf, len);
            F_Dump(buf, len, Str_Text(fileName));
            M_Free(buf); buf = 0;

            F_Delete(file);

            // Music maestro, if you please!
            return musicPlayNativeFile(iMusic, Str_Text(fileName), looped);
        }

        // Music interface offers buffered playback. Use it.
        file->read((uint8_t *) iMusic->SongBuffer(len), len);
        F_Delete(file);

        return iMusic->Play(looped);
    }
    catch(de::FS1::NotFoundError const &)
    {} // Ignore error.
    return 0;
}

static int musicPlayCDTrack(audiointerface_cd_t *iCD, int track, dd_bool looped)
{
    return iCD->Play(track, looped);
}

static dd_bool musicIsPlaying(audiointerface_music_t *iMusic)
{
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
    int i, count = AudioDriver_FindInterfaces(AUDIO_IMUSIC, ifs);
    for(i = 0; i < count; ++i)
    {
        musicSet((audiointerface_music_t *) ifs[i], property, ptr);
    }

    if(property == AUDIOP_SOUNDFONT_FILENAME)
    {
        char const *fn = (char const *) ptr;
        if(!fn || !fn[0]) return; // No path.

        if(F_FileExists(fn))
            LOG_AUDIO_MSG("Current soundfont set to: \"%s\"") << fn;
        else
            LOG_AUDIO_WARNING("Soundfont \"%s\" not found") << fn;
    }
}

int AudioDriver_Music_PlayNativeFile(char const *fileName, dd_bool looped)
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int i, count = AudioDriver_FindInterfaces(AUDIO_IMUSIC, ifs);
    for(i = 0; i < count; ++i)
    {
        if(musicPlayNativeFile((audiointerface_music_t* ) ifs[i], fileName, looped))
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

int AudioDriver_Music_PlayFile(const char* virtualOrNativePath, dd_bool looped)
{
    void* ifs[MAX_AUDIO_INTERFACES];
    int i, count = AudioDriver_FindInterfaces(AUDIO_IMUSIC, ifs);
    for(i = 0; i < count; ++i)
    {
        if(musicPlayFile((audiointerface_music_t*) ifs[i], virtualOrNativePath, looped))
            return true;
    }
    return false;
}

int AudioDriver_Music_PlayCDTrack(int track, dd_bool looped)
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int i, count = AudioDriver_FindInterfaces(AUDIO_ICD, ifs);
    for(i = 0; i < count; ++i)
    {
        if(musicPlayCDTrack((audiointerface_cd_t *) ifs[i], track, looped))
            return true;
    }
    return false;
}

dd_bool AudioDriver_Music_IsPlaying()
{
    void *ifs[MAX_AUDIO_INTERFACES];
    int i, count = AudioDriver_FindInterfaces(AUDIO_IMUSIC_OR_ICD, ifs);
    for(i = 0; i < count; ++i)
    {
        if(musicIsPlaying((audiointerface_music_t *) ifs[i]))
            return true;
    }
    return false;
}
