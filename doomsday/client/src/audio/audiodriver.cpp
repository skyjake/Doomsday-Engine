/** @file audiodriver.cpp Audio driver loading and interface management. 
 * @ingroup audio
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
#include "de_misc.h"
#include "de_audio.h"
#include "audio/sys_audio.h"

#include <de/Library>
#include <de/LibraryFile>

typedef struct driver_s {
    Library* library;
    audiodriver_t interface;
    audiointerface_sfx_t sfx;
    audiointerface_music_t music;
    audiointerface_cd_t cd;
} driver_t;

static driver_t drivers[AUDIODRIVER_COUNT];

static const char* driverIdentifier[AUDIODRIVER_COUNT] = {
    "dummy",
    "sdlmixer",
    "openal",
    "fmod",
    "fluidsynth",
    "dsound",
    "winmm"
};

// The active/loaded interfaces.
typedef struct audiointerface_s {
    audiointerfacetype_t type;
    union {
        void*                   any;
        audiointerface_sfx_t*   sfx;
        audiointerface_music_t* music;
        audiointerface_cd_t*    cd;
    } i;
    /**
     * @todo The audio interface could also declare which audio formats it is
     * capable of playing (e.g., MIDI only, CD tracks only).
     */
} audiointerface_t;

static audiointerface_t activeInterfaces[MAX_AUDIO_INTERFACES];

#ifdef MACOSX
/// Built-in QuickTime audio interface implemented by MusicPlayer.m
DENG_EXTERN_C audiointerface_music_t audiodQuickTimeMusic;
#endif

static void importInterfaces(driver_t* d)
{
    de::Library& lib = Library_File(d->library).library();

    lib.setSymbolPtr( d->interface.Init,        "DS_Init");
    lib.setSymbolPtr( d->interface.Shutdown,    "DS_Shutdown");
    lib.setSymbolPtr( d->interface.Event,       "DS_Event");
    lib.setSymbolPtr( d->interface.Set,         "DS_Set", de::Library::OptionalSymbol);

    if(lib.hasSymbol("DS_SFX_Init"))
    {
        lib.setSymbolPtr( d->sfx.gen.Init,      "DS_SFX_Init");
        lib.setSymbolPtr( d->sfx.gen.Create,    "DS_SFX_CreateBuffer");
        lib.setSymbolPtr( d->sfx.gen.Destroy,   "DS_SFX_DestroyBuffer");
        lib.setSymbolPtr( d->sfx.gen.Load,      "DS_SFX_Load");
        lib.setSymbolPtr( d->sfx.gen.Reset,     "DS_SFX_Reset");
        lib.setSymbolPtr( d->sfx.gen.Play,      "DS_SFX_Play");
        lib.setSymbolPtr( d->sfx.gen.Stop,      "DS_SFX_Stop");
        lib.setSymbolPtr( d->sfx.gen.Refresh,   "DS_SFX_Refresh");
        lib.setSymbolPtr( d->sfx.gen.Set,       "DS_SFX_Set");
        lib.setSymbolPtr( d->sfx.gen.Setv,      "DS_SFX_Setv");
        lib.setSymbolPtr( d->sfx.gen.Listener,  "DS_SFX_Listener");
        lib.setSymbolPtr( d->sfx.gen.Listenerv, "DS_SFX_Listenerv");
        lib.setSymbolPtr( d->sfx.gen.Getv,      "DS_SFX_Getv", de::Library::OptionalSymbol);
    }

    if(lib.hasSymbol("DM_Music_Init"))
    {
        lib.setSymbolPtr( d->music.gen.Init,    "DM_Music_Init");
        lib.setSymbolPtr( d->music.gen.Update,  "DM_Music_Update");
        lib.setSymbolPtr( d->music.gen.Get,     "DM_Music_Get");
        lib.setSymbolPtr( d->music.gen.Set,     "DM_Music_Set");
        lib.setSymbolPtr( d->music.gen.Pause,   "DM_Music_Pause");
        lib.setSymbolPtr( d->music.gen.Stop,    "DM_Music_Stop");
        lib.setSymbolPtr( d->music.SongBuffer,  "DM_Music_SongBuffer", de::Library::OptionalSymbol);
        lib.setSymbolPtr( d->music.Play,        "DM_Music_Play", de::Library::OptionalSymbol);
        lib.setSymbolPtr( d->music.PlayFile,    "DM_Music_PlayFile", de::Library::OptionalSymbol);
    }

    if(lib.hasSymbol("DM_CDAudio_Init"))
    {
        lib.setSymbolPtr( d->cd.gen.Init,       "DM_CDAudio_Init");
        lib.setSymbolPtr( d->cd.gen.Update,     "DM_CDAudio_Update");
        lib.setSymbolPtr( d->cd.gen.Set,        "DM_CDAudio_Set");
        lib.setSymbolPtr( d->cd.gen.Get,        "DM_CDAudio_Get");
        lib.setSymbolPtr( d->cd.gen.Pause,      "DM_CDAudio_Pause");
        lib.setSymbolPtr( d->cd.gen.Stop,       "DM_CDAudio_Stop");
        lib.setSymbolPtr( d->cd.Play,           "DM_CDAudio_Play");
    }
}

static int audioPluginFinder(void* libFile, const char* fileName, const char* absPath, void* ptr)
{
    de::LibraryFile* lib = reinterpret_cast<de::LibraryFile*>(libFile);
    Str* path = (Str*) ptr;

    DENG2_UNUSED(fileName);

    if(lib->hasUnderscoreName(Str_Text(path)))
    {
        Str_Set(path, absPath);
        return true;
    }

    return false; // Keep looking...
}

static AutoStr* findAudioPluginPath(const char* name)
{
    AutoStr* path = AutoStr_FromText(name);
    if(Library_IterateAvailableLibraries(audioPluginFinder, path))
    {
        // The full path of the library was returned in @a path.
        return path;
    }
    return 0;
}

static boolean loadAudioDriver(driver_t* driver, const char* name)
{
    boolean ok = false;

    if(name && name[0])
    {
        AutoStr* libPath = findAudioPluginPath(name);

        // Load the audio driver library and import symbols.
        if(libPath && (driver->library = Library_New(Str_Text(libPath))) != 0)
        {
            importInterfaces(driver);
            ok = true;
        }
        else
        {
            Con_Message("Warning: loadAudioDriver: Loading of \"%s\" failed.", name);
        }
    }
    return ok;
}

static const char* getDriverName(audiodriverid_t id)
{
    static const char* audioDriverNames[AUDIODRIVER_COUNT] = {
    /* AUDIOD_DUMMY */      "Dummy",
    /* AUDIOD_SDL_MIXER */  "SDLMixer",
    /* AUDIOD_OPENAL */     "OpenAL",
    /* AUDIOD_FMOD */       "FMOD",
    /* AUDIOD_FLUIDSYNTH */ "FluidSynth",
    /* AUDIOD_DSOUND */     "DirectSound", // Win32 only
    /* AUDIOD_WINMM */      "Windows Multimedia" // Win32 only
    };
    if(VALID_AUDIODRIVER_IDENTIFIER(id))
        return audioDriverNames[id];
    Con_Error("S_GetDriverName: Unknown driver id %i.\n", id);
    return 0; // Unreachable.
}

static audiodriverid_t identifierToDriverId(const char* name)
{
    for(int i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        if(!stricmp(name, driverIdentifier[i])) return (audiodriverid_t) i;
    }
    Con_Message("'%s' is not a valid audio driver name.", name);
    return AUDIOD_INVALID;
}

static boolean isDriverInited(audiodriverid_t id)
{
    if(!VALID_AUDIODRIVER_IDENTIFIER(id)) return false;
    return drivers[id].interface.Init != 0;
}

/**
 * Initializes the audio driver interfaces.
 *
 * @return  @c true iff successful.
 */
static boolean initDriver(audiodriverid_t id)
{
    driver_t* d = &drivers[id];

    if(!VALID_AUDIODRIVER_IDENTIFIER(id))
    {
        Con_Error("initDriver: Unknown audio driver id %i.\n", id);
        return false;
    }

    assert(!isDriverInited(id));
    memset(d, 0, sizeof(*d));

    switch(id)
    {
    case AUDIOD_DUMMY: // built-in
        memcpy(&d->interface, &audiod_dummy, sizeof(d->interface));
        memcpy(&d->sfx, &audiod_dummy_sfx, sizeof(d->sfx));
        break;

#ifndef DENG_DISABLE_SDLMIXER
    case AUDIOD_SDL_MIXER: // built-in
        memcpy(&d->interface, &audiod_sdlmixer, sizeof(d->interface));
        memcpy(&d->sfx, &audiod_sdlmixer_sfx, sizeof(d->sfx));
        memcpy(&d->music, &audiod_sdlmixer_music, sizeof(d->music));
        break;
#endif

    case AUDIOD_OPENAL:
        if(!loadAudioDriver(d, "openal"))
            return false;
        break;

    case AUDIOD_FMOD:
        if(!loadAudioDriver(d, "fmod"))
            return false;
        break;

    case AUDIOD_FLUIDSYNTH:
        if(!loadAudioDriver(d, "fluidsynth"))
            return false;
        break;

#ifdef WIN32
    case AUDIOD_DSOUND:
        if(!loadAudioDriver(d, "directsound"))
            return false;
        break;

    case AUDIOD_WINMM:
        if(!loadAudioDriver(d, "winmm"))
            return false;
        break;
#endif
    default:
        Con_Error("initDriver: Unknown audio driver id %i.\n", id);
        return false; // Unreachable.
    }

    // All loaded drivers are automatically initialized so they are ready for use.
    assert(d->interface.Init != 0);
    return d->interface.Init();
}

/**
 * Chooses the default audio driver based on configuration options.
 */
static audiodriverid_t chooseAudioDriver(void)
{
    // No audio output?
    if(isDedicated || CommandLine_Exists("-dummy"))
        return AUDIOD_DUMMY;

    if(CommandLine_Exists("-fmod"))
        return AUDIOD_FMOD;

    if(CommandLine_Exists("-oal") || CommandLine_Exists("-openal"))
        return AUDIOD_OPENAL;

#ifdef WIN32
    // DirectSound with 3D sound support, EAX effects?
    if(CommandLine_Exists("-dsound"))
        return AUDIOD_DSOUND;

    // Windows Multimedia?
    if(CommandLine_Exists("-winmm"))
        return AUDIOD_WINMM;
#endif

#ifndef DENG_DISABLE_SDLMIXER
    if(CommandLine_Exists("-sdlmixer"))
        return AUDIOD_SDL_MIXER;
#endif

    // The default audio driver.
    return AUDIOD_FMOD;
}

static audiodriverid_t initDriverIfNeeded(const char* identifier)
{
    audiodriverid_t drvId = identifierToDriverId(identifier);
    if(!isDriverInited(drvId))
    {
        initDriver(drvId);
    }
    assert(VALID_AUDIODRIVER_IDENTIFIER(drvId));
    return drvId;
}

static void appendInterface(audiointerface_t** pos, audiointerfacetype_t type, void* ptr)
{
    (*pos)->type = type;
    (*pos)->i.any = ptr;
    (*pos)++;
}

/**
 * Choose the SFX, Music, and CD audio interfaces to use.
 *
 * @param defaultDriverId  Default audio driver to use unless overridden.
 */
static void selectInterfaces(audiodriverid_t defaultDriverId)
{
    driver_t* defaultDriver = &drivers[defaultDriverId];
    audiodriverid_t drvId;
    audiointerface_t* pos = activeInterfaces;
    int p;

    // The default driver goes on the bottom of the stack.
    if(defaultDriver->sfx.gen.Init) appendInterface(&pos, AUDIO_ISFX, &defaultDriver->sfx);
    if(defaultDriver->music.gen.Init)
    {
        appendInterface(&pos, AUDIO_IMUSIC, &defaultDriver->music);
    }
#ifdef MACOSX
    else if(defaultDriverId != AUDIOD_DUMMY)
    {
        // On the Mac, use the built-in QuickTime interface as the fallback for music.
        appendInterface(&pos, AUDIO_IMUSIC, &audiodQuickTimeMusic);
    }
#endif

#ifndef WIN32
    // At the moment, dsFMOD supports streaming samples so we can
    // automatically load dsFluidSynth for MIDI music.
    if(defaultDriverId == AUDIOD_FMOD)
    {
        initDriverIfNeeded("fluidsynth");
        if(isDriverInited(AUDIOD_FLUIDSYNTH))
        {
            appendInterface(&pos, AUDIO_IMUSIC, &drivers[AUDIOD_FLUIDSYNTH].music);
        }
    }
#endif

    if(defaultDriver->cd.gen.Init) appendInterface(&pos, AUDIO_ICD, &defaultDriver->cd);

    for(p = 1; p < CommandLine_Count() - 1 && pos < activeInterfaces + MAX_AUDIO_INTERFACES; p++)
    {
        if(!CommandLine_IsOption(p)) continue;

        // Check for SFX override.
        if(CommandLine_IsMatchingAlias("-isfx", CommandLine_At(p)))
        {
            drvId = initDriverIfNeeded(CommandLine_At(++p));
            if(!drivers[drvId].sfx.gen.Init)
            {
                Con_Error("Audio driver '%s' does not provide an SFX interface.\n", getDriverName(drvId));
            }
            appendInterface(&pos, AUDIO_ISFX, &drivers[drvId].sfx);
            continue;
        }

        // Check for Music override.
        if(CommandLine_IsMatchingAlias("-imusic", CommandLine_At(p)))
        {
            drvId = initDriverIfNeeded(CommandLine_At(++p));
            if(!drivers[drvId].music.gen.Init)
            {
                Con_Error("Audio driver '%s' does not provide a Music interface.\n", getDriverName(drvId));
            }
            appendInterface(&pos, AUDIO_IMUSIC, &drivers[drvId].music);
            continue;
        }

        // Check for CD override.
        if(CommandLine_IsMatchingAlias("-icd", CommandLine_At(p)))
        {
            drvId = initDriverIfNeeded(CommandLine_At(++p));
            if(!drivers[drvId].cd.gen.Init)
            {
                Con_Error("Audio driver '%s' does not provide a CD interface.\n", getDriverName(drvId));
            }
            appendInterface(&pos, AUDIO_ICD, &drivers[drvId].cd);
            continue;
        }
    }

    AudioDriver_PrintInterfaces();

    // Let the music driver(s) know of the primary sfx interface, in case they
    // want to play audio through it.
    AudioDriver_Music_Set(AUDIOP_SFX_INTERFACE, AudioDriver_SFX());
}

void AudioDriver_PrintInterfaces(void)
{
    int i;

    Con_Message("Audio configuration (by decreasing priority):");
    for(i = MAX_AUDIO_INTERFACES - 1; i >= 0; --i)
    {
        audiointerface_t* a = &activeInterfaces[i];
        if(a->type == AUDIO_IMUSIC || a->type == AUDIO_ICD)
        {
            Con_Message("  %-5s: %s", a->type == AUDIO_IMUSIC? "Music" : "CD",
                        Str_Text(AudioDriver_InterfaceName(a->i.any)));
        }
        else if(a->type == AUDIO_ISFX)
        {
            Con_Message("  SFX  : %s", Str_Text(AudioDriver_InterfaceName(a->i.sfx)));
        }
    }
}

/*
static boolean initInterface(audiointerface_base_t* interface)
{
    if(!interface) return true;
    if(interface->Init)
    {
        return interface->Init();
    }
    return false;
}
*/

boolean AudioDriver_Init(void)
{
    audiodriverid_t defaultDriverId;
    boolean ok = false;

    memset(activeInterfaces, 0, sizeof(activeInterfaces));

    if(CommandLine_Exists("-nosound")) return false;

    defaultDriverId = chooseAudioDriver();
    ok = initDriver(defaultDriverId);
    if(!ok)
    {
        Con_Message("Warning: Failed initializing audio driver \"%s\"", getDriverName(defaultDriverId));
    }

    // Fallback option for the default driver.
#ifndef DENG_DISABLE_SDLMIXER
    if(!ok)
    {
        defaultDriverId = AUDIOD_SDL_MIXER;
        ok = initDriver(defaultDriverId);
    }
#endif

    if(ok)
    {
        // Choose the interfaces to use.
        selectInterfaces(defaultDriverId);
    }
    return ok;
}

void AudioDriver_Shutdown(void)
{
    int i;

    // Shut down all the loaded drivers. (Note: reverse order)
    for(i = AUDIODRIVER_COUNT - 1; i >= 0; --i)
    {
        driver_t* d = &drivers[i];
        if(d->interface.Shutdown) d->interface.Shutdown();
    }

    // Unload the plugins after everything has been shut down.
    for(i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        driver_t* d = &drivers[i];
        if(d->library)
        {
            Library_Delete(d->library);
        }
        memset(d, 0, sizeof(*d));
    }

    // No more interfaces available.
    memset(activeInterfaces, 0, sizeof(activeInterfaces));
}

audiodriver_t* AudioDriver_Interface(void* anyAudioInterface)
{
    int i;

    for(i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        driver_t* d = &drivers[i];
        if((void*)&d->sfx   == anyAudioInterface ||
           (void*)&d->music == anyAudioInterface ||
           (void*)&d->cd    == anyAudioInterface)
        {
            return &d->interface;
        }
    }
    return 0;
}

int AudioDriver_FindInterfaces(audiointerfacetype_t type, void** listOfInterfaces)
{
    int i, count = 0;

    // Least important interfaces are listed first in the stack.
    for(i = MAX_AUDIO_INTERFACES - 1; i >= 0; --i)
    {
        if(activeInterfaces[i].type == type ||
                (type == AUDIO_IMUSIC_OR_ICD && (activeInterfaces[i].type == AUDIO_IMUSIC ||
                                                 activeInterfaces[i].type == AUDIO_ICD)))
        {
            if(listOfInterfaces)
            {
                *listOfInterfaces++ = activeInterfaces[i].i.any;
            }
            ++count;
        }
    }
    return count;
}

audiointerface_sfx_generic_t* AudioDriver_SFX(void)
{
    void* ifs[MAX_AUDIO_INTERFACES];
    if(!AudioDriver_FindInterfaces(AUDIO_ISFX, ifs)) return 0; // No such interface loaded.

    // The primary interface is the first one returned.
    return (audiointerface_sfx_generic_t*) ifs[0];
}

boolean AudioDriver_Music_Available(void)
{
    return AudioDriver_FindInterfaces(AUDIO_IMUSIC, NULL) > 0;
}

audiointerface_cd_t* AudioDriver_CD(void)
{
    void* ifs[MAX_AUDIO_INTERFACES];
    if(!AudioDriver_FindInterfaces(AUDIO_ICD, ifs)) return 0; // No such interface loaded.

    // The primary interface is the first one returned.
    return (audiointerface_cd_t*) ifs[0];
}

audiointerfacetype_t AudioDriver_InterfaceType(void* anyAudioInterface)
{
    int i;
    for(i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        driver_t* d = &drivers[i];
        if((void*)&d->sfx   == anyAudioInterface) return AUDIO_ISFX;
        if((void*)&d->music == anyAudioInterface) return AUDIO_IMUSIC;
        if((void*)&d->cd    == anyAudioInterface) return AUDIO_ICD;
    }
    return AUDIO_INONE;
}

AutoStr* AudioDriver_InterfaceName(void* anyAudioInterface)
{
    int i;

    for(i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        driver_t* d = &drivers[i];

        if((void*)&d->sfx == anyAudioInterface)
        {
            /// @todo  SFX interfaces can't be named yet.
            return AutoStr_FromText(getDriverName(audiodriverid_t(i)));
        }

        if((void*)&d->music == anyAudioInterface || (void*)&d->cd == anyAudioInterface)
        {
            char buf[256];  /// @todo  This could easily overflow...
            audiointerface_music_generic_t* gen = (audiointerface_music_generic_t*) anyAudioInterface;
            if(gen->Get(MUSIP_ID, buf))
            {
                return AutoStr_FromTextStd(buf);
            }
            else
            {
                return AutoStr_FromText("[MUSIP_ID not defined]");
            }
        }
    }

    return AutoStr_FromText("[invalid audio interface]");
}
