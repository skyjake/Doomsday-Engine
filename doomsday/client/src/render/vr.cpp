/** @file render/vr.cpp  Stereoscopic rendering and Oculus Rift support.
 *
 * @authors Copyright (c) 2013-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
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

#include "de_console.h"
#include "render/vr.h"

using namespace de;

VRConfig vrCfg; // global

static int   vrMode             = VRConfig::Mono;
static float vrRiftFovX         = 114.8f;
static float vrIpd;
static int   vrRiftFBSamples;
static float vrNonRiftFovX      = 95.f;
static float riftLatency        = .030f;
static byte  autoLoadRiftParams = 1;

float VR_RiftFovX()
{
    return vrRiftFovX;
}

static void vrLatencyChanged()
{
    vrCfg.oculusRift().setPredictionLatency(riftLatency);
}

static void vrIpdChanged()
{
    vrCfg.setInterpupillaryDistance(vrIpd);
}

static void vrFBSamplesChanged()
{
    vrCfg.setRiftFramebufferSampleCount(vrRiftFBSamples);
}

// Interplay among vrNonRiftFovX, vrRiftFovX, and cameraFov depends on vrMode
// see also rend_main.cpp
static void vrModeChanged()
{
    vrCfg.setMode(VRConfig::StereoMode(vrMode));

    if(ClientWindow::mainExists())
    {
        // The logical UI size may need to be changed.
        ClientWindow &win = ClientWindow::main();
        win.updateRootSize();
        win.updateCanvasFormat(); // possibly changes pixel format
    }

    // Update FOV cvar accordingly.
    if(vrMode == VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);

        // Update prediction latency.
        vrCfg.oculusRift().setPredictionLatency(riftLatency);
    }
    else
    {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }
}

static void vrRiftFovXChanged()
{
    if(vrCfg.mode() == VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);
    }
}

static void vrNonRiftFovXChanged()
{
    if(vrCfg.mode() != VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }
}

D_CMD(LoadRiftParams)
{
    return VR_LoadRiftParameters();
}

void VR_ConsoleRegister()
{
    vrIpd           = vrCfg.interpupillaryDistance();
    vrRiftFBSamples = vrCfg.riftFramebufferSampleCount();

    /**
     * @todo When old-style console variables become obsolete, VRConfig should expose
     * these settings as a Record that can be attached under Config.
     */

    C_VAR_BYTE  ("rend-vr-autoload-rift-params", &autoLoadRiftParams, 0, 0, 1);
    C_VAR_FLOAT2("rend-vr-nonrift-fovx",         &vrNonRiftFovX,      0, 5.0f, 270.0f, vrNonRiftFovXChanged);
    C_VAR_FLOAT2("rend-vr-rift-fovx",            &vrRiftFovX,         0, 5.0f, 270.0f, vrRiftFovXChanged);
    C_VAR_FLOAT2("rend-vr-rift-latency",         &riftLatency,        0, 0.0f, 0.100f, vrLatencyChanged);

    C_VAR_FLOAT ("rend-vr-dominant-eye",  &vrCfg.dominantEye,       0, -1.0f, 1.0f);
    C_VAR_FLOAT ("rend-vr-hud-distance",  &vrCfg.hudDistance,       0, 0.01f, 40.0f);
    C_VAR_FLOAT2("rend-vr-ipd",           &vrIpd,                   0, 0.02f, 0.1f, vrIpdChanged);
    C_VAR_INT2  ("rend-vr-mode",          &vrMode,                  0, 0, VRConfig::NUM_STEREO_MODES - 1, vrModeChanged);
    C_VAR_FLOAT ("rend-vr-player-height", &vrCfg.playerHeight,      0, 1.0f, 2.4f);
    C_VAR_INT2  ("rend-vr-rift-samples",  &vrRiftFBSamples,         0, 1, 4, vrFBSamplesChanged);
    C_VAR_BYTE  ("rend-vr-swap-eyes",     &vrCfg.swapEyes,          0, 0, 1);

    C_CMD("loadriftparams", NULL, LoadRiftParams);
}

// Warping

/// @todo warping

bool VR_LoadRiftParameters()
{
    de::OculusRift &ovr = vrCfg.oculusRift();

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
