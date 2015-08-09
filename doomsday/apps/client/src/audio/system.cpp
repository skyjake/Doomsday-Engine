/** @file system.cpp  Audio subsystem module.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au> *
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DENG_NO_API_MACROS_SOUND

#include "audio/system.h"

#ifdef __CLIENT__
#  include "dd_main.h"  // isDedicated
#  include "api_filesys.h"
#endif

#ifdef __SERVER__
#  include "server/sv_sound.h"
#endif

#include "audio/s_cache.h"
#include "audio/s_mus.h"
#include "audio/s_sfx.h"

#include "api_map.h"
#include "world/p_players.h"
#include "world/sector.h"

#include <doomsday/audio/logical.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <de/App>

using namespace de;

dint soundMinDist = 256;  // No distance attenuation this close.
dint soundMaxDist = 2025;

// Setting these variables is enough to adjust the volumes.
// S_StartFrame() will call the actual routines to change the volume
// when there are changes.
dint sfxVolume = 255 * 2/3;
dint musVolume = 255 * 2/3;

dint sfxBits = 8;
dint sfxRate = 11025;

byte sfxOneSoundPerEmitter;  // Traditional Doomsday behavior: allows sounds to overlap.

bool noRndPitch;

#ifdef __CLIENT__
#  ifdef MACOSX
/// Built-in QuickTime audio interface implemented by MusicPlayer.m
DENG_EXTERN_C audiointerface_music_t audiodQuickTimeMusic;
#  endif
#endif

namespace audio {

static audio::System *theAudioSystem = nullptr;

#ifdef __CLIENT__
static char *soundFontPath = (char *) "";

static audiodriverid_t identifierToDriverId(String name)
{
    static String const driverIdentifier[AUDIODRIVER_COUNT] = {
        "dummy",
        "sdlmixer",
        "openal",
        "fmod",
        "fluidsynth",
        "dsound",
        "winmm"
    };

    name = name.toLower();

    for(dint i = 0; i < AUDIODRIVER_COUNT; ++i)
    {
        if(name == driverIdentifier[i]) return audiodriverid_t( i );
    }

    LOG_AUDIO_ERROR("'%s' is not a valid audio driver name") << name;
    return AUDIOD_INVALID;
}
#endif

DENG2_PIMPL(System)
, DENG2_OBSERVES(App, GameUnload)
{
#ifdef __CLIENT__
    AudioDriver drivers[AUDIODRIVER_COUNT];

    AudioDriver &driverById(audiodriverid_t id)
    {
        DENG2_ASSERT(VALID_AUDIODRIVER_IDENTIFIER(id));
        return drivers[id];
    }

    /**
     * Chooses the default audio driver based on configuration options.
     */
    audiodriverid_t chooseAudioDriver()
    {
        CommandLine &cmdLine = App::commandLine();

        // No audio output?
        if(::isDedicated)
            return AUDIOD_DUMMY;
    
        if(cmdLine.has("-dummy"))
            return AUDIOD_DUMMY;

        if(cmdLine.has("-fmod"))
            return AUDIOD_FMOD;

        if(cmdLine.has("-oal") || cmdLine.has("-openal"))
            return AUDIOD_OPENAL;

#ifdef WIN32
        if(cmdLine.has("-dsound"))
            return AUDIOD_DSOUND;

        if(cmdLine.has("-winmm"))
            return AUDIOD_WINMM;
#endif

#ifndef DENG_DISABLE_SDLMIXER
        if(cmdLine.has("-sdlmixer"))
            return AUDIOD_SDL_MIXER;
#endif

        // The default audio driver.
        return AUDIOD_FMOD;
    }

    /**
     * Initializes the audio driver interfaces.
     *
     * @return  @c true iff successful.
     */
    bool initDriver(audiodriverid_t id)
    {
        try
        {
            AudioDriver &driver = driverById(id);

            switch(id)
            {
            case AUDIOD_DUMMY:      driver.load("dummy");       break;
#ifndef DENG_DISABLE_SDLMIXER
            case AUDIOD_SDL_MIXER:  driver.load("sdlmixer");    break;
#endif
            case AUDIOD_OPENAL:     driver.load("openal");      break;
            case AUDIOD_FMOD:       driver.load("fmod");        break;
            case AUDIOD_FLUIDSYNTH: driver.load("fluidsynth");  break;
#ifdef WIN32
            case AUDIOD_DSOUND:     driver.load("directsound"); break;
            case AUDIOD_WINMM:      driver.load("winmm");       break;
#endif

            default: return false;
            }

            // All loaded drivers are automatically initialized so they are ready for use.
            driver.initialize();
            return driver.isInitialized();
        }
        catch(AudioDriver::LoadError const &er)
        {
            LOG_AUDIO_WARNING("") << er.asText();
        }
        return false;
    }

    audiodriverid_t initDriverIfNeeded(String const &identifier)
    {
        audiodriverid_t id  = identifierToDriverId(identifier);
        AudioDriver &driver = driverById(id);
        if(!driver.isInitialized())
        {
            initDriver(id);
        }
        return id;
    }

    bool loadDrivers()
    {
        activeInterfaces.clear();

        if(App::commandLine().has("-nosound"))
            return false;

        audiodriverid_t defaultDriverId = chooseAudioDriver();

        bool ok = initDriver(defaultDriverId);
        if(!ok)
        {
            LOG_AUDIO_WARNING("Failed initializing audio driver \"%s\"")
                << AudioDriver_GetName(defaultDriverId);
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

    void unloadDrivers()
    {
        // Deinitialize all loaded drivers. (Note: reverse order)
        for(dint i = AUDIODRIVER_COUNT; i--> 0; )
        {
            drivers[i].deinitialize();
        }

        // Unload the plugins after everything has been shut down.
        for(AudioDriver &driver : drivers)
        {
            driver.unload();
        }

        // No more interfaces available.
        activeInterfaces.clear();
    }

    /**
     * The active/loaded interfaces.
     *
     * @todo The audio interface could also declare which audio formats it is capable
     * of playing (e.g., MIDI only, CD tracks only).
     */
    struct AudioInterface
    {
        audiointerfacetype_t type;
        union {
            void                   *any;
            audiointerface_sfx_t   *sfx;
            audiointerface_music_t *music;
            audiointerface_cd_t    *cd;
        } i;
    };
    QList<AudioInterface> activeInterfaces;

    /**
     * Choose the SFX, Music, and CD audio interfaces to use.
     *
     * @param defaultDriverId  Default audio driver to use unless overridden.
     */
    void selectInterfaces(audiodriverid_t defaultDriverId)
    {
        AudioDriver &defaultDriver = driverById(defaultDriverId);

        // The default driver goes on the bottom of the stack.
        if(defaultDriver.hasSfx())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_ISFX;
            ifs.i.any = &defaultDriver.iSfx();
            activeInterfaces << ifs;  // a copy is made
        }

        if(defaultDriver.hasMusic())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_IMUSIC;
            ifs.i.any = &defaultDriver.iMusic();
            activeInterfaces << ifs;  // a copy is made
        }
#ifdef MACOSX
        else if(defaultDriverId != AUDIOD_DUMMY)
        {
            // On the Mac, use the built-in QuickTime interface as the fallback for music.
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_IMUSIC;
            ifs.i.any = &::audiodQuickTimeMusic;
            activeInterfaces << ifs;  // a copy is made
        }
#endif

#ifndef WIN32
        // At the moment, dsFMOD supports streaming samples so we can
        // automatically load dsFluidSynth for MIDI music.
        if(defaultDriverId == AUDIOD_FMOD)
        {
            initDriverIfNeeded("fluidsynth");
            AudioDriver &fluidSynth = driverById(AUDIOD_FLUIDSYNTH);
            if(fluidSynth.isInitialized())
            {
                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_IMUSIC;
                ifs.i.any = &fluidSynth.iMusic();
                activeInterfaces << ifs;  // a copy is made
            }
        }
#endif

        if(defaultDriver.hasCd())
        {
            AudioInterface ifs; zap(ifs);
            ifs.type  = AUDIO_ICD;
            ifs.i.any = &defaultDriver.iCd();
            activeInterfaces << ifs;  // a copy is made
        }

        CommandLine &cmdLine = App::commandLine();
        for(dint p = 1; p < cmdLine.count() - 1; ++p)
        {
            if(!cmdLine.isOption(p)) continue;

            // Check for SFX override.
            if(cmdLine.matches("-isfx", cmdLine.at(p)))
            {
                AudioDriver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasSfx())
                    throw Error("selectInterfaces", "Audio driver '" + driver.name() + "' does not provide an SFX interface");

                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_ISFX;
                ifs.i.any = &driver.iSfx();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }

            // Check for Music override.
            if(cmdLine.matches("-imusic", cmdLine.at(p)))
            {
                AudioDriver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasMusic())
                    throw Error("selectInterfaces", "Audio driver '" + driver.name() + "' does not provide a Music interface");

                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_IMUSIC;
                ifs.i.any = &driver.iMusic();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }

            // Check for CD override.
            if(cmdLine.matches("-icd", cmdLine.at(p)))
            {
                AudioDriver &driver = driverById(initDriverIfNeeded(cmdLine.at(++p)));
                if(!driver.hasCd())
                    throw Error("selectInterfaces", "Audio driver '" + driver.name() + "' does not provide a CD interface");

                AudioInterface ifs; zap(ifs);
                ifs.type  = AUDIO_ICD;
                ifs.i.any = &driver.iCd();
                activeInterfaces << ifs;  // a copy is made
                continue;
            }
        }

        self.printAllInterfaces();

        // Let the music driver(s) know of the primary sfx interface, in case they
        // want to play audio through it.
        setMusicProperty(AUDIOP_SFX_INTERFACE, self.sfx());
    }
#endif  // __CLIENT__

    Instance(Public *i) : Base(i)
    {
        theAudioSystem = thisPublic;

        App::app().audienceForGameUnload() += this;
    }
    ~Instance()
    {
        App::app().audienceForGameUnload() -= this;

        theAudioSystem = nullptr;
    }

#ifdef __CLIENT__
    void setMusicProperty(dint prop, void const *ptr)
    {
        self.forAllInterfaces(AUDIO_IMUSIC, [this, &prop, &ptr] (void *ifs)
        {
            auto *iMusic = (audiointerface_music_t *) ifs;
            if(audiodriver_t *base = self.interface(iMusic))
            {
                if(base->Set) base->Set(prop, ptr);
            }
            return LoopContinue;
        });

        if(prop == AUDIOP_SOUNDFONT_FILENAME)
        {
            auto *fn = (char const *) ptr;
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
#endif

    void aboutToUnloadGame(game::Game const &)
    {
        self.reset();
    }
};

System::System() : d(new Instance(this))
{}

void System::timeChanged(Clock const &)
{
    // Nothing to do.
}

audio::System &System::get()
{
    DENG2_ASSERT(theAudioSystem);
    return *theAudioSystem;
}

void System::reset()
{
#ifdef __CLIENT__
    Sfx_Reset();
#endif
    _api_S.StopMusic();
}

/**
 * @todo Do this in audio::System::timeChanged()
 */
void System::startFrame()
{
#ifdef __CLIENT__
    static dint oldMusVolume = -1;

    if(::musVolume != oldMusVolume)
    {
        oldMusVolume = ::musVolume;
        Mus_SetVolume(::musVolume / 255.0f);
    }

    // Update all channels (freq, 2D:pan,volume, 3D:position,velocity).
    Sfx_StartFrame();
    Mus_StartFrame();
#endif

    // Remove stopped sounds from the LSM.
    Sfx_Logical_SetOneSoundPerEmitter(::sfxOneSoundPerEmitter);
    Sfx_PurgeLogical();
}

void System::endFrame()
{
#ifdef __CLIENT__
    Sfx_EndFrame();
#endif
}

bool System::initPlayback()
{
    Sfx_Logical_SetSampleLengthCallback(Sfx_GetSoundLength);

    CommandLine &cmdLine = App::commandLine();
    if(cmdLine.has("-nosound") || cmdLine.has("-noaudio"))
        return true;

    // Disable random pitch changes?
    ::noRndPitch = cmdLine.has("-norndpitch");

#ifdef __CLIENT__
    LOG_AUDIO_VERBOSE("Initializing Audio System for playback...");

    // Try to load the audio driver plugin(s).
    if(!d->loadDrivers())
    {
        LOG_AUDIO_NOTE("Music and sound effects are disabled");
        return false;
    }

    bool sfxOK = Sfx_Init();
    bool musOK = Mus_Init();

    if(!sfxOK || !musOK)
    {
        LOG_AUDIO_NOTE("Errors during audio subsystem initialization");
        return false;
    }
#endif

    return true;
}

void System::deinitPlayback()
{
#ifdef __CLIENT__
    Sfx_Shutdown();
    Mus_Shutdown();

    d->unloadDrivers();
#endif
}

#ifdef __CLIENT__
void System::updateSoundFont()
{
    NativePath path(soundFontPath);

#ifdef MACOSX
    // On OS X we can try to use the basic DLS soundfont that's part of CoreAudio.
    if(path.isEmpty())
    {
        path = "/System/Library/Components/CoreAudio.component/Contents/Resources/gs_instruments.dls";
    }
#endif

    d->setMusicProperty(AUDIOP_SOUNDFONT_FILENAME, path.expand().toString().toLatin1().constData());
}

bool System::musicIsAvailable() const
{
    // The primary interface is the first one.
    return forAllInterfaces(AUDIO_IMUSIC, [] (void *)
    {
        return LoopAbort;
    });
}

audiointerface_sfx_generic_t *System::sfx() const
{
    // The primary interface is the first one.
    audiointerface_sfx_generic_t *found = nullptr;
    forAllInterfaces(AUDIO_ISFX, [&found] (void *ifs)
    {
        found = (audiointerface_sfx_generic_t *)ifs;
        return LoopAbort;
    });
    return found;
}

audiointerface_cd_t *System::cd() const
{
    // The primary interface is the first one.
    audiointerface_cd_t *found = nullptr;
    forAllInterfaces(AUDIO_ICD, [&found] (void *ifs)
    {
        found = (audiointerface_cd_t *)ifs;
        return LoopAbort;
    });
    return found;
}

String System::interfaceDescription() const
{
    String str;
    QTextStream os(&str);

    os << _E(b) "Audio configuration:\n" _E(.);

    for(dint i = d->activeInterfaces.count(); i--> 0; )
    {
        Instance::AudioInterface const &ifs = d->activeInterfaces[i];

        if(ifs.type == AUDIO_IMUSIC || ifs.type == AUDIO_ICD)
        {
            os << _E(Ta) _E(l) "  " << (ifs.type == AUDIO_IMUSIC ? "Music" : "CD") << ": "
               << _E(.) _E(Tb) << interfaceName(ifs.i.any) << "\n";
        }
        else if(ifs.type == AUDIO_ISFX)
        {
            os << _E(Ta) _E(l) << "  SFX: " << _E(.) _E(Tb)
               << interfaceName(ifs.i.sfx) << "\n";
        }
    }
    return str.rightStrip();
}

void System::printAllInterfaces() const
{
    LOG_AUDIO_MSG("%s") << interfaceDescription();
}

LoopResult System::forAllInterfaces(audiointerfacetype_t type, std::function<LoopResult (void *)> func) const
{
    if(type != AUDIO_INONE)
    {
        for(dint i = d->activeInterfaces.count(); i--> 0; )
        {
            Instance::AudioInterface const &ifs = d->activeInterfaces[i];

            if(ifs.type == type ||
               (type == AUDIO_IMUSIC_OR_ICD && (ifs.type == AUDIO_IMUSIC ||
                                                ifs.type == AUDIO_ICD)))
            {
                if(auto result = func(ifs.i.any))
                    return result;
            }
        }
    }
    return LoopContinue;
}

audiodriver_t *System::interface(void *anyAudioInterface) const
{
    if(anyAudioInterface)
    {
        for(AudioDriver const &driver : d->drivers)
        {
            if((void *)&driver.iSfx()   == anyAudioInterface ||
               (void *)&driver.iMusic() == anyAudioInterface ||
               (void *)&driver.iCd()    == anyAudioInterface)
            {
                return &driver.iBase();
            }
        }
    }
    return nullptr;
}

audiointerfacetype_t System::interfaceType(void *anyAudioInterface) const
{
    if(anyAudioInterface)
    {
        for(AudioDriver const &driver : d->drivers)
        {
            if((void *)&driver.iSfx()   == anyAudioInterface) return AUDIO_ISFX;
            if((void *)&driver.iMusic() == anyAudioInterface) return AUDIO_IMUSIC;
            if((void *)&driver.iCd()    == anyAudioInterface) return AUDIO_ICD;
        }
    }
    return AUDIO_INONE;
}

String System::interfaceName(void *anyAudioInterface) const
{
    if(anyAudioInterface)
    {
        for(AudioDriver const &driver : d->drivers)
        {
            String const name = driver.interfaceName(anyAudioInterface);
            if(!name.isEmpty()) return name;
        }
    }
    return "(invalid)";
}

audiodriverid_t System::toDriverId(AudioDriver const *driver) const
{
    if(driver && driver >= &d->drivers[0] && driver <= &d->drivers[AUDIODRIVER_COUNT])
    {
        return audiodriverid_t( driver - d->drivers );
    }
    return AUDIOD_INVALID;
}
#endif

void System::aboutToUnloadMap()
{
    // Stop everything in the LSM.    
    Sfx_InitLogical();

#ifdef __CLIENT__
    Sfx_MapChange();
#endif
}

void System::worldMapChanged()
{
#ifdef __CLIENT__
    // Update who is listening now.
    Sfx_SetListener(S_GetListenerMobj());
#endif
}

/**
 * Console command for playing a (local) sound effect.
 */
D_CMD(PlaySound)
{
    DENG2_UNUSED(src);

    if(argc < 2)
    {
        LOG_SCR_NOTE("Usage: %s (id) (volume) at (x) (y) (z)") << argv[0];
        LOG_SCR_MSG("(volume) must be in 0..1, but may be omitted");
        LOG_SCR_MSG("'at (x) (y) (z)' may also be omitted");
        LOG_SCR_MSG("The sound is always played locally");
        return true;
    }
    dint p = 0;

    // The sound ID is always first.
    dint const id = ::defs.getSoundNum(argv[1]);

    // The second argument may be a volume.
    dfloat volume = 1;
    if(argc >= 3 && String(argv[2]).compareWithoutCase("at"))
    {
        volume = String(argv[2]).toFloat();
        p = 3;
    }
    else
    {
        p = 2;
    }

    bool useFixedPos = false;
    coord_t fixedPos[3];
    if(argc >= p + 4 && !String(argv[p]).compareWithoutCase("at"))
    {
        useFixedPos = true;
        fixedPos[0] = String(argv[p + 1]).toDouble();
        fixedPos[1] = String(argv[p + 2]).toDouble();
        fixedPos[2] = String(argv[p + 3]).toDouble();
    }

    // Check that the volume is valid.
    volume = de::clamp(0.f, volume, 1.f);
    if(de::fequal(volume, 0)) return true;

    if(useFixedPos)
    {
        _api_S.LocalSoundAtVolumeFrom(id, nullptr, fixedPos, volume);
    }
    else
    {
        _api_S.LocalSoundAtVolume(id, nullptr, volume);
    }

    return true;
}

#ifdef __CLIENT__
static void reverbVolumeChanged()
{
    Sfx_UpdateReverb();
}

static void soundFontChanged()
{
    App_AudioSystem().updateSoundFont();
}
#endif

void System::consoleRegister()  // static
{
    C_VAR_BYTE  ("sound-overlap-stop",  &sfxOneSoundPerEmitter, 0, 0, 1);

#ifdef __CLIENT__
    // Sound effects:
    C_VAR_INT     ("sound-volume",        &sfxVolume,             0, 0, 255);
    C_VAR_INT     ("sound-rate",          &sfxSampleRate,         0, 11025, 44100);
    C_VAR_INT     ("sound-16bit",         &sfx16Bit,              0, 0, 1);
    C_VAR_INT     ("sound-3d",            &sfx3D,                 0, 0, 1);
    C_VAR_FLOAT2  ("sound-reverb-volume", &sfxReverbStrength,     0, 0, 1.5f, reverbVolumeChanged);

    C_CMD_FLAGS("playsound", nullptr, PlaySound, CMDF_NO_DEDICATED);

    C_VAR_INT("sound-info", &showSoundInfo, 0, 0, 1);

    // Music:
    C_VAR_CHARPTR2("music-soundfont",     &soundFontPath,         0, 0, 0, soundFontChanged);

    Mus_ConsoleRegister();
#endif
}

}  // namespace audio

mobj_t *S_GetListenerMobj()
{
    return DD_Player(::displayPlayer)->publicData().mo;
}

sfxinfo_t *S_GetSoundInfo(dint soundID, dfloat *freq, dfloat *volume)
{
    if(soundID <= 0 || soundID >= ::defs.sounds.size())
        return nullptr;

    dfloat dummy = 0;
    if(!freq)   freq   = &dummy;
    if(!volume) volume = &dummy;

    // Traverse all links when getting the definition. (But only up to 10, which is
    // certainly enough and prevents endless recursion.) Update the sound id at the
    // same time. The links were checked in Def_Read() so there cannot be any bogus
    // ones.
    sfxinfo_t *info = &::runtimeDefs.sounds[soundID];

    for(dint i = 0; info->link && i < 10;
        info     = info->link,
        *freq    = (info->linkPitch > 0    ? info->linkPitch  / 128.0f : *freq),
        *volume += (info->linkVolume != -1 ? info->linkVolume / 127.0f : 0),
        soundID  = ::runtimeDefs.sounds.indexOf(info),
       ++i)
    {}

    DENG2_ASSERT(soundID < ::defs.sounds.size());

    return info;
}

dd_bool S_IsRepeating(dint idFlags)
{
    if(idFlags & DDSF_REPEAT)
        return true;

    if(sfxinfo_t *info = S_GetSoundInfo(idFlags & ~DDSF_FLAG_MASK, nullptr, nullptr))
    {
        return (info->flags & SF_REPEAT) != 0;
    }
    return false;
}

dint S_LocalSoundAtVolumeFrom(dint soundIdAndFlags, mobj_t *origin, coord_t *point, dfloat volume)
{
#ifdef __CLIENT__
    // A dedicated server never starts any local sounds (only logical sounds in the LSM).
    if(::isDedicated || DoomsdayApp::app().busyMode().isActive())
        return false;

    dint soundId = (soundIdAndFlags & ~DDSF_FLAG_MASK);
    if(soundId <= 0 || soundId >= ::defs.sounds.size())
        return false;

    if(::sfxVolume <= 0 || volume <= 0)
        return false; // This won't play...

    LOG_AS("S_LocalSoundAtVolumeFrom");

    if(volume > 1)
    {
        LOGDEV_AUDIO_WARNING("Volume is too high (%f > 1)") << volume;
    }

    dfloat freq = 1;
    // This is the sound we're going to play.
    sfxinfo_t *info = S_GetSoundInfo(soundId, &freq, &volume);
    if(!info) return false;  // Hmm? This ID is not defined.

    bool const isRepeating = S_IsRepeating(soundIdAndFlags);

    // Check the distance (if applicable).
    if(!(info->flags & SF_NO_ATTENUATION) && !(soundIdAndFlags & DDSF_NO_ATTENUATION))
    {
        // If origin is too far, don't even think about playing the sound.
        coord_t *fixPoint = (origin ? origin->origin : point);

        if(Mobj_ApproxPointDistance(S_GetListenerMobj(), fixPoint) > soundMaxDist)
            return false;
    }

    // Load the sample.
    sfxsample_t *sample = Sfx_Cache(soundId);
    if(!sample)
    {
        if(sfxAvail)
        {
            LOG_AUDIO_VERBOSE("S_LocalSoundAtVolumeFrom: Caching of sound %i failed")
                    << soundId;
        }
        return false;
    }

    // Random frequency alteration? (Multipliers chosen to match original
    // sound code.)
    if(!::noRndPitch)
    {
        if(info->flags & SF_RANDOM_SHIFT)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (7.0f / 255);
        }
        if(info->flags & SF_RANDOM_SHIFT2)
        {
            freq += (RNG_RandFloat() - RNG_RandFloat()) * (15.0f / 255);
        }
    }

    // If the sound has an exclusion group, either all or the same emitter's
    // iterations of this sound will stop.
    if(info->group)
    {
        mobj_t *emitter = ((info->flags & SF_GLOBAL_EXCLUDE) ? nullptr : origin);
        Sfx_StopSoundGroup(info->group, emitter);
    }

    // Let's play it.
    dint flags = 0;
    flags |= (((info->flags & SF_NO_ATTENUATION) || (soundIdAndFlags & DDSF_NO_ATTENUATION)) ? SF_NO_ATTENUATION : 0);
    flags |= (isRepeating ? SF_REPEAT : 0);
    flags |= ((info->flags & SF_DONT_STOP) ? SF_DONT_STOP : 0);
    return Sfx_StartSound(sample, volume, freq, origin, point, flags);

#else
    DENG2_UNUSED4(soundIdAndFlags, origin, point, volume);
    return false;
#endif
}

dint S_LocalSoundAtVolume(dint soundID, mobj_t *origin, dfloat volume)
{
    return S_LocalSoundAtVolumeFrom(soundID, origin, nullptr, volume);
}

dint S_LocalSound(dint soundID, mobj_t *origin)
{
    // Play local sound at max volume.
    return S_LocalSoundAtVolumeFrom(soundID, origin, nullptr, 1);
}

dint S_LocalSoundFrom(dint soundID, coord_t *fixedPos)
{
    return S_LocalSoundAtVolumeFrom(soundID, nullptr, fixedPos, 1);
}

dint S_StartSound(dint soundID, mobj_t *origin)
{
#ifdef __SERVER__
    // The sound is audible to everybody.
    Sv_Sound(soundID, origin, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

dint S_StartSoundEx(dint soundID, mobj_t *origin)
{
#ifdef __SERVER__
    Sv_Sound(soundID, origin, SVSF_TO_ALL | SVSF_EXCLUDE_ORIGIN);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    return S_LocalSound(soundID, origin);
}

dint S_StartSoundAtVolume(dint soundID, mobj_t *origin, dfloat volume)
{
#ifdef __SERVER__
    Sv_SoundAtVolume(soundID, origin, volume, SVSF_TO_ALL);
#endif
    Sfx_StartLogical(soundID, origin, S_IsRepeating(soundID));

    // The sound is audible to everybody.
    return S_LocalSoundAtVolume(soundID, origin, volume);
}

dint S_ConsoleSound(dint soundID, mobj_t *origin, dint targetConsole)
{
#ifdef __SERVER__
    Sv_Sound(soundID, origin, targetConsole);
#endif

    // If it's for us, we can hear it.
    if(targetConsole == consolePlayer)
    {
        S_LocalSound(soundID, origin);
    }

    return true;
}

/**
 * @param sectorEmitter  Sector in which to stop sounds.
 * @param soundID        Unique identifier of the sound to be stopped.
 *                       If @c 0, ID not checked.
 * @param flags          @ref soundStopFlags
 */
static void stopSectorSounds(ddmobj_base_t *sectorEmitter, dint soundID, dint flags)
{
    if(!sectorEmitter || !flags) return;

    // Are we stopping with this sector's emitter?
    if(flags & SSF_SECTOR)
    {
        _api_S.StopSound(soundID, (mobj_t *)sectorEmitter);
    }

    // Are we stopping with linked emitters?
    if(!(flags & SSF_SECTOR_LINKED_SURFACES)) return;

    // Process the rest of the emitter chain.
    ddmobj_base_t *base = sectorEmitter;
    while((base = (ddmobj_base_t *)base->thinker.next))
    {
        // Stop sounds from this emitter.
        _api_S.StopSound(soundID, (mobj_t *)base);
    }
}

void S_StopSound(dint soundID, mobj_t *emitter)
{
#ifdef __CLIENT__
    // No special stop behavior.
    // Sfx provides a routine for this.
    Sfx_StopSound(soundID, emitter);
#endif

    // Notify the LSM.
    if(Sfx_StopLogical(soundID, emitter))
    {
#ifdef __SERVER__
        // In netgames, the server is responsible for telling clients
        // when to stop sounds. The LSM will tell us if a sound was
        // stopped somewhere in the world.
        Sv_StopSound(soundID, emitter);
#endif
    }
}

void S_StopSound2(dint soundID, mobj_t *emitter, dint flags)
{
    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        if(emitter->thinker.id)
        {
            // Emitter is a real Mobj.
            stopSectorSounds(&Mobj_Sector(emitter)->soundEmitter(), soundID, flags);
            return;
        }

        // The head of the chain is the sector. Find it.
        while(emitter->thinker.prev)
        {
            emitter = (mobj_t *)emitter->thinker.prev;
        }
        stopSectorSounds((ddmobj_base_t *)emitter, soundID, flags);
        return;
    }

    // A regular stop.
    S_StopSound(soundID, emitter);
}

dint S_IsPlaying(dint soundID, mobj_t *emitter)
{
    // The Logical Sound Manager (under Sfx) provides a routine for this.
    return Sfx_IsPlaying(soundID, emitter);
}

dint S_StartMusicNum(dint id, dd_bool looped)
{
#ifdef __CLIENT__

    // Don't play music if the volume is at zero.
    if(::isDedicated) return true;

    if(id < 0 || id >= ::defs.musics.size()) return false;

    Record const &def = ::defs.musics[id];
    LOG_AUDIO_MSG("Starting music '%s'") << def.gets("id");

    return Mus_Start(def, looped);

#else
    DENG2_UNUSED2(id, looped);
    return false;
#endif
}

dint S_StartMusic(char const *musicID, dd_bool looped)
{
    LOG_AS("S_StartMusic");
    dint idx = ::defs.getMusicNum(musicID);
    if(idx < 0)
    {
        if(musicID && !String(musicID).isEmpty())
        {
            LOG_AUDIO_WARNING("Song \"%s\" not defined, cannot start playback") << musicID;
        }
        return false;
    }
    return S_StartMusicNum(idx, looped);
}

void S_StopMusic()
{
#ifdef __CLIENT__
    Mus_Stop();
#endif
}

void S_PauseMusic(dd_bool paused)
{
#ifdef __CLIENT__
    Mus_Pause(paused);
#else
    DENG2_UNUSED(paused);
#endif
}

DENG_DECLARE_API(S) =
{
    { DE_API_SOUND },
    //S_MapChange,
    S_LocalSoundAtVolumeFrom,
    S_LocalSoundAtVolume,
    S_LocalSound,
    S_LocalSoundFrom,
    S_StartSound,
    S_StartSoundEx,
    S_StartSoundAtVolume,
    S_ConsoleSound,
    S_StopSound,
    S_StopSound2,
    S_IsPlaying,
    S_StartMusic,
    S_StartMusicNum,
    S_StopMusic,
    S_PauseMusic
};
