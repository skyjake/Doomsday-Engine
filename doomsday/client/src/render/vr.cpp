/** @file render/vr.cpp  Stereoscopic rendering and Oculus Rift support.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/OculusRift>

namespace de {

DENG2_PIMPL(VRConfig)
{
    de::OculusRift ovr;
    float eyeHeightInMapUnits;

    Instance(Public *i)
        : Base(i)
        , eyeHeightInMapUnits(41)
    {}
};

VRConfig::VRConfig()
    : d(new Instance(this))
    , vrMode(Mono)
    , ipd(.064f) // average male IPD
    , playerHeight(1.75f)
    , dominantEye(0.0f)
    , swapEyes(0)
    , applyFrustumShift(true)
    , eyeShift(0)
    , hudDistance(20.0f)
    , weaponDistance(10)
    , riftFramebufferSamples(2)
{}

OculusRift &VRConfig::oculusRift()
{
    return d->ovr;
}

OculusRift const &VRConfig::oculusRift() const
{
    return d->ovr;
}

VRConfig::StereoMode VRConfig::mode() const
{
    return (StereoMode) vrMode;
}

bool VRConfig::modeNeedsStereoGLFormat(StereoMode mode)
{
    return mode == QuadBuffered;
}

void VRConfig::setEyeHeightInMapUnits(float eyeHeightInMapUnits)
{
    d->eyeHeightInMapUnits = eyeHeightInMapUnits;
}

float VRConfig::getEyeShift(float eye) const
{
    // 0.95 because eyes are not at top of head
    float mapUnitsPerMeter = d->eyeHeightInMapUnits / (0.95 * playerHeight);
    float result = mapUnitsPerMeter * (eye - dominantEye) * 0.5 * ipd;
    if(swapEyes)
    {
        result *= -1;
    }
    return result;
}

} // namespace de

de::VRConfig vrCfg; // global

static float vrRiftFovX    = 114.8f;
static float vrNonRiftFovX = 95.f;
static float riftLatency   = .030f;
static byte autoLoadRiftParams = 1;

float VR::riftFovX()
{
    return vrRiftFovX;
}

static void vrLatencyChanged()
{
    vrCfg.oculusRift().setPredictionLatency(riftLatency);
}

// Interplay among vrNonRiftFovX, vrRiftFovX, and cameraFov depends on vrMode
// see also rend_main.cpp
static void vrModeChanged()
{
    if(ClientWindow::mainExists())
    {
        // The logical UI size may need to be changed.
        ClientWindow &win = ClientWindow::main();
        win.updateRootSize();
        win.updateCanvasFormat(); // possibly changes pixel format
    }
    if(vrCfg.mode() == de::VRConfig::OculusRift)
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
    if(vrCfg.mode() == de::VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);
    }
}

static void vrNonRiftFovXChanged()
{
    if(vrCfg.mode() != de::VRConfig::OculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }
}

D_CMD(LoadRiftParams)
{
    return VR::loadRiftParameters();
}

void VR::consoleRegister()
{
    C_VAR_BYTE  ("rend-vr-autoload-rift-params", &autoLoadRiftParams, 0, 0, 1);
    C_VAR_FLOAT2("rend-vr-nonrift-fovx",         &vrNonRiftFovX,      0, 5.0f, 270.0f, vrNonRiftFovXChanged);
    C_VAR_FLOAT2("rend-vr-rift-fovx",            &vrRiftFovX,         0, 5.0f, 270.0f, vrRiftFovXChanged);
    C_VAR_FLOAT2("rend-vr-rift-latency",         &riftLatency,        0, 0.0f, 0.100f, vrLatencyChanged);

    C_VAR_FLOAT ("rend-vr-dominant-eye",  &vrCfg.dominantEye,       0, -1.0f, 1.0f);
    C_VAR_FLOAT ("rend-vr-hud-distance",  &vrCfg.hudDistance,       0, 0.01f, 40.0f);
    C_VAR_FLOAT ("rend-vr-ipd",           &vrCfg.ipd,               0, 0.02f, 0.1f);
    C_VAR_INT2  ("rend-vr-mode",          &vrCfg.vrMode,            0, 0, de::VRConfig::NUM_STEREO_MODES - 1, vrModeChanged);
    C_VAR_FLOAT ("rend-vr-player-height", &vrCfg.playerHeight,      0, 1.0f, 2.4f);
    C_VAR_INT   ("rend-vr-rift-samples",  &vrCfg.riftFramebufferSamples, 0, 1, 4);
    C_VAR_BYTE  ("rend-vr-swap-eyes",     &vrCfg.swapEyes,          0, 0, 1);

    C_CMD("loadriftparams", NULL, LoadRiftParams);
}

// Warping

/// @todo warping

bool VR::loadRiftParameters()
{
    de::OculusRift &ovr = vrCfg.oculusRift();

    if(ovr.isReady())
    {
        Con_SetFloat("rend-vr-ipd", ovr.interpupillaryDistance());
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
