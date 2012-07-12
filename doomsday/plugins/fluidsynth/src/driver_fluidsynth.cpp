/**
 * @file driver_fluidsynth.cpp
 * FluidSynth music plugin. @ingroup dsfluidsynth
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fluidsynth.h"
#include "sys_audiod.h"
#include <stdio.h>

#include "doomsday.h"

static fluid_settings_t* fsConfig;
static fluid_synth_t* fsSynth;
static audiointerface_sfx_t* fsSfx;

fluid_synth_t* DMFluid_Synth()
{
    DENG_ASSERT(fsSynth != 0);
    return fsSynth;
}

audiointerface_sfx_generic_t* DMFluid_Sfx()
{
    DENG_ASSERT(fsSfx != 0);
    return &fsSfx->gen;
}

/**
 * Initialize the FluidSynth sound driver.
 */
int DS_Init(void)
{
    if(fsSynth)
    {
        return true; // Already initialized.
    }

    fsConfig = new_fluid_settings();

    // TODO: configure fluidsynth

    // Create the synthesizer.
    fsSynth = new_fluid_synth(fsConfig);
    if(!fsSynth)
    {
        Con_Message("Failed to create FluidSynth synthesizer.\n");
        return false;
    }

#if 0
    if(fmodSystem)
    {
        return true; // Already initialized.
    }

    // Create the FMOD audio system.
    FMOD_RESULT result;
    if((result = FMOD::System_Create(&fmodSystem)) != FMOD_OK)
    {
        Con_Message("FMOD::System_Create failed: (%d) %s\n", result, FMOD_ErrorString(result));
        fmodSystem = 0;
        return false;
    }

    // Initialize FMOD.
    if((result = fmodSystem->init(50, FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED | FMOD_INIT_HRTF_LOWPASS, 0)) != FMOD_OK)
    {
        Con_Message("FMOD init failed: (%d) %s\n", result, FMOD_ErrorString(result));
        fmodSystem->release();
        fmodSystem = 0;
        return false;
    }

    // Options.
    FMOD_ADVANCEDSETTINGS settings;
    zeroStruct(settings);
    settings.HRTFMaxAngle = 360;
    settings.HRTFMinAngle = 180;
    settings.HRTFFreq = 11000;
    fmodSystem->setAdvancedSettings(&settings);

#ifdef _DEBUG
    int numPlugins = 0;
    fmodSystem->getNumPlugins(FMOD_PLUGINTYPE_CODEC, &numPlugins);
    DSFMOD_TRACE("Plugins loaded: " << numPlugins);
    for(int i = 0; i < numPlugins; i++)
    {
        unsigned int handle;
        fmodSystem->getPluginHandle(FMOD_PLUGINTYPE_CODEC, i, &handle);

        FMOD_PLUGINTYPE pType;
        char pName[100];
        unsigned int pVer = 0;
        fmodSystem->getPluginInfo(handle, &pType, pName, sizeof(pName), &pVer);

        DSFMOD_TRACE("Plugin " << i << ", handle " << handle << ": type " << pType
                     << ", name:'" << pName << "', ver:" << pVer);
    }
#endif

    // Print the credit required by FMOD license.
    Con_Message("FMOD Sound System (c) Firelight Technologies Pty, Ltd., 1994-2012.\n");

#endif

    DSFLUIDSYNTH_TRACE("DS_Init: FluidSynth initialized.");
    return true;
}

/**
 * Shut everything down.
 */
void DS_Shutdown(void)
{
    if(!fsSynth) return;

    DMFluid_Shutdown();

    DSFLUIDSYNTH_TRACE("DS_Shutdown.");

    delete_fluid_synth(fsSynth);
    delete_fluid_settings(fsConfig);

    fsSynth = 0;
    fsConfig = 0;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_Event(int type)
{
    if(!fsSynth) return;

    if(type == SFXEV_END)
    {
        // End of frame, do an update.
        DMFluid_Update();
    }
}

int DS_Set(int prop, const void* ptr)
{
    //if(!fmodSystem) return false;

    switch(prop)
    {
    case AUDIOP_SOUNDFONT_FILENAME: {
        const char* path = reinterpret_cast<const char*>(ptr);
        DSFLUIDSYNTH_TRACE("DS_Set: Soundfont = " << path);
        if(!path || !strlen(path))
        {
            // Use the default.
            path = 0;
        }
        DMFluid_SetSoundFont(path);
        return true; }

    case AUDIOP_SFX_INTERFACE:
        fsSfx = (audiointerface_sfx_t*) ptr;
        DSFLUIDSYNTH_TRACE("DS_Set: iSFX = " << fsSfx);
        return true;

    default:
        DSFLUIDSYNTH_TRACE("DS_Set: Unknown property " << prop);
        return false;
    }
}
