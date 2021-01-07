/**
 * @file driver_fmod.cpp
 * FMOD Studio low-level audio plugin. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html (with exception granted to allow
 * linking against FMOD Studio)
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses
 *
 * <b>Special Exception to GPLv2:</b>
 * Linking the Doomsday Audio Plugin for FMOD Studio (audio_fmod) statically or
 * dynamically with other modules is making a combined work based on
 * audio_fmod. Thus, the terms and conditions of the GNU General Public License
 * cover the whole combination. In addition, <i>as a special exception</i>, the
 * copyright holders of audio_fmod give you permission to combine audio_fmod
 * with free software programs or libraries that are released under the GNU
 * LGPL and with code included in the standard release of "FMOD Studio Programmer's
 * API" under the "FMOD Studio Programmer's API" license (or modified versions of
 * such code, with unchanged license). You may copy and distribute such a
 * system following the terms of the GNU GPL for audio_fmod and the licenses of
 * the other code concerned, provided that you include the source code of that
 * other code when and as the GNU GPL requires distribution of source code.
 * (Note that people who make modified versions of audio_fmod are not obligated
 * to grant this special exception for their modified versions; it is their
 * choice whether to do so. The GNU General Public License gives permission to
 * release a modified version without this exception; this exception also makes
 * it possible to release a modified version which carries forward this
 * exception.) </small>
 */

#include "driver_fmod.h"
#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "api_audiod_mus.h"
#include <stdio.h>
#include <fmod.h>
#include <fmod_errors.h>

#include "doomsday.h"
#include <de/c_wrapper.h>
#include <de/config.h>
#include <de/extension.h>
#include <de/logbuffer.h>
#include <de/dscript.h>

struct Driver
{
    de::String       name;
    FMOD_GUID        guid;
    int              systemRate;
    FMOD_SPEAKERMODE speakerMode;
    int              speakerModeChannels;
};

FMOD_SYSTEM *           fmodSystem;
static de::List<Driver> fmodDrivers;

static const char *speakerModeText(FMOD_SPEAKERMODE mode)
{
    switch (mode)
    {
    case FMOD_SPEAKERMODE_DEFAULT:  return "Default";
    case FMOD_SPEAKERMODE_RAW:      return "Raw";
    case FMOD_SPEAKERMODE_MONO:     return "Mono";
    case FMOD_SPEAKERMODE_STEREO:   return "Stereo";
    case FMOD_SPEAKERMODE_QUAD:     return "Quad";
    case FMOD_SPEAKERMODE_SURROUND: return "Surround";
    case FMOD_SPEAKERMODE_5POINT1:  return "5.1";
    case FMOD_SPEAKERMODE_7POINT1:  return "7.1";
    default: break;
    }
    return "";
}

/**
 * Initialize the FMOD Studio low-level sound driver.
 */
static int DS_Init(void)
{
    using namespace de;

    if (fmodSystem)
    {
        return true; // Already initialized.
    }

    // Create the FMOD audio system.
    FMOD_RESULT result;
    if ((result = FMOD_System_Create(&fmodSystem)) != FMOD_OK)
    {
        LOGDEV_AUDIO_ERROR("FMOD::System_Create failed (%d) %s") << result << FMOD_ErrorString(result);
        fmodSystem = 0;
        return false;
    }

    // Print the credit required by FMOD license.
    LOG_AUDIO_NOTE("FMOD by Firelight Technologies Pty Ltd");

    // Check what kind of drivers are available.
    {
        int numDrivers = 0;
        FMOD_System_GetNumDrivers(fmodSystem, &numDrivers);
        fmodDrivers.resize(numDrivers);
        std::unique_ptr<de::ArrayValue> names(new de::ArrayValue);
        for (int i = 0; i < numDrivers; ++i)
        {
            auto &drv = fmodDrivers[i];
            char nameBuf[512];
            zap(nameBuf);
            FMOD_System_GetDriverInfo(fmodSystem, i, nameBuf, sizeof(nameBuf),
                                      &drv.guid, &drv.systemRate,
                                      &drv.speakerMode, &drv.speakerModeChannels);
            drv.name = String::format("%s (%s)", nameBuf, speakerModeText(drv.speakerMode));
            names->add(TextValue(drv.name));

            LOG_AUDIO_MSG("FMOD driver %i: \"%s\" Rate:%iHz Mode:%s Channels:%i")
                    << i << drv.name
                    << drv.systemRate
                    << speakerModeText(drv.speakerMode)
                    << drv.speakerModeChannels;
        }
        ScriptSystem::get()["Audio"]["outputs"].value<DictionaryValue>()
                .add(new TextValue("fmod"), names.release());
    }

    // Select the configured driver.
    {
        int configuredDriverIndex = Config::get().geti("audio.output", 0);
        if (configuredDriverIndex < fmodDrivers.size())
        {
            result = FMOD_System_SetDriver(fmodSystem, configuredDriverIndex);
            if (result != FMOD_OK)
            {
                LOG_AUDIO_ERROR("Failed to select FMOD audio driver: %s")
                        << fmodDrivers[configuredDriverIndex].name;
            }
        }        
    }

#if 0
    const de::String speakerMode = de::Config::get().gets("audio.fmod.speakerMode", "");
    if (speakerMode == "5.1")
    {
        FMOD_System_SetSpeakerMode(fmodSystem, FMOD_SPEAKERMODE_5POINT1);
    }
    else if (speakerMode == "7.1")
    {
        FMOD_System_SetSpeakerMode(fmodSystem, FMOD_SPEAKERMODE_7POINT1);
    }
    else if (speakerMode == "prologic")
    {
        FMOD_System_SetSpeakerMode(fmodSystem, FMOD_SPEAKERMODE_SRS5_1_MATRIX);
    }

    // Manual overrides.
    if (CommandLine_Exists("-speaker51"))
    {
        FMOD_System_SetSpeakerMode(fmodSystem, FMOD_SPEAKERMODE_5POINT1);
    }
    if (CommandLine_Exists("-speaker71"))
    {
        FMOD_System_SetSpeakerMode(fmodSystem, FMOD_SPEAKERMODE_7POINT1);
    }
    if (CommandLine_Exists("-speakerprologic"))
    {
        FMOD_System_SetSpeakerMode(fmodSystem, FMOD_SPEAKERMODE_SRS5_1_MATRIX);
    }
#endif

    // Initialize FMOD.
    if ((result = FMOD_System_Init(fmodSystem,
                                   50,
                                   FMOD_INIT_NORMAL |
                                   FMOD_INIT_3D_RIGHTHANDED |
                                   FMOD_INIT_CHANNEL_LOWPASS, 0)) != FMOD_OK)
    {
        LOGDEV_AUDIO_ERROR("FMOD init failed: (%d) %s") << result << FMOD_ErrorString(result);
        FMOD_System_Release(fmodSystem);
        fmodSystem = 0;
        return false;
    }

#if FMOD_VERSION < 0x20000
    // Options.
    FMOD_ADVANCEDSETTINGS settings;
    de::zap(settings);
    settings.cbSize = sizeof(settings);
    settings.HRTFMaxAngle = 360;
    settings.HRTFMinAngle = 180;
    settings.HRTFFreq = 11000;
    FMOD_System_SetAdvancedSettings(fmodSystem, &settings);
#endif

#ifdef _DEBUG
    int numPlugins = 0;
    FMOD_System_GetNumPlugins(fmodSystem, FMOD_PLUGINTYPE_CODEC, &numPlugins);
    DSFMOD_TRACE("Plugins loaded: " << numPlugins);
    for (int i = 0; i < numPlugins; i++)
    {
        unsigned int handle;
        FMOD_System_GetPluginHandle(fmodSystem, FMOD_PLUGINTYPE_CODEC, i, &handle);

        FMOD_PLUGINTYPE pType;
        char pName[100];
        unsigned int pVer = 0;
        FMOD_System_GetPluginInfo(fmodSystem, handle, &pType, pName, sizeof(pName), &pVer);

        DSFMOD_TRACE("Plugin " << i << ", handle " << handle << ": type " << pType
                     << ", name:'" << pName << "', ver:" << pVer);
    }
#endif

    LOGDEV_AUDIO_VERBOSE("[FMOD] Initialized");
    return true;
}

/**
 * Shut everything down.
 */
static void DS_Shutdown(void)
{
    fmod_Music_Shutdown();
    //DMFmod_CDAudio_Shutdown();

    DSFMOD_TRACE("DS_Shutdown.");
    FMOD_System_Release(fmodSystem);
    fmodSystem = 0;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
static void DS_Event(int type)
{
    if (!fmodSystem) return;

    if (type == SFXEV_END)
    {
        // End of frame, do an update.
        FMOD_System_Update(fmodSystem);
    }
}

static int DS_Set(int prop, const void* ptr)
{
    if (!fmodSystem) return false;

    switch (prop)
    {
    case AUDIOP_SOUNDFONT_FILENAME: {
        const char* path = reinterpret_cast<const char*>(ptr);
        DSFMOD_TRACE("DS_Set: Soundfont = " << path);
        if (!path || !strlen(path))
        {
            // Use the default.
            path = 0;
        }
        fmod_Music_SetSoundFont(path);
        return true; }

    default:
        DSFMOD_TRACE("DS_Set: Unknown property " << prop);
        return false;
    }
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
static const char *deng_LibraryType(void)
{
    return "deng-plugin/audio";
}

DE_ENTRYPOINT void *extension_fmod_symbol(const char *name)
{
    DE_SYMBOL_PTR(name, deng_LibraryType)
    DE_SYMBOL_PTR(name, DS_Init)
    DE_SYMBOL_PTR(name, DS_Shutdown)
    DE_SYMBOL_PTR(name, DS_Event)
    DE_SYMBOL_PTR(name, DS_Set)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Init)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_CreateBuffer)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_DestroyBuffer)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Load)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Reset)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Play)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Stop)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Refresh)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Set)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Setv)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Listener)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Listenerv)
    DE_EXT_SYMBOL_PTR(fmod, name, DS_SFX_Getv)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_Init)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_Update)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_Get)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_Set)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_Pause)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_Stop)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_SongBuffer)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_Play)
    DE_EXT_SYMBOL_PTR(fmod, name, DM_Music_PlayFile)
    de::warning("\"%s\" not found in audio_fmod", name);
    return nullptr;
}
