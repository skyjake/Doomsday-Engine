/** @file render/vr.cpp  Stereoscopic rendering and Oculus Rift support.
 *
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

VR::RiftState VR::riftState;

VR::RiftState::RiftState()
    : m_screenSize(0.14976f, 0.09360f)
    , m_lensSeparationDistance(0.0635f)
    , m_hmdWarpParam(1.0f, 0.220f, 0.240f, 0.000f)
    , m_chromAbParam(0.996f, -0.004f, 1.014f, 0.0f)
    , m_eyeToScreenDistance(0.041f)
{
}

float VR::RiftState::distortionScale() const
{
    float lensShift = hScreenSize() * 0.25f - lensSeparationDistance() * 0.5f;
    float lensViewportShift = 4.0f * lensShift / hScreenSize();
    float fitRadius = fabs(-1 - lensViewportShift);
    float rsq = fitRadius*fitRadius;
    const de::Vector4f& k = hmdWarpParam();
    float scale = (k[0] + k[1] * rsq + k[2] * rsq * rsq + k[3] * rsq * rsq * rsq);
    return scale;
}

float VR::RiftState::fovX() const {
    float w = 0.25 * hScreenSize() * distortionScale();
    float d = m_eyeToScreenDistance;
    float fov = de::radianToDegree(2.0 * atan(w/d));
    return fov;
}

float VR::RiftState::fovY() const {
    float w = 0.5 * vScreenSize() * distortionScale();
    float d = m_eyeToScreenDistance;
    float fov = de::radianToDegree(2.0 * atan(w/d));
    return fov;
}

// Sometimes we want viewpoint to remain constant between left and right eye views
static bool holdView = false;
void VR::holdViewPosition()
{
    holdView = true;
}

void VR::releaseViewPosition()
{
    holdView = false;
}

bool VR::viewPositionHeld()
{
    return holdView;
}

// Console variables

static int vrMode = (int)VR::MODE_MONO;
VR::Stereo3DMode VR::mode()
{
    return (VR::Stereo3DMode)vrMode;
}

bool VR::modeNeedsStereoGLFormat(VR::Stereo3DMode mode)
{
    return mode == VR::MODE_QUAD_BUFFERED;
}

static float vrRiftFovX = 114.8f;
float VR::riftFovX() /// Horizontal field of view in degrees
{
    return vrRiftFovX;
}

static float vrNonRiftFovX = 95.0;

static float vrLatency = 0.030f;
float VR::riftLatency() {
    return vrLatency;
}

// Interpupillary distance in meters
float VR::ipd = 0.0622f;
float VR::playerHeight = 1.70f;
float VR::dominantEye = 0.0f;
byte  VR::swapEyes = 0;


// Global variables
bool  VR::applyFrustumShift = true;
float  VR::eyeShift = 0;
float  VR::hudDistance = 20.0f;
float  VR::weaponDistance = 10.0f;


/// @param eye: -1 means left eye, +1 means right eye
/// @return viewpoint eye shift in map units
float VR::getEyeShift(float eye)
{
    // 0.95 because eyes are not at top of head
    float mapUnitsPerMeter = Con_GetInteger("player-eyeheight") / ((0.95) *  VR::playerHeight);
    float result = mapUnitsPerMeter * (eye -  VR::dominantEye) * 0.5 *  VR::ipd;
    if ( VR::swapEyes != 0)
        result *= -1;
    return result;
}

static void vrLatencyChanged()
{
    if (VR::hasHeadOrientation()) {
        VR::setRiftLatency(vrLatency);
    }
}


// Interplay among vrNonRiftFovX, vrRiftFovX, and cameraFov depends on vrMode
// see also rend_main.cpp
static void vrModeChanged()
{
    if(ClientWindow::hasMain())
    {
        // The logical UI size may need to be changed.
        ClientWindow &win = ClientWindow::main();
        win.updateRootSize();
        win.updateCanvasFormat(); // possibly changes pixel format
    }
    if (VR::mode() == VR::MODE_OCULUS_RIFT) {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);
    }
    else {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }
}

static void vrRiftFovXChanged()
{
    if (VR::mode() == VR::MODE_OCULUS_RIFT) {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);
    }
}

static void vrNonRiftFovXChanged()
{
    if (VR::mode() != VR::MODE_OCULUS_RIFT) {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }
}

static byte autoLoadRiftParams = 1;
D_CMD(LoadRiftParams)
{
    return VR::loadRiftParameters();
}

void VR::consoleRegister()
{
    C_VAR_BYTE  ("rend-vr-autoload-rift-params", & autoLoadRiftParams, 0, 0, 1);
    C_VAR_FLOAT ("rend-vr-dominant-eye",     & VR::dominantEye,   0, -1.0f, 1.0f);
    C_VAR_FLOAT ("rend-vr-hud-distance",     & VR::hudDistance,           0, 0.01f, 40.0f);
    C_VAR_FLOAT ("rend-vr-ipd",              & VR::ipd,           0, 0.02f, 0.2f);
    C_VAR_INT2  ("rend-vr-mode",             & vrMode,            0, 0, (int)(VR::MODE_MAX_3D_MODE_PLUS_ONE - 1), vrModeChanged);
    C_VAR_FLOAT2("rend-vr-nonrift-fovx",     & vrNonRiftFovX,     0, 5.0f, 270.0f, vrNonRiftFovXChanged);
    C_VAR_FLOAT ("rend-vr-player-height",    & VR::playerHeight,  0, 1.0f, 3.0f);
    C_VAR_FLOAT2("rend-vr-rift-fovx",        & vrRiftFovX,        0, 5.0f, 270.0f, vrRiftFovXChanged);
    C_VAR_FLOAT2("rend-vr-rift-latency",     & vrLatency,         0, 0.0f, 0.250f, vrLatencyChanged);
    C_VAR_BYTE  ("rend-vr-swap-eyes",        & VR::swapEyes,      0, 0, 1);

    C_CMD("loadriftparams", NULL, LoadRiftParams);
}


// Warping

/// @todo warping

// Head tracking

#ifdef DENG_HAVE_OCULUS_API
#include "OVR.h"

class OculusTracker {
public:
    OculusTracker()
        : pitch(0)
        , roll(0)
        , yaw(0)
        , latency(0)
    {
        OVR::System::Init();
        pFusionResult = new OVR::SensorFusion();
        pManager = *OVR::DeviceManager::Create();
        pHMD = *pManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
        if(pHMD)
        {
            InfoLoaded = pHMD->GetDeviceInfo(&Info);
            pSensor = pHMD->GetSensor();
        }
        else
        {
            pSensor = *pManager->EnumerateDevices<OVR::SensorDevice>().CreateDevice();
        }

        if (pSensor)
        {
            pFusionResult->AttachToSensor(pSensor);
        }
    }

    ~OculusTracker()
    {
        pSensor.Clear();
        pHMD.Clear();
        pManager.Clear();
        delete pFusionResult;
        OVR::System::Destroy();
    }

    const OVR::HMDInfo& getInfo() const {
        return Info;
    }

    bool isGood() const {
        return pSensor.GetPtr() != NULL;
    }

    void update()
    {

        OVR::Quatf quaternion;
        if (latency == 0)
            quaternion = pFusionResult->GetOrientation();
        else
            quaternion = pFusionResult->GetPredictedOrientation();
        quaternion.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
    }

    void setLatency(float lat)
    {
        if (latency == lat)
            return; // no change
        latency = lat;
        if (latency == 0)
        {
            pFusionResult->SetPredictionEnabled(false);
            pFusionResult->SetPrediction(latency);
        }
        else
        {
            pFusionResult->SetPredictionEnabled(true);
            pFusionResult->SetPrediction(latency);
        }
    }

    // Head orientation state, refreshed by call to update();
    float pitch, roll, yaw;

private:
    OVR::Ptr<OVR::DeviceManager> pManager;
    OVR::Ptr<OVR::HMDDevice> pHMD;
    OVR::Ptr<OVR::SensorDevice> pSensor;
    OVR::SensorFusion* pFusionResult;
    OVR::HMDInfo Info;
    bool InfoLoaded;
    float latency;
};

static OculusTracker* oculusTracker = NULL;
#endif

static bool loadRiftParametersNoCheck() {
#ifdef DENG_HAVE_OCULUS_API
    VR::riftState.loadRiftParameters();
    const OVR::HMDInfo& info = oculusTracker->getInfo();
    Con_SetFloat("rend-vr-ipd", info.InterpupillaryDistance);
    Con_SetFloat("rend-vr-rift-fovx", VR::riftState.fovX());
    // I think this field of view is unreliable... CMB
    /*
    float fov = 180.0f / de::PI * 2.0f * (atan2(
              info.EyeToScreenDistance,
              0.5f * (info.HScreenSize - info.InterpupillaryDistance)));
              */
    return true;
#endif
    return false;
}
// True if Oculus Rift is enabled and can report head orientation.
bool VR::hasHeadOrientation()
{
#ifdef DENG_HAVE_OCULUS_API
    if (oculusTracker == NULL) {
        oculusTracker = new OculusTracker();
        if (oculusTracker->isGood() && autoLoadRiftParams)
            loadRiftParametersNoCheck();
    }
    return oculusTracker->isGood();
#else
    // No API; No head tracking.
    return false;
#endif
}

bool VR::loadRiftParameters() {
    if (! VR::hasHeadOrientation())
        return false;
    return loadRiftParametersNoCheck();
}

bool VR::RiftState::loadRiftParameters() {
    if (! VR::hasHeadOrientation())
        return false;
#ifdef DENG_HAVE_OCULUS_API
    const OVR::HMDInfo& info = oculusTracker->getInfo();
    m_screenSize = de::Vector2f(info.HScreenSize, info.VScreenSize);
    m_lensSeparationDistance = info.LensSeparationDistance;
    m_hmdWarpParam = de::Vector4f(
                info.DistortionK[0],
                info.DistortionK[1],
                info.DistortionK[2],
                info.DistortionK[3]);
    m_chromAbParam = de::Vector4f(
                info.ChromaAbCorrection[0],
                info.ChromaAbCorrection[1],
                info.ChromaAbCorrection[2],
                info.ChromaAbCorrection[3]);
    m_eyeToScreenDistance = info.EyeToScreenDistance;
    return true;
#else
    return false;
#endif
}

void VR::setRiftLatency(float latency)
{
#ifdef DENG_HAVE_OCULUS_API
    if (! VR::hasHeadOrientation())
        return;
    oculusTracker->setLatency(latency);
#endif
}

// Returns current pitch, roll, yaw angles, in radians
std::vector<float> VR::getHeadOrientation()
{
    std::vector<float> result;
#ifdef DENG_HAVE_OCULUS_API
    if (! VR::hasHeadOrientation())
        return result; // empty vector
    oculusTracker->update();
    result.push_back(oculusTracker->pitch);
    result.push_back(oculusTracker->roll);
    result.push_back(oculusTracker->yaw);
    return result;
#else
    // No API; Return empty vector. You should have called VR::hasHeadOrientation() first...
    return result;
#endif
}

// To release memory and resources when done, for tidiness.
void VR::deleteOculusTracker()
{
#ifdef DENG_HAVE_OCULUS_API
    if (oculusTracker != NULL)
    {
        delete oculusTracker;
        oculusTracker = NULL;
    }
#endif
}

