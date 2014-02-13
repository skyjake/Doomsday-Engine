/** @file oculusrift.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/OculusRift"

#include <de/Lockable>
#include <de/Guard>
#include <de/App>

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
#ifdef DENG2_DEBUG
        OVR::System::Init(OVR::Log::ConfigureDefaultLog(OVR::LogMask_All));
#else
        OVR::System::Init();
#endif
        _fusionResult = new OVR::SensorFusion();
        _manager = *OVR::DeviceManager::Create();
        _hmd = *_manager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
        if(_hmd)
        {
            _infoLoaded = _hmd->GetDeviceInfo(&_info);
            _sensor = *_hmd->GetSensor();
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

    OVR::HMDInfo const &getInfo() const
    {
        return _info;
    }

    bool isGood() const
    {
        return _sensor.GetPtr() != NULL;
    }

    void update()
    {
        OVR::Quatf quaternion;
        if(_latency == 0)
        {
            quaternion = _fusionResult->GetOrientation();
        }
        else
        {
            quaternion = _fusionResult->GetPredictedOrientation();
        }
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

    float latency() const
    {
        return _latency;
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

DENG2_PIMPL(OculusRift), public Lockable
{
    bool inited;
    Vector2f screenSize;
    float lensSeparationDistance;
    Vector4f hmdWarpParam;
    Vector4f chromAbParam;
    float eyeToScreenDistance;
    float latency;
    float ipd;
    bool headOrientationUpdateIsAllowed;

#ifdef DENG_HAVE_OCULUS_API
    OculusTracker *oculusTracker;
#endif

    Instance(Public *i)
        : Base(i)
        , inited(false)
        , screenSize(0.14976f, 0.09360f)
        , lensSeparationDistance(0.0635f)
        , hmdWarpParam(1.0f, 0.220f, 0.240f, 0.000f)
        , chromAbParam(0.996f, -0.004f, 1.014f, 0.0f)
        , eyeToScreenDistance(0.041f)
        , latency(.030f)
        , ipd(.064f)
        , headOrientationUpdateIsAllowed(true)
#ifdef DENG_HAVE_OCULUS_API
        , oculusTracker(0)
#endif
    {}

    ~Instance()
    {
        DENG2_GUARD(this);
        deinit();
    }

    void init()
    {
        if(inited) return;
        inited = true;

#ifdef DENG_HAVE_OCULUS_API
        DENG2_GUARD(this);

        DENG2_ASSERT_IN_MAIN_THREAD();
        DENG2_ASSERT(!oculusTracker);

        oculusTracker = new OculusTracker;

        if(oculusTracker->isGood() /*&& autoLoadRiftParams*/)
        {
            OVR::HMDInfo const &info = oculusTracker->getInfo();
            ipd = info.InterpupillaryDistance;
            screenSize = Vector2f(info.HScreenSize, info.VScreenSize);
            lensSeparationDistance = info.LensSeparationDistance;
            hmdWarpParam = Vector4f(
                        info.DistortionK[0],
                        info.DistortionK[1],
                        info.DistortionK[2],
                        info.DistortionK[3]);
            chromAbParam = Vector4f(
                        info.ChromaAbCorrection[0],
                        info.ChromaAbCorrection[1],
                        info.ChromaAbCorrection[2],
                        info.ChromaAbCorrection[3]);
            eyeToScreenDistance = info.EyeToScreenDistance;
        }
#endif
    }

    void deinit()
    {
        if(!inited) return;
        inited = false;

#ifdef DENG_HAVE_OCULUS_API
        DENG2_GUARD(this);

        delete oculusTracker;
        oculusTracker = 0;
#endif
    }
};

OculusRift::OculusRift() : d(new Instance(this))
{}

bool OculusRift::init()
{
    d->init();
    return isReady();
}

void OculusRift::deinit()
{
    d->deinit();
}

float OculusRift::interpupillaryDistance() const
{
    return d->ipd;
}

float OculusRift::aspect() const
{
    return 0.5f * d->screenSize.x / d->screenSize.y;
}

Vector2f OculusRift::screenSize() const
{
    return d->screenSize;
}

Vector4f OculusRift::chromAbParam() const
{
    return d->chromAbParam;
}

Vector4f OculusRift::hmdWarpParam() const
{
    return d->hmdWarpParam;
}

float OculusRift::lensSeparationDistance() const
{
    return d->lensSeparationDistance;
}

void OculusRift::setPredictionLatency(float latency)
{
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);
    if(isReady())
    {
        d->oculusTracker->setLatency(latency);
    }
#endif
}

float OculusRift::predictionLatency() const
{
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);
    if(isReady())
    {
        return d->oculusTracker->latency();
    }
#endif
    return 0;
}

// True if Oculus Rift is enabled and can report head orientation.
bool OculusRift::isReady() const
{
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);
    return d->inited && d->oculusTracker->isGood();
#else
    return false;
#endif
}

void OculusRift::allowUpdate()
{
    d->headOrientationUpdateIsAllowed = true;
}

void OculusRift::update()
{
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);

    if(d->headOrientationUpdateIsAllowed && isReady())
    {
        d->oculusTracker->update();
        d->headOrientationUpdateIsAllowed = false;
    }
#endif
}

Vector3f OculusRift::headOrientation() const
{
    de::Vector3f result;
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);
    if(isReady())
    {
        result[0] = d->oculusTracker->pitch;
        result[1] = d->oculusTracker->roll;
        result[2] = d->oculusTracker->yaw;
    }
#endif
    return result;
}

float OculusRift::distortionScale() const
{
    float lensShift = d->screenSize.x * 0.25f - lensSeparationDistance() * 0.5f;
    float lensViewportShift = 4.0f * lensShift / d->screenSize.x;
    float fitRadius = fabs(-1 - lensViewportShift);
    float rsq = fitRadius*fitRadius;
    Vector4f k = hmdWarpParam();
    float scale = (k[0] + k[1] * rsq + k[2] * rsq * rsq + k[3] * rsq * rsq * rsq);
    return scale;
}

float OculusRift::fovX() const
{
    float const w = 0.25 * d->screenSize.x * distortionScale();
    return de::radianToDegree(2.0 * atan(w / d->eyeToScreenDistance));
}

float OculusRift::fovY() const
{
    float const w = 0.5 * d->screenSize.y * distortionScale();
    return de::radianToDegree(2.0 * atan(w / d->eyeToScreenDistance));
}

} // namespace de
