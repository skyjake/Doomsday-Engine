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

#include <de/extension.h>
//#include <de/library.h>
//#include <de/libraryfile.h>
//#include <de/nativefile.h>

using namespace de;

DE_PIMPL(AudioDriver)
{
    bool   initialized = false;
    String extension;

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

//    static LibraryFile *tryFindAudioPlugin(const String &name)
//    {
//        if (!name.isEmpty())
//        {
//            LibraryFile *found = nullptr;
//            Library_ForAll([&name, &found] (LibraryFile &lib)
//            {
//                if (lib.hasUnderscoreName(name))
//                {
//                    found = &lib;
//                    return LoopAbort;
//                }
//                return LoopContinue;
//            });
//            return found;
//        }
//        return nullptr;
//    }

    template <typename Type> bool setSymbolPtr(Type &ptr, const char *name, bool required = true)
    {
        functionAssign(ptr, extensionSymbol(extension, name));
        if (required && !ptr)
        {
            throw LoadError("AudioDriver::setSymbolPtr",
                            "Extension \"" + extension + "\" does not have symbol \"" + name + "\"");
        }
        return ptr != 0;
    }

    void getDummyInterfaces()
    {
        DE_ASSERT(!initialized);

        extension.clear();
        std::memcpy(&iBase,  &audiod_dummy,       sizeof(iBase));
        std::memcpy(&iSfx,   &audiod_dummy_sfx,   sizeof(iSfx));
        std::memcpy(&iMusic, &audiod_dummy_music, sizeof(iMusic));
        std::memcpy(&iCd,    &audiod_dummy_cd,    sizeof(iCd));
    }

#ifndef DE_DISABLE_SDLMIXER
    void getSdlMixerInterfaces()
    {
        DE_ASSERT(!initialized);

        extension.clear();
        std::memcpy(&iBase,  &audiod_sdlmixer,       sizeof(iBase));
        std::memcpy(&iSfx,   &audiod_sdlmixer_sfx,   sizeof(iSfx));
        std::memcpy(&iMusic, &audiod_sdlmixer_music, sizeof(iMusic));
        std::memcpy(&iCd,    &audiod_dummy_cd,       sizeof(iCd));
    }
#endif

    void importInterfaces(const String &plugName)
    {
        DE_ASSERT(!initialized);

        if (!isExtensionRegistered(plugName))
        {
            /// @throw LoadError  Unknown driver specified.
            throw LoadError("AudioDriver::load", "Unknown driver \"" + plugName + "\"");
        }

        extension = plugName;

        zap(iBase);
        zap(iSfx);
        zap(iMusic);
        zap(iCd);

//        library = Library_New(libFile.path());
//        if(!library)
//        {
//            throw LoadError("AudioDriver::importInterfaces",
//                            "Failed to load " + libFile.description());
//        }

//        de::Library &lib = libFile.library();

        setSymbolPtr( iBase.Init,        "DS_Init");
        setSymbolPtr( iBase.Shutdown,    "DS_Shutdown");
        setSymbolPtr( iBase.Event,       "DS_Event");
        setSymbolPtr( iBase.Set,         "DS_Set", false);

        if (extensionSymbol(extension, "DS_SFX_Init"))
        {
            setSymbolPtr( iSfx.gen.Init,      "DS_SFX_Init");
            setSymbolPtr( iSfx.gen.Create,    "DS_SFX_CreateBuffer");
            setSymbolPtr( iSfx.gen.Destroy,   "DS_SFX_DestroyBuffer");
            setSymbolPtr( iSfx.gen.Load,      "DS_SFX_Load");
            setSymbolPtr( iSfx.gen.Reset,     "DS_SFX_Reset");
            setSymbolPtr( iSfx.gen.Play,      "DS_SFX_Play");
            setSymbolPtr( iSfx.gen.Stop,      "DS_SFX_Stop");
            setSymbolPtr( iSfx.gen.Refresh,   "DS_SFX_Refresh");
            setSymbolPtr( iSfx.gen.Set,       "DS_SFX_Set");
            setSymbolPtr( iSfx.gen.Setv,      "DS_SFX_Setv");
            setSymbolPtr( iSfx.gen.Listener,  "DS_SFX_Listener");
            setSymbolPtr( iSfx.gen.Listenerv, "DS_SFX_Listenerv");
            setSymbolPtr( iSfx.gen.Getv,      "DS_SFX_Getv", false);
        }

        if (extensionSymbol(extension, "DM_Music_Init"))
        {
            setSymbolPtr( iMusic.gen.Init,    "DM_Music_Init");
            setSymbolPtr( iMusic.gen.Update,  "DM_Music_Update");
            setSymbolPtr( iMusic.gen.Get,     "DM_Music_Get");
            setSymbolPtr( iMusic.gen.Set,     "DM_Music_Set");
            setSymbolPtr( iMusic.gen.Pause,   "DM_Music_Pause");
            setSymbolPtr( iMusic.gen.Stop,    "DM_Music_Stop");
            setSymbolPtr( iMusic.SongBuffer,  "DM_Music_SongBuffer", false);
            setSymbolPtr( iMusic.Play,        "DM_Music_Play",       false);
            setSymbolPtr( iMusic.PlayFile,    "DM_Music_PlayFile",   false);
        }

        if (extensionSymbol(extension, "DM_CDAudio_Init"))
        {
            setSymbolPtr( iCd.gen.Init,       "DM_CDAudio_Init");
            setSymbolPtr( iCd.gen.Update,     "DM_CDAudio_Update");
            setSymbolPtr( iCd.gen.Set,        "DM_CDAudio_Set");
            setSymbolPtr( iCd.gen.Get,        "DM_CDAudio_Get");
            setSymbolPtr( iCd.gen.Pause,      "DM_CDAudio_Pause");
            setSymbolPtr( iCd.gen.Stop,       "DM_CDAudio_Stop");
            setSymbolPtr( iCd.Play,           "DM_CDAudio_Play");
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
    switch (status())
    {
    case Invalid:     return "Invalid";
    case Loaded:      return "Loaded";
    case Initialized: return "Initialized";
    }
    return "";
}

void AudioDriver::load(const String &identifier)
{
    LOG_AS("AudioDriver");

    if (isLoaded())
    {
        /// @throw LoadError  Attempted to load on top of an already loaded driver.
        throw LoadError("AudioDriver::load", "Already initialized. Cannot load '" + identifier + "'");
    }

    // Perhaps a built-in audio driver?
    if (!identifier.compareWithoutCase("dummy"))
    {
        d->getDummyInterfaces();
        return;
    }
#ifndef DE_DISABLE_SDLMIXER
    if (!identifier.compareWithoutCase("sdlmixer"))
    {
        d->getSdlMixerInterfaces();
        return;
    }
#endif

    // Exchange entrypoints.
    d->importInterfaces(identifier);
}

void AudioDriver::unload()
{
    LOG_AS("AudioDriver");

    if (isInitialized())
    {
        /// @throw LoadError  Cannot unload while initialized.
        throw LoadError("AudioDriver::unload", "'" + name() + "' is still initialized, cannot unload");
    }

    if (isLoaded())
    {
//        Library_Delete(d->library); d->library = nullptr;
        zap(d->iBase);
        zap(d->iSfx);
        zap(d->iMusic);
        zap(d->iCd);
        d->extension.clear();
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

String AudioDriver::extensionName() const
{
    return d->extension;
}

bool AudioDriver::isAvailable(const String &identifier)
{
    if (identifier == "dummy") return true;
#ifndef DE_DISABLE_SDLMIXER
    if (identifier == "sdlmixer") return true;
#else
    if (identifier == "sdlmixer") return false;
#endif
    return isExtensionRegistered(identifier);
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
