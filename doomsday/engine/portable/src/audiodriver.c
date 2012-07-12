/**
 * @file audiodriver.c
 * Audio driver loading and interface management. @ingroup audio
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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
#include "sys_audio.h"
#include "audiodriver.h"

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

// The active interfaces.
static audiointerface_sfx_t* iSFX;
static audiointerface_music_t* iMusic;
static audiointerface_cd_t* iCD;

#ifdef MACOSX
/// Built-in QuickTime audio interface implemented by MusicPlayer.m
extern audiointerface_music_t audiodQuickTimeMusic;
#endif

static void importInterfaces(driver_t* d)
{
#define Imp(name) Library_Symbol(d->library, name)

    d->interface.Init     = Imp("DS_Init");
    d->interface.Shutdown = Imp("DS_Shutdown");
    d->interface.Event    = Imp("DS_Event");
    d->interface.Set      = Imp("DS_Set");

    if(Imp("DS_SFX_Init"))
    {
        d->sfx.gen.Init      = Imp("DS_SFX_Init");
        d->sfx.gen.Create    = Imp("DS_SFX_CreateBuffer");
        d->sfx.gen.Destroy   = Imp("DS_SFX_DestroyBuffer");
        d->sfx.gen.Load      = Imp("DS_SFX_Load");
        d->sfx.gen.Reset     = Imp("DS_SFX_Reset");
        d->sfx.gen.Play      = Imp("DS_SFX_Play");
        d->sfx.gen.Stop      = Imp("DS_SFX_Stop");
        d->sfx.gen.Refresh   = Imp("DS_SFX_Refresh");
        d->sfx.gen.Set       = Imp("DS_SFX_Set");
        d->sfx.gen.Setv      = Imp("DS_SFX_Setv");
        d->sfx.gen.Listener  = Imp("DS_SFX_Listener");
        d->sfx.gen.Listenerv = Imp("DS_SFX_Listenerv");
        d->sfx.gen.Getv      = Imp("DS_SFX_Getv");
    }

    if(Imp("DM_Music_Init"))
    {
        d->music.gen.Init   = Imp("DM_Music_Init");
        d->music.gen.Update = Imp("DM_Music_Update");
        d->music.gen.Get    = Imp("DM_Music_Get");
        d->music.gen.Set    = Imp("DM_Music_Set");
        d->music.gen.Pause  = Imp("DM_Music_Pause");
        d->music.gen.Stop   = Imp("DM_Music_Stop");
        d->music.SongBuffer = Imp("DM_Music_SongBuffer");
        d->music.Play       = Imp("DM_Music_Play");
        d->music.PlayFile   = Imp("DM_Music_PlayFile");
    }

    if(Imp("DM_CDAudio_Init"))
    {
        d->cd.gen.Init   = Imp("DM_CDAudio_Init");
        d->cd.gen.Update = Imp("DM_CDAudio_Update");
        d->cd.gen.Set    = Imp("DM_CDAudio_Set");
        d->cd.gen.Get    = Imp("DM_CDAudio_Get");
        d->cd.gen.Pause  = Imp("DM_CDAudio_Pause");
        d->cd.gen.Stop   = Imp("DM_CDAudio_Stop");
        d->cd.Play       = Imp("DM_CDAudio_Play");
    }

#undef Imp
}

static boolean loadAudioDriver(driver_t* driver, const char* name)
{
    boolean ok = false;

    if(name && name[0])
    {
        ddstring_t libPath;

        // Compose the name using the prefix "ds".
        Str_Init(&libPath);
#ifdef WIN32
        Str_Appendf(&libPath, "%sds%s.dll", ddBinPath, name);
#elif defined(MACOSX)
        Str_Appendf(&libPath, "ds%s.bundle", name);
#else
        Str_Appendf(&libPath, "libds%s.so", name);
#endif

        // Load the audio driver library and import symbols.
        if((driver->library = Library_New(Str_Text(&libPath))) != 0)
        {
            importInterfaces(driver);
            ok = true;
        }
        else
        {
            Con_Message("Warning: loadAudioDriver: Loading of \"%s\" failed.\n", Str_Text(&libPath));
        }
        Str_Free(&libPath);
    }
    return ok;
}

static const char* getDriverName(audiodriver_e id)
{
    static const char* audioDriverNames[AUDIODRIVER_COUNT] = {
    /* AUDIOD_DUMMY */      "Dummy",
    /* AUDIOD_SDL_MIXER */  "SDLMixer",
    /* AUDIOD_OPENAL */     "OpenAL",
    /* AUDIOD_FMOD */       "FMOD Ex",
    /* AUDIOD_FLUIDSYNTH */ "FluidSynth",
    /* AUDIOD_DSOUND */     "DirectSound", // Win32 only
    /* AUDIOD_WINMM */      "Windows Multimedia" // Win32 only
    };
    if(VALID_AUDIODRIVER_IDENTIFIER(id))
        return audioDriverNames[id];
    Con_Error("S_GetDriverName: Unknown driver id %i.\n", id);
    return 0; // Unreachable.
}

static int identifierToDriverId(const char* name)
{
    int i;
    for(i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        if(!stricmp(name, driverIdentifier[i])) return i;
    }
    return -1;
}

static boolean isDriverInited(audiodriver_e id)
{
    if(!VALID_AUDIODRIVER_IDENTIFIER(id)) return false;
    return drivers[id].interface.Init != 0;
}

/**
 * Initializes the audio driver interfaces.
 *
 * @return  @c true iff successful.
 */
static boolean initDriver(audiodriver_e id)
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

#ifdef MACOSX
    case AUDIOD_OPENAL:
        if(!loadAudioDriver(d, "OpenAL"))
            return false;
        break;

    case AUDIOD_FMOD:
        if(!loadAudioDriver(d, "FMOD"))
            return false;
        break;

    case AUDIOD_FLUIDSYNTH:
        if(!loadAudioDriver(d, "FluidSynth"))
            return false;
        break;
#else
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
#endif

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
static audiodriver_e chooseAudioDriver(void)
{
    // No audio output?
    if(isDedicated || CommandLine_Exists("-dummy"))
        return AUDIOD_DUMMY;

    if(CommandLine_Exists("-fmod"))
        return AUDIOD_FMOD;

    if(CommandLine_Exists("-oal"))
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

static audiodriver_e initDriverIfNeeded(const char* identifier)
{
    audiodriver_e drvId = identifierToDriverId(identifier);
    if(!isDriverInited(drvId))
    {
        initDriver(drvId);
    }
    assert(VALID_AUDIODRIVER_IDENTIFIER(drvId));
    return drvId;
}

/**
 * Choose the SFX, Music, and CD audio interfaces to use.
 *
 * @param defaultDriverId  Default audio driver to use unless overridden.
 */
static void selectInterfaces(audiodriver_e defaultDriverId)
{
    driver_t* defaultDriver = &drivers[defaultDriverId];
    driver_t* musicDriver = 0;
    audiodriver_e drvId;
    int p;

    iSFX = 0;
    iMusic = 0;
    iCD = 0;

    if(defaultDriver->sfx.gen.Init) iSFX = &defaultDriver->sfx;
    if(defaultDriver->music.gen.Init)
    {
        iMusic = &defaultDriver->music;
        musicDriver = defaultDriver;
    }
    if(defaultDriver->cd.gen.Init) iCD = &defaultDriver->cd;

    // Check for SFX override.
    if((p = CommandLine_CheckWith("-isfx", 1)) > 0)
    {
        drvId = initDriverIfNeeded(CommandLine_At(p + 1));
        if(!drivers[drvId].sfx.gen.Init)
        {
            Con_Error("Audio driver '%s' does not provide an SFX interface.\n",
                      getDriverName(drvId));
        }
        iSFX = &drivers[drvId].sfx;
    }

    // Check for Music override.
    if((p = CommandLine_CheckWith("-imusic", 1)) > 0)
    {
        drvId = initDriverIfNeeded(CommandLine_At(p + 1));
        if(!drivers[drvId].music.gen.Init)
        {
            Con_Error("Audio driver '%s' does not provide a Music interface.\n",
                      getDriverName(drvId));
        }
        musicDriver = &drivers[drvId];
        iMusic = &musicDriver->music;
    }

    // Check for Music override.
    if((p = CommandLine_CheckWith("-icd", 1)) > 0)
    {
        drvId = initDriverIfNeeded(CommandLine_At(p + 1));
        if(!drivers[drvId].cd.gen.Init)
        {
            Con_Error("Audio driver '%s' does not provide a CD interface.\n",
                      getDriverName(drvId));
        }
        iCD = &drivers[drvId].cd;
    }

#ifdef MACOSX
    if(!iMusic && defaultDriverId != AUDIOD_DUMMY)
    {
        // On the Mac, use the built-in QuickTime interface as the fallback for music.
        iMusic = &audiodQuickTimeMusic;
    }
#endif   

    if(musicDriver && musicDriver->interface.Set)
    {
        // Let the music driver know of the chosen sfx interface, in case it
        // wants to play audio through it.
        musicDriver->interface.Set(AUDIOP_SFX_INTERFACE, iSFX);
    }
}

static boolean initInterface(audiointerface_base_t* interface)
{
    if(!interface) return true;
    if(interface->Init)
    {
        return interface->Init();
    }
    return false;
}

boolean AudioDriver_Init(void)
{
    audiodriver_e defaultDriverId;
    boolean ok = false;

    if(CommandLine_Exists("-nosound")) return false;

    defaultDriverId = chooseAudioDriver();
    ok = initDriver(defaultDriverId);
    if(!ok)
    {
        Con_Message("Warning: Failed initializing audio driver \"%s\"\n", getDriverName(defaultDriverId));
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

    // Shut down all the loaded drivers.
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
    iSFX = 0;
    iMusic = 0;
    iCD = 0;
}

audiodriver_t* AudioDriver_Interface(void* audioInterface)
{
    int i;

    for(i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        driver_t* d = &drivers[i];
        if((void*)&d->sfx   == audioInterface ||
           (void*)&d->music == audioInterface ||
           (void*)&d->cd    == audioInterface)
        {
            return &d->interface;
        }
    }
    return 0;
}

audiointerface_sfx_generic_t* AudioDriver_SFX(void)
{
    return (audiointerface_sfx_generic_t*) iSFX;
}

audiointerface_music_t* AudioDriver_Music(void)
{
    return iMusic;
}

audiointerface_cd_t* AudioDriver_CD(void)
{
    return iCD;
}
