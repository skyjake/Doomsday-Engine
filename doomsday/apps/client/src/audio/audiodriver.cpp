/** @file audiodriver.cpp  Audio driver loading and interface management.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#include "audio/audiodriver.h"

#include "dd_main.h"
#include "audio/sys_audiod_dummy.h"
#ifndef DE_DISABLE_SDLMIXER
#  include "audio/sys_audiod_sdlmixer.h"
#endif

#include <de/Library>
#include <de/LibraryFile>
#include <de/NativeFile>

using namespace de;

DE_PIMPL(AudioDriver)
{
    bool initialized   = false;
    ::Library *library = nullptr;

    audiodriver_t          iBase;
    audiointerface_sfx_t   iSfx;
    audiointerface_music_t iMusic;
    audiointerface_cd_t    iCd;

    Impl(Public *i) : Base(i)
    {
        zap(iBase);
        zap(iSfx);
        zap(iMusic);
        zap(iCd);
    }

    static LibraryFile *tryFindAudioPlugin(String const &name)
    {
        if (!name.isEmpty())
        {
            LibraryFile *found = nullptr;
            Library_ForAll([&name, &found] (LibraryFile &lib)
            {
                if (lib.hasUnderscoreName(name))
                {
                    found = &lib;
                    return LoopAbort;
                }
                return LoopContinue;
            });
            return found;
        }
        return nullptr;
    }

    void getDummyInterfaces()
    {
        DE_ASSERT(!initialized);

        library = nullptr;
        std::memcpy(&iBase,  &audiod_dummy,       sizeof(iBase));
        std::memcpy(&iSfx,   &audiod_dummy_sfx,   sizeof(iSfx));
        std::memcpy(&iMusic, &audiod_dummy_music, sizeof(iMusic));
        std::memcpy(&iCd,    &audiod_dummy_cd,    sizeof(iCd));
    }

#ifndef DE_DISABLE_SDLMIXER
    void getSdlMixerInterfaces()
    {
        DE_ASSERT(!initialized);

        library = nullptr;
        std::memcpy(&iBase,  &audiod_sdlmixer,       sizeof(iBase));
        std::memcpy(&iSfx,   &audiod_sdlmixer_sfx,   sizeof(iSfx));
        std::memcpy(&iMusic, &audiod_sdlmixer_music, sizeof(iMusic));
        std::memcpy(&iCd,    &audiod_dummy_cd,       sizeof(iCd));
    }
#endif

    void importInterfaces(LibraryFile &libFile)
    {
        DE_ASSERT(!initialized);

        zap(iBase);
        zap(iSfx);
        zap(iMusic);
        zap(iCd);

        library = Library_New(libFile.path().toUtf8().constData());
        if(!library)
        {
            throw LoadError("AudioDriver::importInterfaces",
                            "Failed to load " + libFile.description());
        }

        de::Library &lib = libFile.library();

        lib.setSymbolPtr( iBase.Init,        "DS_Init");
        lib.setSymbolPtr( iBase.Shutdown,    "DS_Shutdown");
        lib.setSymbolPtr( iBase.Event,       "DS_Event");
        lib.setSymbolPtr( iBase.Set,         "DS_Set", de::Library::OptionalSymbol);

        if(lib.hasSymbol("DS_SFX_Init"))
        {
            lib.setSymbolPtr( iSfx.gen.Init,      "DS_SFX_Init");
            lib.setSymbolPtr( iSfx.gen.Create,    "DS_SFX_CreateBuffer");
            lib.setSymbolPtr( iSfx.gen.Destroy,   "DS_SFX_DestroyBuffer");
            lib.setSymbolPtr( iSfx.gen.Load,      "DS_SFX_Load");
            lib.setSymbolPtr( iSfx.gen.Reset,     "DS_SFX_Reset");
            lib.setSymbolPtr( iSfx.gen.Play,      "DS_SFX_Play");
            lib.setSymbolPtr( iSfx.gen.Stop,      "DS_SFX_Stop");
            lib.setSymbolPtr( iSfx.gen.Refresh,   "DS_SFX_Refresh");
            lib.setSymbolPtr( iSfx.gen.Set,       "DS_SFX_Set");
            lib.setSymbolPtr( iSfx.gen.Setv,      "DS_SFX_Setv");
            lib.setSymbolPtr( iSfx.gen.Listener,  "DS_SFX_Listener");
            lib.setSymbolPtr( iSfx.gen.Listenerv, "DS_SFX_Listenerv");
            lib.setSymbolPtr( iSfx.gen.Getv,      "DS_SFX_Getv", de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_Music_Init"))
        {
            lib.setSymbolPtr( iMusic.gen.Init,    "DM_Music_Init");
            lib.setSymbolPtr( iMusic.gen.Update,  "DM_Music_Update");
            lib.setSymbolPtr( iMusic.gen.Get,     "DM_Music_Get");
            lib.setSymbolPtr( iMusic.gen.Set,     "DM_Music_Set");
            lib.setSymbolPtr( iMusic.gen.Pause,   "DM_Music_Pause");
            lib.setSymbolPtr( iMusic.gen.Stop,    "DM_Music_Stop");
            lib.setSymbolPtr( iMusic.SongBuffer,  "DM_Music_SongBuffer", de::Library::OptionalSymbol);
            lib.setSymbolPtr( iMusic.Play,        "DM_Music_Play",       de::Library::OptionalSymbol);
            lib.setSymbolPtr( iMusic.PlayFile,    "DM_Music_PlayFile",   de::Library::OptionalSymbol);
        }

        if(lib.hasSymbol("DM_CDAudio_Init"))
        {
            lib.setSymbolPtr( iCd.gen.Init,       "DM_CDAudio_Init");
            lib.setSymbolPtr( iCd.gen.Update,     "DM_CDAudio_Update");
            lib.setSymbolPtr( iCd.gen.Set,        "DM_CDAudio_Set");
            lib.setSymbolPtr( iCd.gen.Get,        "DM_CDAudio_Get");
            lib.setSymbolPtr( iCd.gen.Pause,      "DM_CDAudio_Pause");
            lib.setSymbolPtr( iCd.gen.Stop,       "DM_CDAudio_Stop");
            lib.setSymbolPtr( iCd.Play,           "DM_CDAudio_Play");
        }
    }
};

AudioDriver::AudioDriver() : d(new Impl(this))
{}

String AudioDriver::name() const
{
    if(!isLoaded()) return "(invalid)";
    return AudioDriver_GetName(App_AudioSystem().toDriverId(this));
}

AudioDriver::Status AudioDriver::status() const
{
    if(d->initialized) return Initialized;
    if(d->iBase.Init != nullptr) return Loaded;
    return Invalid;
}

String AudioDriver::statusAsText() const
{
    switch(status())
    {
    case Invalid:     return "Invalid";
    case Loaded:      return "Loaded";
    case Initialized: return "Initialized";

    default:
        DE_ASSERT_FAIL("AudioDriver::statusAsText: Unknown status");
        return "Unknown";
    }
}

void AudioDriver::load(String const &identifier)
{
    LOG_AS("AudioDriver");

    if(isLoaded())
    {
        /// @throw LoadError  Attempted to load on top of an already loaded driver.
        throw LoadError("AudioDriver::load", "Already initialized. Cannot load '" + identifier + "'");
    }

    // Perhaps a built-in audio driver?
    if(!identifier.compareWithoutCase("dummy"))
    {
        d->getDummyInterfaces();
        return;
    }
#ifndef DE_DISABLE_SDLMIXER
    if(!identifier.compareWithoutCase("sdlmixer"))
    {
        d->getSdlMixerInterfaces();
        return;
    }
#endif

    // Perhaps a plugin audio driver?
    LibraryFile *plugin = Impl::tryFindAudioPlugin(identifier);
    if(!plugin)
    {
        /// @throw LoadError  Unknown driver specified.
        throw LoadError("AudioDriver::load", "Unknown driver \"" + identifier + "\"");
    }

    try
    {
        // Exchange entrypoints.
        d->importInterfaces(*plugin);
    }
    catch(de::Library::SymbolMissingError const &er)
    {
        /// @throw LoadError  One or more missing symbol.
        throw LoadError("AudioDriver::load", "Failed exchanging entrypoints:\n" + er.asText());
    }
}

void AudioDriver::unload()
{
    LOG_AS("AudioDriver");

    if(isInitialized())
    {
        /// @throw LoadError  Cannot unload while initialized.
        throw LoadError("AudioDriver::unload", "'" + name() + "' is still initialized, cannot unload");
    }

    if(isLoaded())
    {
        Library_Delete(d->library); d->library = nullptr;
        zap(d->iBase);
        zap(d->iSfx);
        zap(d->iMusic);
        zap(d->iCd);
    }
}

void AudioDriver::initialize()
{
    LOG_AS("AudioDriver");

    // Already been here?
    if(d->initialized) return;

    DE_ASSERT(d->iBase.Init != nullptr);
    d->initialized = d->iBase.Init();
}

void AudioDriver::deinitialize()
{
    LOG_AS("AudioDriver");

    // Already been here?
    if(!d->initialized) return;

    if(d->iBase.Shutdown)
    {
        d->iBase.Shutdown();
    }
    d->initialized = false;
}

::Library *AudioDriver::library() const
{
    return d->library;
}

bool AudioDriver::isAvailable(String const &identifier)
{
    if (identifier == "dummy") return true;
#ifndef DE_DISABLE_SDLMIXER
    if (identifier == "sdlmixer") return true;
#else
    if (identifier == "sdlmixer") return false;
#endif
    return Impl::tryFindAudioPlugin(identifier);
}

audiodriver_t /*const*/ &AudioDriver::iBase() const
{
    return d->iBase;
}

bool AudioDriver::hasSfx() const
{
    return iSfx().gen.Init != nullptr;
}

bool AudioDriver::hasMusic() const
{
    return iMusic().gen.Init != nullptr;
}

bool AudioDriver::hasCd() const
{
    return iCd().gen.Init != nullptr;
}

audiointerface_sfx_t /*const*/ &AudioDriver::iSfx() const
{
    return d->iSfx;
}

audiointerface_music_t /*const*/ &AudioDriver::iMusic() const
{
    return d->iMusic;
}

audiointerface_cd_t /*const*/ &AudioDriver::iCd() const
{
    return d->iCd;
}

String AudioDriver::interfaceName(void *anyAudioInterface) const
{
    if((void *)&d->iSfx == anyAudioInterface)
    {
        /// @todo  SFX interfaces can't be named yet.
        return name();
    }

    if((void *)&d->iMusic == anyAudioInterface || (void *)&d->iCd == anyAudioInterface)
    {
        char buf[256];  /// @todo  This could easily overflow...
        auto *gen = (audiointerface_music_generic_t *) anyAudioInterface;
        if(gen->Get(MUSIP_ID, buf))
        {
            return buf;
        }
        else
        {
            return "[MUSIP_ID not defined]";
        }
    }

    return "";  // Not recognized.
}

String AudioDriver_GetName(audiodriverid_t id)
{
    static String const audioDriverNames[AUDIODRIVER_COUNT] = {
        /* AUDIOD_DUMMY */      "Dummy",
        /* AUDIOD_SDL_MIXER */  "SDLMixer",
        /* AUDIOD_OPENAL */     "OpenAL",
        /* AUDIOD_FMOD */       "FMOD",
        /* AUDIOD_FLUIDSYNTH */ "FluidSynth",
        /* AUDIOD_DSOUND */     "DirectSound",        // Win32 only
        /* AUDIOD_WINMM */      "Windows Multimedia"  // Win32 only
    };
    if(VALID_AUDIODRIVER_IDENTIFIER(id))
        return audioDriverNames[id];

    DE_ASSERT_FAIL("S_GetDriverName: Unknown driver id");
    return "";
}
