/** @file vrconfig.cpp  Virtual reality configuration.
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

#include "de/VRConfig"

namespace de {

DENG2_PIMPL(VRConfig)
{
    StereoMode mode;
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

    Instance(Public *i)
        : Base(i)
        , mode(Mono)
        , screenDistance(20.f)
        , ipd(.064f) // average male IPD
        , eyeHeightInMapUnits(41)
        , eyeShift(0)
        , playerPhysicalHeight(1.75f)
        , swapEyes(false)
        , riftFramebufferSamples(2)
        , frustumShift(true)
        , dominantEye(0.0f)
    {
        ovr.init();
    }
};

VRConfig::VRConfig() : d(new Instance(this))
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

    // 0.95 because eyes are not at top of head
    float mapUnitsPerMeter = d->eyeHeightInMapUnits / (0.95 * d->playerPhysicalHeight);
    d->eyeShift = mapUnitsPerMeter * (eyePos - d->dominantEye) * 0.5 * d->ipd;
    if(d->swapEyes)
    {
        d->eyeShift *= -1;
    }
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

OculusRift &VRConfig::oculusRift()
{
    return d->ovr;
}

OculusRift const &VRConfig::oculusRift() const
{
    return d->ovr;
}

} // namespace de
