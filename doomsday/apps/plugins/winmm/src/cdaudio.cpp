/** @file cdaudio.cpp  WinMM CD-DA playback interface.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define WIN32_LEAN_AND_MEAN
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0400
# undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0400
#endif

#include "cdaudio.h"

#include <de/Error>
#include <de/Log>
#include <de/c_wrapper.h>
#include <de/timer.h>
#include <windows.h>
#include <mmsystem.h>
#include <cstdio>

using namespace de;

/**
 * @todo fixme: (Debug) NtClose throws "invalid handle was specified" during deinit -ds
 */
DENG2_PIMPL(CdAudio)
{
    // Base class for MCI command errors. @ingroup errors
    DENG2_ERROR(MCIError);

    // Device binding:
    String deviceId;
    bool initialized = false;

    // Current track:
    dint track = -1;  ///< @c -1= not set
    ddouble trackLength = 0;

    // Playback:
    bool looping = false;
    ddouble startTime = 0;
    ddouble pauseTime = 0;

    Instance(Public *i) : Base(i) {}
    ~Instance() { DENG2_ASSERT(!initialized); }

    void initialize()
    {
        if(initialized) return;
        if(deviceId.isEmpty()) return;

        try
        {
            sendMCICmd(String("open cdaudio alias ") + deviceId);
            sendMCICmd(String("set ") + deviceId + " time format tmsf");
            initialized = true;
        }
        catch(MCIError const &er)
        {
            LOG_AUDIO_ERROR("Init failed. ") << er.asText();
        };
    }

    void deinitialize()
    {
        if(!initialized) return;
        
        initialized = false;
        try
        {
            sendMCICmd(String("close ") + deviceId);
        }
        catch(MCIError const &er)
        {
            LOG_AUDIO_ERROR("Deinit failed. ") << er.asText();
        }
    }

    /**
     * Returns the length of the given @a track number in seconds.
     */
    dint getTrackLength(dint track)
    {
        if(!initialized) return 0;

        try
        {
            Block const lenStr = sendMCICmd(String("status ") + deviceId + " length track " + String::number(track))
                                     .toLatin1();
            dint minutes, seconds;
            sscanf(lenStr.constData(), "%i:%i", &minutes, &seconds);
            return minutes * 60 + seconds;
        }
        catch(Instance::MCIError const &er)
        {
            LOG_AUDIO_ERROR("") << er.asText();
        }
        return 0;
    }

    /**
     * Execute an MCI command string.
     */
    static String sendMCICmd(String const &command)
    {
        static char retInfo[80];
        zap(retInfo);

        LOG_WIP("Sending command:\n") << command;
        if(MCIERROR error = mciSendStringA(command.toLatin1().constData(), retInfo, 80, nullptr))
        {
            char msg[300];
            mciGetErrorStringA(error, msg, 300);
            throw MCIError("[WinMM]CDAudio", String("MCI Error:") + msg);
        }
        return retInfo;
    }
};

CdAudio::CdAudio(String const &deviceId) : d(new Instance(this))
{
    LOG_AS("[WinMM]CdAudio");
    d->deviceId = deviceId;
    d->initialize();
}

CdAudio::~CdAudio()
{
    LOG_AS("[WinMM]~CdAudio");
    stop();
    d->deinitialize();
}

void CdAudio::update()
{
    if(d->track < 0 || !d->looping) return;
    
    // Time to restart the track?
    if(Timer_Seconds() - d->startTime > d->trackLength)
    {
        LOG_AS("[WinMM]CdAudio::update");
        LOG_WIP("Restarting track #%i...") << d->track;
        play(d->track, true);
    }
}

bool CdAudio::isPlaying()
{
    if(!d->initialized) return false;
    try
    {
        return Instance::sendMCICmd(String("status ") + d->deviceId + " mode wait")
                             .beginsWith("playing");
    }
    catch(Instance::MCIError const &er)
    {
        LOG_AS("[WinMM]CDAudio::isPlaying");
        LOG_AUDIO_ERROR("") << er.asText();
    }
    return false;
}

void CdAudio::stop()
{
    if(!d->initialized) return;
    if(d->track < 0) return;

    d->track = -1;
    Instance::sendMCICmd(String("stop ") + d->deviceId);
}

void CdAudio::pause(bool setPause)
{
    if(!d->initialized) return;

    if(d->track >= 0)
    {
        Instance::sendMCICmd(String(setPause ? "pause " : "play ") + d->deviceId);
    }

    if(setPause)
    {
        d->pauseTime = Timer_Seconds();
    }
    else
    {
        d->startTime += Timer_Seconds() - d->pauseTime;
    }
}

bool CdAudio::play(dint newTrack, bool looped)
{
    if(!d->initialized) return false;

    LOG_AS("[WinMM]CdAudio::play");

    // Only play CD-DA tracks of non-zero length.
    d->trackLength = d->getTrackLength(newTrack);
    if(!d->trackLength) return false;

    d->track = -1;

    // Play it!
    try
    {
        Instance::sendMCICmd(String("play ") + d->deviceId + " from " + String::number(newTrack) + " to " + String::number(MCI_MAKE_TMSF(newTrack, 0, d->trackLength, 0)));
        d->looping   = looped;
        d->startTime = Timer_Seconds();
        d->track     = newTrack;
        return true;
    }
    catch(Instance::MCIError const &er)
    {
        LOG_AUDIO_ERROR("") << er.asText();
    }
    return false;
}
