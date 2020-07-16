/** @file vrconfig.cpp  Virtual reality configuration.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/vrconfig.h"
#include "de/math.h"

namespace de {

DE_PIMPL(VRConfig)
{
    StereoMode mode;
    Eye currentEye = NeitherEye;
    de::OculusRift ovr;
    float screenDistance;
    float ipd;
    float eyeHeightInMapUnits;
    float eyeShift;
    float playerPhysicalHeight;
    bool swapEyes;
    int riftFramebufferSamples; // Multisampling used in unwarped Rift framebuffer

    /**
     * Unlike most 3D modes, Oculus Rift typically uses no frustum shift. (or if we did,
     * it would be different and complicated)
     */
    bool frustumShift;

    float dominantEye; ///< Kludge for aim-down-weapon-sight modes

    Impl(Public *i)
        : Base(i)
        , mode(Mono)
        , screenDistance(20.f)
        , ipd(.064f) // average male IPD
        , eyeHeightInMapUnits(41)
        , eyeShift(0)
        , playerPhysicalHeight(1.75f)
        , swapEyes(false)
        , riftFramebufferSamples(1)
        , frustumShift(true)
        , dominantEye(0.0f)
    {}

    float mapUnitsPerMeter() const
    {
        // 0.925 because eyes are not at top of head
        return eyeHeightInMapUnits / (0.925 * playerPhysicalHeight);
    }
};

VRConfig::VRConfig() : d(new Impl(this))
{}

void VRConfig::setMode(StereoMode newMode)
{
    d->mode = newMode;
}

void VRConfig::setScreenDistance(float distance)
{
    d->screenDistance = distance;
}

void VRConfig::setEyeHeightInMapUnits(float eyeHeightInMapUnits)
{
    d->eyeHeightInMapUnits = eyeHeightInMapUnits;
}

void VRConfig::setInterpupillaryDistance(float ipd)
{
    d->ipd = ipd;
}

void VRConfig::setPhysicalPlayerHeight(float heightInMeters)
{
    d->playerPhysicalHeight = heightInMeters;
}

void VRConfig::setCurrentEye(Eye eye)
{
    float eyePos = (eye == NeitherEye? 0 : eye == LeftEye? -1 : 1);

    d->currentEye = eye;
    d->eyeShift = d->mapUnitsPerMeter() * (eyePos - d->dominantEye) * 0.5 * d->ipd;
    if (d->swapEyes)
    {
        d->eyeShift *= -1;
    }
}

VRConfig::Eye VRConfig::currentEye() const
{
    return d->currentEye;
}

void VRConfig::enableFrustumShift(bool enable)
{
    d->frustumShift = enable;
}

void VRConfig::setRiftFramebufferSampleCount(int samples)
{
    d->riftFramebufferSamples = samples;
}

void VRConfig::setSwapEyes(bool swapped)
{
    d->swapEyes = swapped;
}

void VRConfig::setDominantEye(float value)
{
    d->dominantEye = value;
}

VRConfig::StereoMode VRConfig::mode() const
{
    return d->mode;
}

float VRConfig::screenDistance() const
{
    return d->screenDistance;
}

bool VRConfig::needsStereoGLFormat() const
{
    return modeNeedsStereoGLFormat(mode());
}

bool VRConfig::modeNeedsStereoGLFormat(StereoMode mode)
{
    return mode == QuadBuffered;
}

float VRConfig::interpupillaryDistance() const
{
    return d->ipd;
}

float VRConfig::eyeHeightInMapUnits() const
{
    return d->eyeHeightInMapUnits;
}

float VRConfig::mapUnitsPerMeter() const
{
    return d->mapUnitsPerMeter();
}

float VRConfig::physicalPlayerHeight() const
{
    return d->playerPhysicalHeight;
}

float VRConfig::eyeShift() const
{
    return d->eyeShift;
}

bool VRConfig::frustumShift() const
{
    return d->frustumShift;
}

bool VRConfig::swapEyes() const
{
    return d->swapEyes;
}

float VRConfig::dominantEye() const
{
    return d->dominantEye;
}

int VRConfig::riftFramebufferSampleCount() const
{
    return d->riftFramebufferSamples;
}

float VRConfig::viewAspect(const Vec2f &viewPortSize) const
{
    /*if (mode() == OculusRift)
    {
        // Override with the Oculus Rift's aspect ratio.
        return oculusRift().aspect();
    }*/

    // We're assuming pixels are squares.
    return viewPortSize.x / viewPortSize.y;
}

float VRConfig::verticalFieldOfView(float horizFovDegrees, const Vec2f &viewPortSize) const
{
    // We're assuming pixels are squares.
    const float aspect = viewAspect(viewPortSize);

    if (mode() == OculusRift)
    {
        // A little trigonometry to apply aspect ratio to angles
        float x = std::tan(.5f * degreeToRadian(horizFovDegrees));
        return radianToDegree(2.f * std::atan2(x / aspect, 1.f));
    }

    return clamp(1.f, horizFovDegrees / aspect, 179.f);
}

Mat4f VRConfig::projectionMatrix(float fovDegrees,
                                    const Vec2f &viewPortSize,
                                    float nearClip, float farClip) const
{
    if (mode() == OculusRift && oculusRift().isReady())
    {
        // OVR will calculate our projection matrix.
        const float mapUnits = d->mapUnitsPerMeter();
        return oculusRift().projection(nearClip, farClip) *
               Mat4f::translate(oculusRift().eyeOffset() * mapUnits);
    }
    return Mat4f::perspective(fovDegrees, viewAspect(viewPortSize), nearClip, farClip) *
           Mat4f::translate(Vec3f(-eyeShift(), 0, 0));

#if 0
    const float yfov = verticalFieldOfView(fovDegrees, viewPortSize);
    const float fH   = std::tan(.5f * degreeToRadian(yfov)) * nearClip;
    const float fW   = fH * viewAspect(viewPortSize);

    /*
     * Asymmetric frustum shift is computed to realign screen-depth items after view point has shifted.
     * Asymmetric frustum shift method is probably superior to competing toe-in stereo 3D method:
     *  - AFS preserves identical near and far clipping planes in both views
     *  - AFS shows items at/near infinity better
     *  - AFS conforms to what stereo 3D photographers call "ortho stereo"
     * Asymmetric frustum shift is used for all stereo 3D modes except Oculus Rift mode, which only
     * applies the viewpoint shift.
     */
    float shift = 0;
    if (frustumShift())
    {
        shift = eyeShift() * nearClip / screenDistance();
    }

    return Mat4f::frustum(-fW - shift, fW - shift,
                             -fH, fH,
                             nearClip, farClip) *
           Mat4f::translate(Vec3f(-eyeShift(), 0, 0));
#endif
}

OculusRift &VRConfig::oculusRift()
{
    return d->ovr;
}

const OculusRift &VRConfig::oculusRift() const
{
    return d->ovr;
}

bool VRConfig::modeAppliesDisplacement(StereoMode mode)
{
    switch (mode)
    {
    case Mono:
    case GreenMagenta:
    case RedCyan:
    case LeftOnly:
    case RightOnly:
    case QuadBuffered:
    case RowInterleaved:
        return false;

    default:
        break;
    }
    return true;
}

} // namespace de
