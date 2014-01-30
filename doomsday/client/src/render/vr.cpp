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

#ifdef DENG_HAVE_OCULUS_API
#  include <OVR.h>
#endif

namespace de {

#ifdef DENG_HAVE_OCULUS_API
class OculusTracker
{
public:
    OculusTracker()
        : pitch(0)
        , roll(0)
        , yaw(0)
        , _latency(0)
    {
        OVR::System::Init();
        _fusionResult = new OVR::SensorFusion();
        _manager = *OVR::DeviceManager::Create();
        _hmd = *_manager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
        if(_hmd)
        {
            _infoLoaded = _hmd->GetDeviceInfo(&_info);
            _sensor = _hmd->GetSensor();
        }
        else
        {
            _sensor = *_manager->EnumerateDevices<OVR::SensorDevice>().CreateDevice();
        }

        if (_sensor)
        {
            _fusionResult->AttachToSensor(_sensor);
        }
    }

    ~OculusTracker()
    {
        _sensor.Clear();
        _hmd.Clear();
        _manager.Clear();
        delete _fusionResult;
        OVR::System::Destroy();
    }

    OVR::HMDInfo const &getInfo() const {
        return _info;
    }

    bool isGood() const {
        return _sensor.GetPtr() != NULL;
    }

    void update()
    {
        OVR::Quatf quaternion;
        if (_latency == 0)
            quaternion = _fusionResult->GetOrientation();
        else
            quaternion = _fusionResult->GetPredictedOrientation();
        quaternion.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
    }

    void setLatency(float lat)
    {
        if (_latency == lat)
            return; // no change
        _latency = lat;
        if (_latency == 0)
        {
            _fusionResult->SetPredictionEnabled(false);
            _fusionResult->SetPrediction(_latency);
        }
        else
        {
            _fusionResult->SetPredictionEnabled(true);
            _fusionResult->SetPrediction(_latency);
        }
    }

    // Head orientation state, refreshed by call to update();
    float pitch, roll, yaw;

private:
    OVR::Ptr<OVR::DeviceManager> _manager;
    OVR::Ptr<OVR::HMDDevice> _hmd;
    OVR::Ptr<OVR::SensorDevice> _sensor;
    OVR::SensorFusion* _fusionResult;
    OVR::HMDInfo _info;
    bool _infoLoaded;
    float _latency;
};
#endif

DENG2_PIMPL(VRConfig)
{
    OculusRift ovr;
    float eyeHeightInMapUnits;

    Instance(Public *i)
        : Base(i)
        , eyeHeightInMapUnits(41)
    {}
};

VRConfig::VRConfig()
    : d(new Instance(this))
    , vrMode(ModeMono)
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

VRConfig::OculusRift &VRConfig::ovr()
{
    return d->ovr;
}

VRConfig::OculusRift const &VRConfig::ovr() const
{
    return d->ovr;
}

struct VRConfig::OculusRift::Instance : public Private<VRConfig::OculusRift>
{
    float ipd;
    bool headOrientationUpdateIsAllowed;

#ifdef DENG_HAVE_OCULUS_API
    OculusTracker *oculusTracker;
#endif

    Instance(VRConfig::OculusRift *i)
        : Private<VRConfig::OculusRift>(i)
        , ipd(.064f)
        , headOrientationUpdateIsAllowed(true)
#ifdef DENG_HAVE_OCULUS_API
        , oculusTracker(0)
#endif
    {}

    ~Instance()
    {
        self.deinit();
    }

    bool init()
    {
#ifdef DENG_HAVE_OCULUS_API
        if(oculusTracker)
        {
            return oculusTracker->isGood(); // Already inited.
        }

        oculusTracker = new OculusTracker;

        if(oculusTracker->isGood() /*&& autoLoadRiftParams*/)
        {
            OVR::HMDInfo const &info = oculusTracker->getInfo();
            ipd = info.InterpupillaryDistance;
            self._screenSize = Vector2f(info.HScreenSize, info.VScreenSize);
            self._lensSeparationDistance = info.LensSeparationDistance;
            self._hmdWarpParam = Vector4f(
                        info.DistortionK[0],
                        info.DistortionK[1],
                        info.DistortionK[2],
                        info.DistortionK[3]);
            self._chromAbParam = Vector4f(
                        info.ChromaAbCorrection[0],
                        info.ChromaAbCorrection[1],
                        info.ChromaAbCorrection[2],
                        info.ChromaAbCorrection[3]);
            self._eyeToScreenDistance = info.EyeToScreenDistance;
            return true;
        }
        return false;
#else
        return false;
#endif
    }
};

VRConfig::OculusRift::OculusRift()
    : d(new Instance(this))
    , _screenSize(0.14976f, 0.09360f)
    , _lensSeparationDistance(0.0635f)
    , _hmdWarpParam(1.0f, 0.220f, 0.240f, 0.000f)
    , _chromAbParam(0.996f, -0.004f, 1.014f, 0.0f)
    , _eyeToScreenDistance(0.041f)
    , riftLatency(.030f)
{}

bool VRConfig::OculusRift::init()
{
    return d->init();
}

void VRConfig::OculusRift::deinit()
{
#ifdef DENG_HAVE_OCULUS_API
    delete d->oculusTracker;
    d->oculusTracker = 0;
#endif
}

float VRConfig::OculusRift::interpupillaryDistance() const
{
    return d->ipd;
}

void VRConfig::OculusRift::setRiftLatency(float latency)
{
#ifdef DENG_HAVE_OCULUS_API
    if(isReady())
    {
        d->oculusTracker->setLatency(latency);
    }
#endif
}

// True if Oculus Rift is enabled and can report head orientation.
bool VRConfig::OculusRift::isReady() const
{
#ifdef DENG_HAVE_OCULUS_API
    if(!d->oculusTracker)
    {
        if(!d->init()) return false;
    }
    return d->oculusTracker->isGood();
#else
    // No API; No head tracking.
    return false;
#endif
}

void VRConfig::OculusRift::allowUpdate()
{
    d->headOrientationUpdateIsAllowed = true;
}

void VRConfig::OculusRift::update()
{
#ifdef DENG_HAVE_OCULUS_API
    if(d->headOrientationUpdateIsAllowed && isReady())
    {
        d->oculusTracker->update();
        d->headOrientationUpdateIsAllowed = false;
    }
#endif
}

Vector3f VRConfig::OculusRift::headOrientation() const
{
    de::Vector3f result;
#ifdef DENG_HAVE_OCULUS_API
    if(isReady())
    {
        result[0] = d->oculusTracker->pitch;
        result[1] = d->oculusTracker->roll;
        result[2] = d->oculusTracker->yaw;
    }
#endif
    return result;
}

float VRConfig::OculusRift::distortionScale() const
{
    float lensShift = _screenSize.x * 0.25f - lensSeparationDistance() * 0.5f;
    float lensViewportShift = 4.0f * lensShift / _screenSize.x;
    float fitRadius = fabs(-1 - lensViewportShift);
    float rsq = fitRadius*fitRadius;
    Vector4f k = hmdWarpParam();
    float scale = (k[0] + k[1] * rsq + k[2] * rsq * rsq + k[3] * rsq * rsq * rsq);
    return scale;
}

float VRConfig::OculusRift::fovX() const
{
    float w = 0.25 * _screenSize.x * distortionScale();
    float d = _eyeToScreenDistance;
    float fov = de::radianToDegree(2.0 * atan(w/d));
    return fov;
}

float VRConfig::OculusRift::fovY() const
{
    float w = 0.5 * _screenSize.y * distortionScale();
    float d = _eyeToScreenDistance;
    float fov = de::radianToDegree(2.0 * atan(w/d));
    return fov;
}

VRConfig::StereoMode VRConfig::mode() const
{
    return (StereoMode) vrMode;
}

bool VRConfig::modeNeedsStereoGLFormat(StereoMode mode)
{
    return mode == ModeQuadBuffered;
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

static float vrRiftFovX = 114.8f;
static float vrNonRiftFovX = 95.f;

float VR::riftFovX()
{
    return vrRiftFovX;
}

static void vrLatencyChanged()
{
    vrCfg.ovr().setRiftLatency(vrCfg.ovr().riftLatency);
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
    if(vrCfg.mode() == de::VRConfig::ModeOculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);

        // Update prediction latency.
        vrCfg.ovr().setRiftLatency(vrCfg.ovr().riftLatency);
    }
    else
    {
        if(Con_GetFloat("rend-camera-fov") != vrNonRiftFovX)
            Con_SetFloat("rend-camera-fov", vrNonRiftFovX);
    }
}

static void vrRiftFovXChanged()
{
    if(vrCfg.mode() == de::VRConfig::ModeOculusRift)
    {
        if(Con_GetFloat("rend-camera-fov") != vrRiftFovX)
            Con_SetFloat("rend-camera-fov", vrRiftFovX);
    }
}

static void vrNonRiftFovXChanged()
{
    if(vrCfg.mode() != de::VRConfig::ModeOculusRift)
    {
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
    C_VAR_BYTE  ("rend-vr-autoload-rift-params", & autoLoadRiftParams,  0, 0, 1);
    C_VAR_FLOAT2("rend-vr-nonrift-fovx",         & vrNonRiftFovX,       0, 5.0f, 270.0f, vrNonRiftFovXChanged);
    C_VAR_FLOAT2("rend-vr-rift-fovx",            & vrRiftFovX,          0, 5.0f, 270.0f, vrRiftFovXChanged);
    C_VAR_FLOAT2("rend-vr-rift-latency",         & vrCfg.ovr().riftLatency, 0, 0.0f, 0.100f, vrLatencyChanged);

    C_VAR_FLOAT ("rend-vr-dominant-eye",  & vrCfg.dominantEye,       0, -1.0f, 1.0f);
    C_VAR_FLOAT ("rend-vr-hud-distance",  & vrCfg.hudDistance,       0, 0.01f, 40.0f);
    C_VAR_FLOAT ("rend-vr-ipd",           & vrCfg.ipd,               0, 0.02f, 0.1f);
    C_VAR_INT2  ("rend-vr-mode",          & vrCfg.vrMode,            0, 0, de::VRConfig::NUM_STEREO_MODES - 1, vrModeChanged);
    C_VAR_FLOAT ("rend-vr-player-height", & vrCfg.playerHeight,      0, 1.0f, 2.4f);
    C_VAR_INT   ("rend-vr-rift-samples",  & vrCfg.riftFramebufferSamples, 0, 1, 4);
    C_VAR_BYTE  ("rend-vr-swap-eyes",     & vrCfg.swapEyes,          0, 0, 1);

    C_CMD("loadriftparams", NULL, LoadRiftParams);
}

// Warping

/// @todo warping

bool VR::loadRiftParameters()
{
    de::VRConfig::OculusRift &ovr = vrCfg.ovr();

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
