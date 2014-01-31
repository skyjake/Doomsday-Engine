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
    de::OculusRift ovr;
    float eyeHeightInMapUnits;
    float eyeShift;

    /**
     * Unlike most 3D modes, Oculus Rift typically uses no frustum shift. (or if we did,
     * it would be different and complicated)
     */
    bool frustumShift;

    Instance(Public *i)
        : Base(i)
        , eyeHeightInMapUnits(41)
        , eyeShift(0)
        , frustumShift(true)
    {
        ovr.init();
    }
};

VRConfig::VRConfig()
    : d(new Instance(this))
    , vrMode(Mono)
    , ipd(.064f) // average male IPD
    , playerHeight(1.75f)
    , dominantEye(0.0f)
    , swapEyes(false)
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

bool VRConfig::needsStereoGLFormat() const
{
    return modeNeedsStereoGLFormat(mode());
}

bool VRConfig::modeNeedsStereoGLFormat(StereoMode mode)
{
    return mode == QuadBuffered;
}

void VRConfig::setEyeHeightInMapUnits(float eyeHeightInMapUnits)
{
    d->eyeHeightInMapUnits = eyeHeightInMapUnits;
}

void VRConfig::setCurrentEye(Eye eye)
{
    float eyePos = (eye == NeitherEye? 0 : eye == LeftEye? -1 : 1);

    // 0.95 because eyes are not at top of head
    float mapUnitsPerMeter = d->eyeHeightInMapUnits / (0.95 * playerHeight);
    d->eyeShift = mapUnitsPerMeter * (eyePos - dominantEye) * 0.5 * ipd;
    if(swapEyes)
    {
        d->eyeShift *= -1;
    }
}

float VRConfig::eyeShift() const
{
    return d->eyeShift;
}

void VRConfig::enableFrustumShift(bool enable)
{
    d->frustumShift = enable;
}

bool VRConfig::frustumShift() const
{
    return d->frustumShift;
}

} // namespace de
