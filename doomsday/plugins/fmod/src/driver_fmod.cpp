/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "driver_fmod.h"
#include "sys_audiod.h"
#include "sys_audiod_sfx.h"
#include <stdio.h>
#include <fmod.h>
#include <fmod_errors.h>
#include <fmod.hpp>

FMOD::System* fmodSystem = 0;

/**
 * Initialize the FMOD Ex sound driver.
 */
int DS_Init(void)
{
    if(fmodSystem)
    {
        return true; // Already initialized.
    }

    // Create the FMOD audio system.
    FMOD_RESULT result;
    if((result = FMOD::System_Create(&fmodSystem)) != FMOD_OK)
    {
        printf("DS_Init: FMOD::System_Create failed: (%d) %s\n", result, FMOD_ErrorString(result));
        fmodSystem = 0;
        return false;
    }

    // Initialize FMOD.
    if((result = fmodSystem->init(50, FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED | FMOD_INIT_HRTF_LOWPASS, 0)) != FMOD_OK)
    {
        printf("DS_Init: FMOD init failed: (%d) %s\n", result, FMOD_ErrorString(result));
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

    DSFMOD_TRACE("DS_Init: FMOD initialized.");
    return true;
}

/**
 * Shut everything down.
 */
void DS_Shutdown(void)
{
    DM_Music_Shutdown();
    DM_CDAudio_Shutdown();

    DSFMOD_TRACE("DS_Shutdown.");
    fmodSystem->release();
    fmodSystem = 0;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_Event(int type)
{
    if(!fmodSystem) return;

    if(type == SFXEV_END)
    {
        // End of frame, do an update.
        fmodSystem->update();
    }
}

int DS_Set(int prop, const void* ptr)
{
    if(!fmodSystem) return false;

    switch(prop)
    {
    case AUDIOP_SOUNDFONT_FILENAME: {
        const char* path = reinterpret_cast<const char*>(ptr);
        DSFMOD_TRACE("DS_Set: Soundfont = " << path);
        if(!path || !strlen(path))
        {
            // Use the default.
            path = 0;
        }
        DM_Music_SetSoundFont(path);
        return true; }

    default:
        DSFMOD_TRACE("DS_Set: Unknown property " << prop);
        return false;
    }
}
