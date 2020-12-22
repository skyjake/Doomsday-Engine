/** @file vr.cpp  Stereoscopic rendering and Oculus Rift support.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
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

#include "de_base.h"
#include "render/vr.h"

#include <de/legacy/timer.h>
#include <de/baseguiapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include "ui/clientwindow.h"

using namespace de;

namespace VR {
    float weaponDistance = 10.f; // global
}

static int   vrMode             = VRConfig::Mono;
//static float vrRiftFovX         = 114.8f;
//static float vrNonRiftFovX      = 95.f;
//static float vrRiftLatency;
//static byte  autoLoadRiftParams = 1;
static int   vrRiftFBSamples;
static float vrHudDistance;
static float vrPlayerHeight;
static float vrIpd;
static byte  vrSwapEyes;
static float vrDominantEye;

VRConfig &vrCfg()
{
    DE_ASSERT(DE_BASE_GUI_APP != 0);
    return DE_BASE_GUI_APP->vr();
}

/*
float VR_RiftFovX()
{
    return vrRiftFovX;
}
*/

static void vrConfigVariableChanged()
{
    vrCfg().setDominantEye(vrDominantEye);
    vrCfg().setScreenDistance(vrHudDistance);
    vrCfg().setInterpupillaryDistance(vrIpd);
    vrCfg().setPhysicalPlayerHeight(vrPlayerHeight);
    //vrCfg().oculusRift().setPredictionLatency(vrRiftLatency);
    vrCfg().setRiftFramebufferSampleCount(vrRiftFBSamples);
    vrCfg().setSwapEyes(vrSwapEyes);
}

// Interplay among vrNonRiftFovX, vrRiftFovX, and cameraFov depends on vrMode
// see also rend_main.cpp
static void vrModeChanged()
{
    /*
    if(vrMode == VRConfig::OculusRift && !vrCfg().oculusRift().isReady())
    {
        // Can't activate Oculus Rift mode unless the device is connected.
        vrMode = VRConfig::Mono;

        LOG_WARNING("Oculus Rift not connected, reverting to normal 3D mode");
    }*/

    vrCfg().setMode(VRConfig::StereoMode(vrMode));

    if (ClientWindow::mainExists())
    {
        // The logical UI size may need to be changed.
        ClientWindow &win = ClientWindow::main();
        win.updateRootSize();
        //win.updateCanvasFormat(); // possibly changes pixel format
    }

    /*
    // Update FOV cvar accordingly.
    if(vrMode == VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);

        // Update prediction latency.
        vrCfg().oculusRift().setPredictionLatency(vrRiftLatency);
    }
    else
    {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }*/
}

/*
static void vrRiftFovXChanged()
{
    if(vrCfg().mode() == VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);
    }
}

static void vrNonRiftFovXChanged()
{
    if(vrCfg().mode() != VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }
}

D_CMD(LoadRiftParams)
{
    DE_UNUSED(src, argc, argv);
    return VR_LoadRiftParameters();
}*/

D_CMD(ResetRiftPose)
{
    DE_UNUSED(src, argc, argv);
    vrCfg().oculusRift().resetTracking();
    LOG_INPUT_MSG("Reset Oculus Rift position tracking");
    return true;
}

void VR_ConsoleRegister()
{
    // Get the built-in defaults.
    vrDominantEye   = vrCfg().dominantEye();
    vrHudDistance   = vrCfg().screenDistance();
    vrIpd           = vrCfg().interpupillaryDistance();
    vrPlayerHeight  = vrCfg().physicalPlayerHeight();
    //vrRiftLatency   = vrCfg().oculusRift().predictionLatency();
    vrRiftFBSamples = vrCfg().riftFramebufferSampleCount();
    vrSwapEyes      = vrCfg().swapEyes();

    /**
     * @todo When old-style console variables become obsolete, VRConfig should expose
     * these settings as a Record that can be attached under Config.
     */

    C_VAR_INT2  ("rend-vr-mode",                 &vrMode,             0, 0, VRConfig::NUM_STEREO_MODES - 1, vrModeChanged);
    //C_VAR_BYTE  ("rend-vr-autoload-rift-params", &autoLoadRiftParams, 0, 0, 1);
    //C_VAR_FLOAT2("rend-vr-nonrift-fovx",         &vrNonRiftFovX,      0, 5.0f, 270.0f, vrNonRiftFovXChanged);
    //C_VAR_FLOAT2("rend-vr-rift-fovx",            &vrRiftFovX,         0, 5.0f, 270.0f, vrRiftFovXChanged);
    //C_VAR_FLOAT2("rend-vr-rift-latency",  &vrRiftLatency,   0, 0.0f,  0.100f, vrConfigVariableChanged);

    C_VAR_FLOAT2("rend-vr-dominant-eye",  &vrDominantEye,   0, -1.0f, 1.0f,   vrConfigVariableChanged);
    C_VAR_FLOAT2("rend-vr-hud-distance",  &vrHudDistance,   0, 0.01f, 40.0f,  vrConfigVariableChanged);
    C_VAR_FLOAT2("rend-vr-ipd",           &vrIpd,           0, 0.02f, 0.1f,   vrConfigVariableChanged);
    C_VAR_FLOAT2("rend-vr-player-height", &vrPlayerHeight,  0, 1.0f,  2.4f,   vrConfigVariableChanged);
    C_VAR_INT2  ("rend-vr-rift-samples",  &vrRiftFBSamples, 0, 1, 4,          vrConfigVariableChanged);
    C_VAR_BYTE2 ("rend-vr-swap-eyes",     &vrSwapEyes,      0, 0, 1,          vrConfigVariableChanged);

    //C_CMD("loadriftparams", NULL, LoadRiftParams);
    C_CMD("resetriftpose", NULL, ResetRiftPose);
}

#if 0
bool VR_LoadRiftParameters()
{
    de::OculusRift &ovr = vrCfg().oculusRift();

    if(ovr.isReady())
    {
        Con_SetFloat("rend-vr-ipd", ovr.interpupillaryDistance()); // from Oculus SDK
        Con_SetFloat("rend-vr-rift-fovx", ovr.fovX());

        // I think this field of view is unreliable... CMB
        /*
        float fov = 180.0f / de::PI * 2.0f * (atan2(
                  info.EyeToScreenDistance,
                  0.5f * (info.HScreenSize - info.InterpupillaryDistance)));
                */
        return true;
    }
    return false;
}
#endif
