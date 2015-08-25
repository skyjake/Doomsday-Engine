/** @file mixer.cpp  Object-oriented model for a logical audio mixer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2008-2015 Daniel Swanson <danij@dengine.net>
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

#define WIN32_LEAN_AND_MEAN
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0400
# undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0400
#endif

#include "mixer.h"

#include <de/Log>
#include <windows.h>
#include <mmsystem.h>
#include <cmath>

using namespace de;

static MMRESULT res;  ///< Used for conveniently testing zero-value => success results.

DENG2_PIMPL_NOREF(Mixer::Line)
{
    Mixer *mixer = nullptr;  ///< Parent mixer (not owned).
    MIXERLINE hndl;
    MIXERLINECONTROLS controls;
    MIXERCONTROL volume;
    bool initialized = false;

    Instance()
    {
        de::zap(hndl);
        de::zap(controls);
        de::zap(volume);
    }

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }

    void initialize(Type type)
    {
        DENG2_ASSERT(mixer != nullptr);
        DENG2_ASSERT(!initialized);

        de::zap(hndl);
        hndl.cbStruct        = sizeof(hndl);
        hndl.dwComponentType = (type == Cd ? MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC
                                           : MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER);

        if((::res = mixerGetLineInfo(reinterpret_cast<HMIXEROBJ>(mixer->getHandle()), &hndl,
                                     MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE))
           != MMSYSERR_NOERROR)
        {
            LOG_AUDIO_ERROR("[WinMM] Error getting line info: Error %u") << ::res;
            return;
        }

        LOGDEV_AUDIO_MSG("  Destination line idx: %i") << hndl.dwDestination;
        LOGDEV_AUDIO_MSG("  Line ID: 0x%x") << hndl.dwLineID;
        LOGDEV_AUDIO_MSG("  Channels: %i") << hndl.cChannels;
        LOGDEV_AUDIO_MSG("  Controls: %i") << hndl.cControls;
        LOG_AUDIO_MSG("  Line name: %s (%s)") << hndl.szName << hndl.szShortName;

        de::zap(controls);
        de::zap(volume);
        controls.cbStruct      = sizeof(controls);
        controls.dwLineID      = hndl.dwLineID;
        controls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
        controls.cControls     = 1;
        controls.cbmxctrl      = sizeof(volume);
        controls.pamxctrl      = &volume;
        if((::res = mixerGetLineControls(reinterpret_cast<HMIXEROBJ>(mixer->getHandle()), &controls,
                                         MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE))
           != MMSYSERR_NOERROR)
        {
            LOG_AUDIO_ERROR("[WinMM] Error getting line controls (vol): error %u") << ::res;
            return;
        }

        LOGDEV_AUDIO_MSG("  Volume control ID: 0x%x") << volume.dwControlID;
        LOG_AUDIO_MSG("  Volume name: %s (%s)") << volume.szName << volume.szShortName;
        LOGDEV_AUDIO_MSG("  Min/Max: %i/%i") << volume.Bounds.dwMinimum << volume.Bounds.dwMaximum;

        initialized = true;
    }

    void deinitialize()
    {
        // Stub.
        initialized = false;
    }
};

Mixer::Line::Line(Mixer &mixer, Mixer::Line::Type type) : d(new Instance)
{
    d->mixer = &mixer;
    d->initialize(type);
}

Mixer::Line::~Line()
{
    d->deinitialize();
}

bool Mixer::Line::isReady() const
{
    return d->initialized;
}

Mixer &Mixer::Line::mixer()
{
    DENG2_ASSERT(d->mixer != nullptr);
    return *d->mixer;
}

Mixer const &Mixer::Line::mixer() const
{
    return const_cast<Line *>(this)->mixer();
}

void Mixer::Line::setVolume(dfloat newVolume)
{
    LOG_AS("[WinMM]Mixer::Line");

    if(!d->initialized)
    {
        /// @throw Mixer::ReadyError  Not yet initialized.
        throw Mixer::ReadyError("[WinMM]Mixer::Line::setVolume", "Line is not initialized");
    }

    // Straighten the volume curve.
    //auto const vol = de::clamp<dint>(0, newVolume * 255 + .5f, 255) << 8 /*Make it a word*/;
    auto const vol = de::clamp<dint>(0, 255.9980469 * std::sqrt(newVolume), 255);

    /// @todo Evidently there is only a single volume control for both channels on Windows 10.
    /// Is this always the case or should we be enumerating the available controls? -ds
    MIXERCONTROLDETAILS mcd; de::zap(mcd);
    MIXERCONTROLDETAILS_UNSIGNED mcdUnsigned/*[2]*/; de::zap(mcdUnsigned);
    mcd.cbStruct    = sizeof(mcd);
    mcd.dwControlID = d->volume.dwControlID;
    mcd.cChannels   = 1;  //line.hndl->cChannels;
    mcd.cbDetails   = sizeof(mcdUnsigned);
    mcd.paDetails   = &mcdUnsigned;

    mcdUnsigned/*[0]*/.dwValue =
        //mcdUnsigned[1].dwValue =
            (vol * (d->volume.Bounds.dwMaximum - d->volume.Bounds.dwMinimum))
            / 255 + d->volume.Bounds.dwMinimum;

    if((::res = mixerSetControlDetails(reinterpret_cast<HMIXEROBJ>(mixer().getHandle()), &mcd,
                                       MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE))
        != MMSYSERR_NOERROR)
    {
        auto const msg = "Error: " + String::number(::res);
        LOG_AUDIO_ERROR("Failed setting volume. ") << msg;
    }
}

dint Mixer::Line::volume() const
{
    LOG_AS("[WinMM]Mixer::Line");

    if(!d->initialized)
    {
        /// @throw Mixer::ReadyError  Not yet initialized.
        throw Mixer::ReadyError("[WinMM]Mixer::Line::volume", "Line is not initialized");
    }

    /// @todo Evidently there is only a single volume control for both channels on Windows 10.
    /// Is this always the case or should we be enumerating the available controls? -ds
    MIXERCONTROLDETAILS mcd; de::zap(mcd);
    MIXERCONTROLDETAILS_UNSIGNED mcdUnsigned/*[2]*/; de::zap(mcdUnsigned);
    mcd.cbStruct    = sizeof(mcd);
    mcd.dwControlID = d->volume.dwControlID;
    mcd.cChannels   = 1;  //line.hndl->cChannels;
    mcd.cbDetails   = sizeof(mcdUnsigned);
    mcd.paDetails   = &mcdUnsigned;
    if((::res = mixerGetControlDetails(reinterpret_cast<HMIXEROBJ>(mixer().getHandle()), &mcd,
                                       MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE))
        != MMSYSERR_NOERROR)
    {
        auto const msg = "Error: " + String::number(::res);
        LOG_AUDIO_ERROR("Failed getting volume. ") << msg;
    }

    // The bigger one is the real volume.
    dint const i = mcdUnsigned/*[mcdUnsigned[0].dwValue > mcdUnsigned[1].dwValue ? 0 : 1]*/.dwValue;

    // Return the value in range 0-255.
    return (255 * (i - d->volume.Bounds.dwMinimum)) / (d->volume.Bounds.dwMaximum - d->volume.Bounds.dwMinimum);
}

DENG2_PIMPL_NOREF(Mixer)
{
    HMIXER hndl = nullptr;
    std::unique_ptr<Line> cdLine;
    std::unique_ptr<Line> synthLine;
    bool initialized = false;

    ~Instance()
    {
        // Should have been deinitialized by now.
        DENG2_ASSERT(!initialized);
    }

    void initialize(Mixer &self)
    {
        // Already been here?
        if(initialized) return;

        // Open the mixer device.
        if((::res = mixerOpen(&hndl, 0, 0, 0, MIXER_OBJECTF_MIXER))
           != MMSYSERR_NOERROR)
        {
            LOG_AUDIO_ERROR("[WinMM] Error opening mixer: Error %u") << ::res;
            return;
        }

        // We're successful.
        initialized = true;

        {
            // Log available caps.
            MIXERCAPS mixerCaps;
            mixerGetDevCaps((UINT_PTR) hndl, &mixerCaps, sizeof(mixerCaps));
            LOG_AUDIO_MSG("[WinMM] %s") << mixerCaps.szPname;
            LOG_AUDIO_VERBOSE("  Audio line destinations: %i") << mixerCaps.cDestinations;
        }

        LOG_AUDIO_VERBOSE("Initializing CD-Audio line...");
        cdLine.reset(new Line(self, Line::Type::Cd));

        LOG_AUDIO_VERBOSE("Initializing Synthesizer line...");
        synthLine.reset(new Line(self, Line::Type::Synth));
    }

    void deinitialize()
    {
        // Already been here?
        if(!initialized) return;

        initialized = false;
        mixerClose(hndl);
        hndl = nullptr;
    }
};

Mixer::Mixer() : d(new Instance)
{
    LOG_AS("[WinMM]Mixer");
    d->initialize(*this);
}

Mixer::~Mixer()
{
    LOG_AS("[WinMM]~Mixer");
    d->deinitialize();
}

bool Mixer::isReady() const
{
    return d->initialized;
}

Mixer::Line &Mixer::line(Mixer::LineId lineId)
{
    if(d->initialized)
    {
        switch(lineId)
        {
        case LineId::Cd:    if(d->cdLine)    return *d->cdLine;    break;
        case LineId::Synth: if(d->synthLine) return *d->synthLine; break;

        default: DENG2_ASSERT(!"Invalid line id"); break;
        }
        /// @throw MissingLineError  Invalid line id specified.
        throw MissingLineError("Mixer::line", "Unknown line #" + String::number(lineId));
    }
    /// @throw ReadyError  Not yet initialized.
    throw ReadyError("Mixer::line", "Mixer is not initialized");
}

Mixer::Line const &Mixer::line(Mixer::LineId lineId) const
{
    return const_cast<Mixer *>(this)->line(lineId);
}

void *Mixer::getHandle() const
{
    return d->hndl;
}

dint Mixer::deviceCount()  // static
{
    return mixerGetNumDevs();
}
