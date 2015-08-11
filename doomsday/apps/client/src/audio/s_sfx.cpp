/** @file s_sfx.cpp  Sound Effects.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de_base.h"
#include "audio/s_sfx.h"

#include <de/concurrency.h>
#include <de/timer.h>
#include <de/vector1.h>
#include <de/Log>
#include "dd_share.h"  // SF_* flags

#ifdef __CLIENT__
#  include "sys_system.h"  // Sys_Sleep()
#endif

#include "audio/audiodriver.h"
#include "audio/s_cache.h"
#include "audio/sfxchannel.h"

#include "world/thinkers.h"
#include "Sector"
#include "SectorCluster"

using namespace de;

#ifdef __CLIENT__
using audio::SfxChannel;
using audio::SfxChannels;
#endif

dd_bool sfxAvail;
dfloat sfxReverbStrength = .5f;

// Console variables:
dint sfx3D;
dint sfx16Bit;
dint sfxSampleRate = 11025;

static mobj_t *listener;
static SectorCluster *listenerCluster;

static thread_t refreshHandle;
static volatile dd_bool allowRefresh, refreshing;

void Sfx_UpdateReverb()
{
    ::listenerCluster = nullptr;
}

#ifdef __CLIENT__

/**
 * This is a high-priority thread that periodically checks if the channels
 * need to be updated with more data. The thread terminates when it notices
 * that the channels have been destroyed. The Sfx audio driver maintains a 250ms
 * buffer for each channel, which means the refresh must be done often
 * enough to keep them filled.
 *
 * @todo Use a real mutex, will you?
 */
dint C_DECL Sfx_ChannelRefreshThread(void *)
{
    // We'll continue looping until the Sfx module is shut down.
    while(::sfxAvail && App_AudioSystem().hasSfxChannels())
    {
        // The bit is swapped on each refresh (debug info).
        ::refMonitor ^= 1;

        if(allowRefresh)
        {
            // Do the refresh.
            ::refreshing = true;
            App_AudioSystem().sfxChannels().forAll([] (SfxChannel &ch)
            {
                if(ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
                {
                    App_AudioSystem().sfx()->Refresh(&ch.buffer());
                }
                return LoopContinue;
            });

            refreshing = false;
            // Let's take a nap.
            Sys_Sleep(200);
        }
        else
        {
            // Refreshing is not allowed, so take a shorter nap while
            // waiting for allowRefresh.
            Sys_Sleep(150);
        }
    }

    // Time to end this thread.
    return 0;
}

#endif // __CLIENT__

void Sfx_AllowRefresh(dd_bool allow)
{
    if(!sfxAvail) return;
    if(allowRefresh == allow) return; // No change.

    allowRefresh = allow;

    // If we're denying refresh, let's make sure that if it's currently
    // running, we don't continue until it has stopped.
    if(!allow)
    {
        while(refreshing)
        {
            Sys_Sleep(0);
        }
    }

    // Sys_SuspendThread(refreshHandle, !allow);
}

void Sfx_StopSoundGroup(dint group, mobj_t *emitter)
{
    if(!::sfxAvail) return;
    App_AudioSystem().sfxChannels().forAll([&group, &emitter] (SfxChannel &ch)
    {
        if(ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();
            if((sbuf.flags & SFXBF_PLAYING) &&
               (sbuf.sample->group == group && (!emitter || ch.emitter() == emitter)))
            {
                // This channel must stop.
                App_AudioSystem().sfx()->Stop(&sbuf);
            }
        }
        return LoopContinue;
    });
}

dint Sfx_StopSound(dint id, mobj_t *emitter)
{
    return Sfx_StopSoundWithLowerPriority(id, emitter, -1);
}

dint Sfx_StopSoundWithLowerPriority(dint id, mobj_t *emitter, dint defPriority)
{
    if(!::sfxAvail) return false;

    dint stopCount = 0;
    App_AudioSystem().sfxChannels().forAll([&id, &emitter, &defPriority, &stopCount] (SfxChannel &ch)
    {
        if(!ch.hasBuffer()) return LoopContinue;
        sfxbuffer_t &sbuf = ch.buffer();

        if(!(sbuf.flags & SFXBF_PLAYING) || (id && sbuf.sample->id != id) ||
           (emitter && ch.emitter() != emitter))
        {
            return LoopContinue;
        }

        // Can it be stopped?
        if(sbuf.flags & SFXBF_DONT_STOP)
        {
            // The emitter might get destroyed...
            ch.setEmitter(nullptr);
            ch.setFlags(ch.flags() | (SFXCF_NO_UPDATE | SFXCF_NO_ORIGIN));
            return LoopContinue;
        }

        // Check the priority.
        if(defPriority >= 0)
        {
            dint oldPrio = ::defs.sounds[sbuf.sample->id].priority;
            if(oldPrio < defPriority)  // Old is more important.
            {
                stopCount = -1;
                return LoopAbort;
            }
        }

        // This channel must be stopped!
        /// @todo audio::System should observe. -ds
        App_AudioSystem().sfx()->Stop(&sbuf);
        ++stopCount;
        return LoopContinue;
    });

    return stopCount;
}

#if 0 // Currently unused.
dint Sfx_IsPlaying(dint id, mobj_t *emitter)
{
    if(!::sfxAvail) return false;

    return App_AudioSystem().sfxChannels().forAll([&id, &emitter] (SfxChannel &ch)
    {
        if(ch.hasSample())
        {
            sfxbuffer_t &sbuf = ch.sample();

            if(!(sbuf.flags & SFXBF_PLAYING) ||
               ch.emitter() != emitter ||
               id && sbuf.sample->id != id)
            {
                return LoopContinue;
            }

            // Once playing, repeating sounds don't stop.
            if(sbuf.flags & SFXBF_REPEAT)
                return LoopAbort;

            // Check time. The flag is updated after a slight delay (only at refresh).
            if(Sys_GetTime() - ch->startTime() < sbuf.sample->numsamples / (dfloat) sbuf.freq * TICSPERSEC)
            {
                return LoopAbort;
            }
        }
        return LoopContinue;
    });
}
#endif

void Sfx_UnloadSoundID(dint id)
{
    if(!::sfxAvail) return;

    BEGIN_COP;
    App_AudioSystem().sfxChannels().forAll([&id] (SfxChannel &ch)
    {
        if(ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();
            if(sbuf.sample && sbuf.sample->id == id)
            {
                // Stop and unload.
                App_AudioSystem().sfx()->Reset(&sbuf);
            }
        }
        return LoopContinue;
    });
    END_COP;
}

dint Sfx_CountPlaying(dint id)
{
    if(!::sfxAvail) return 0;

    dint count = 0;
    App_AudioSystem().sfxChannels().forAll([&id, &count] (SfxChannel &ch)
    {
        if(ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();
            if((sbuf.flags & SFXBF_PLAYING) && sbuf.sample && sbuf.sample->id == id)
            {
                count += 1;
            }
        }
        return LoopContinue;
    });
    return count;
}

mobj_t *Sfx_Listener()
{
    return ::listener;
}

void Sfx_SetListener(mobj_t *mobj)
{
    ::listener = mobj;
}

/**
 * Returns the actual 3D coordinates of the listener.
 */
void Sfx_GetListenerXYZ(dfloat *origin)
{
    DENG2_ASSERT(origin);

    if(!::listener) return;

    /// @todo Make it exactly eye-level! (viewheight).
    origin[0] = ::listener->origin[0];
    origin[1] = ::listener->origin[1];
    origin[2] = ::listener->origin[2] + ::listener->height - 5;
}

void Sfx_ListenerUpdate()
{
    if(!::sfxAvail || !::sfx3D) return;

    // No volume means no sound.
    if(!::sfxVolume) return;

    // Update the listener mobj.
    Sfx_SetListener(S_GetListenerMobj());
    if(::listener)
    {
        // Position. At eye-level.
        dfloat vec[4]; Sfx_GetListenerXYZ(vec);
        App_AudioSystem().sfx()->Listenerv(SFXLP_POSITION, vec);

        // Orientation. (0,0) will produce front=(1,0,0) and up=(0,0,1).
        vec[0] = ::listener->angle / (dfloat) ANGLE_MAX *360;

        vec[1] = (::listener->dPlayer ? LOOKDIR2DEG(::listener->dPlayer->lookDir) : 0);
        App_AudioSystem().sfx()->Listenerv(SFXLP_ORIENTATION, vec);

        // Velocity. The unit is world distance units per second.
        vec[0] = listener->mom[0] * TICSPERSEC;
        vec[1] = listener->mom[1] * TICSPERSEC;
        vec[2] = listener->mom[2] * TICSPERSEC;
        App_AudioSystem().sfx()->Listenerv(SFXLP_VELOCITY, vec);

        // Reverb effects. Has the current sector cluster changed?
        SectorCluster *newCluster = Mobj_ClusterPtr(*listener);
        if(newCluster && (!::listenerCluster || ::listenerCluster != newCluster))
        {
            ::listenerCluster = newCluster;

            // It may be necessary to recalculate the reverb properties...
            AudioEnvironmentFactors const &envFactors = listenerCluster->reverb();

            for(dint i = 0; i < NUM_REVERB_DATA; ++i)
            {
                vec[i] = envFactors[i];
                if(i == SRD_VOLUME)
                {
                    vec[i] *= ::sfxReverbStrength;
                }
            }

            App_AudioSystem().sfx()->Listenerv(SFXLP_REVERB, vec);
        }
    }

    // Update all listener properties.
    App_AudioSystem().sfx()->Listener(SFXLP_UPDATE, 0);
}

void Sfx_ListenerNoReverb()
{
    if(!::sfxAvail) return;

    ::listenerCluster = nullptr;

    dfloat rev[4] = { 0, 0, 0, 0 };
    App_AudioSystem().sfx()->Listenerv(SFXLP_REVERB, rev);
    App_AudioSystem().sfx()->Listener(SFXLP_UPDATE, 0);
}

void Sfx_GetChannelPriorities(dfloat *prios)
{
    if(!prios) return;

    dint idx = 0;
    App_AudioSystem().sfxChannels().forAll([&prios, &idx] (SfxChannel &ch)
    {
        prios[idx++] = ch.priority();
        return LoopContinue;
    });
}

dfloat Sfx_Priority(mobj_t *emitter, coord_t const *point, dfloat volume, dint startTic)
{
    // In five seconds all priority of a sound is gone.
    dfloat const timeoff = 1000 * (Timer_Ticks() - startTic) / (5.0f * TICSPERSEC);

    if(!listener || (!emitter && !point))
    {
        // The sound does not have an origin.
        return 1000 * volume - timeoff;
    }

    // The sound has an origin, base the points on distance.
    coord_t const *origin;
    if(emitter)
    {
        origin = emitter->origin;
    }
    else
    {
        // No emitter mobj, use the fixed source position.
        origin = point;
    }

    return 1000 * volume - Mobj_ApproxPointDistance(listener, origin) / 2 - timeoff;
}

dint Sfx_StartSound(sfxsample_t *sample, dfloat volume, dfloat freq, mobj_t *emitter,
    coord_t *fixedOrigin, dint flags)
{
    DENG2_ASSERT(sample);
    LOG_AS("Sfx_StartSound");

    bool const play3D = ::sfx3D && (emitter || fixedOrigin);

    if(!::sfxAvail) return false;

    if(sample->id < 1 || sample->id >= ::defs.sounds.size()) return false;
    if(volume <= 0 || !sample->size) return false;

    if(emitter && ::sfxOneSoundPerEmitter)
    {
        // Stop any other sounds from the same emitter.
        if(Sfx_StopSoundWithLowerPriority(0, emitter, ::defs.sounds[sample->id].priority) < 0)
        {
            // Something with a higher priority is playing, can't start now.
            LOG_AUDIO_MSG("Cannot start ID %i (prio%i), overridden (emitter %i)")
                << sample->id
                << ::defs.sounds[sample->id].priority
                << emitter->thinker.id;
            return false;
        }
    }

    // Calculate the new sound's priority.
    dint const nowTime  = Timer_Ticks();
    dfloat const myPrio = Sfx_Priority(emitter, fixedOrigin, volume, nowTime);

    bool haveChannelPrios = false;
    dfloat channelPrios[256/*MAX_CHANNEL_COUNT*/];
    dfloat lowPrio = 0;

    // Ensure there aren't already too many channels playing this sample.
    sfxinfo_t *info = &::runtimeDefs.sounds[sample->id];
    if(info->channels > 0)
    {
        // The decision to stop channels is based on priorities.
        Sfx_GetChannelPriorities(channelPrios);
        haveChannelPrios = true;

        dint count = Sfx_CountPlaying(sample->id);
        while(count >= info->channels)
        {
            // Stop the lowest priority sound of the playing instances, again
            // noting sounds that are more important than us.
            dint idx = 0;
            SfxChannel *selCh = nullptr;
            App_AudioSystem().sfxChannels().forAll([&sample, &myPrio, &channelPrios,
                                   &selCh, &lowPrio, &idx] (SfxChannel &ch)
            {
                dfloat const chPriority = channelPrios[idx++];

                if(ch.hasBuffer())
                {
                    sfxbuffer_t &sbuf = ch.buffer();
                    if((sbuf.flags & SFXBF_PLAYING))
                    {
                        DENG2_ASSERT(sbuf.sample != nullptr);

                        if(sbuf.sample->id == sample->id &&
                           (myPrio >= chPriority && (!selCh || chPriority <= lowPrio)))
                        {
                            selCh   = &ch;
                            lowPrio = chPriority;
                        }
                    }
                }

                return LoopContinue;
            });

            if(!selCh)
            {
                // The new sound can't be played because we were unable to stop
                // enough channels to accommodate the limitation.
                LOG_AUDIO_XVERBOSE("Not playing #%i because all channels are busy")
                    << sample->id;
                return false;
            }

            // Stop this one.
            count--;
            selCh->stop();
        }
    }

    // Hit count tells how many times the cached sound has been used.
    App_AudioSystem().sfxSampleCache().hit(sample->id);

    /*
     * Pick a channel for the sound. We will do our best to play the sound,
     * cancelling existing ones if need be. The ideal choice is a free channel
     * that is already loaded with the sample, in the correct format and mode.
     */
    BEGIN_COP;

    // First look through the stopped channels. At this stage we're very picky:
    // only the perfect choice will be good enough.
    SfxChannel *selCh = App_AudioSystem().sfxChannels().tryFindVacant(play3D, sample->bytesPer,
                                                     sample->rate, sample->id);

    if(!selCh)
    {
        // Perhaps there is a vacant channel (with any sample, but preferably one
        // with no sample already loaded).
        selCh = App_AudioSystem().sfxChannels().tryFindVacant(play3D, sample->bytesPer, sample->rate, 0);
    }

    if(!selCh)
    {
        // Try any non-playing channel in the correct format.
        selCh = App_AudioSystem().sfxChannels().tryFindVacant(play3D, sample->bytesPer, sample->rate, -1);
    }

    if(!selCh)
    {
        // A perfect channel could not be found.
        // We must use a channel with the wrong format or decide which one of the
        // playing ones gets stopped.

        if(!haveChannelPrios)
        {
            Sfx_GetChannelPriorities(channelPrios);
        }

        // All channels with a priority less than or equal to ours can be stopped.
        SfxChannel *prioCh = nullptr;
        dint idx = 0;
        App_AudioSystem().sfxChannels().forAll([&play3D, &myPrio, &channelPrios,
                               &selCh, &prioCh, &lowPrio, &idx] (SfxChannel &ch)
        {
            dfloat const chPriority = channelPrios[idx++];

            if(ch.hasBuffer())
            {
                // Sample buffer must be configured for the right mode.
                sfxbuffer_t &sbuf = ch.buffer();
                if(play3D == ((sbuf.flags & SFXBF_3D) != 0))
                {
                    if(!(sbuf.flags & SFXBF_PLAYING))
                    {
                        // This channel is not playing, we'll take it!
                        selCh = &ch;
                        return LoopAbort;
                    }

                    // Are we more important than this sound?
                    // We want to choose the lowest priority sound.
                    if(myPrio >= chPriority && (!prioCh || chPriority <= lowPrio))
                    {
                        prioCh  = &ch;
                        lowPrio = chPriority;
                    }
                }
            }

            return LoopContinue;
        });

        // If a good low-priority channel was found, use it.
        if(prioCh)
        {
            selCh = prioCh;
            selCh->stop();
        }
    }

    if(!selCh)
    {
        // A suitable channel was not found.
        END_COP;
        LOG_AUDIO_XVERBOSE("Failed to find suitable channel for sample %i") << sample->id;
        return false;
    }

    DENG2_ASSERT(selCh->hasBuffer());
    // The sample buffer may need to be reformatted.

    if(selCh->buffer().rate  != sample->rate ||
       selCh->buffer().bytes != sample->bytesPer)
    {
        // Create a new sample buffer with the correct format.
        App_AudioSystem().sfx()->Destroy(&selCh->buffer());
        selCh->setBuffer(App_AudioSystem().sfx()->Create(play3D ? SFXBF_3D : 0, sample->bytesPer * 8, sample->rate));
    }
    sfxbuffer_t &sbuf = selCh->buffer();

    // Configure buffer flags.
    sbuf.flags &= ~(SFXBF_REPEAT | SFXBF_DONT_STOP);
    if(flags & SF_REPEAT)    sbuf.flags |= SFXBF_REPEAT;
    if(flags & SF_DONT_STOP) sbuf.flags |= SFXBF_DONT_STOP;

    // Init the channel information.
    selCh->setFlags(selCh->flags() & ~(SFXCF_NO_ORIGIN | SFXCF_NO_ATTENUATION | SFXCF_NO_UPDATE));
    selCh->setVolume(volume);
    selCh->setFrequency(freq);

    if(!emitter && !fixedOrigin)
    {
        selCh->setFlags(selCh->flags() | SFXCF_NO_ORIGIN);
        selCh->setEmitter(nullptr);
    }
    else
    {
        selCh->setEmitter(emitter);
        if(fixedOrigin)
        {
            selCh->setFixedOrigin(Vector3d(fixedOrigin));
        }
    }

    if(flags & SF_NO_ATTENUATION)
    {   
        // The sound can be heard from any distance.
        selCh->setFlags(selCh->flags() | SFXCF_NO_ATTENUATION);
    }

    /**
     * Load in the sample. Must load prior to setting properties, because the audio driver
     * might actually create the real buffer only upon loading.
     *
     * @note The sample is not reloaded if a sample with the same ID is already loaded on
     * the channel.
     */
    if(!sbuf.sample || sbuf.sample->id != sample->id)
    {
        App_AudioSystem().sfx()->Load(&sbuf, sample);
    }

    // Update channel properties.
    selCh->updatePriority();

    // 3D sounds need a few extra properties set up.
    if(play3D)
    {
        // Init the buffer's min/max distances.
        // This is only done once, when the sound is started (i.e., here).
        App_AudioSystem().sfx()->Set(&sbuf, SFXBP_MIN_DISTANCE,
                                     (selCh->flags() & SFXCF_NO_ATTENUATION)? 10000 :
                                     ::soundMinDist);

        App_AudioSystem().sfx()->Set(&sbuf, SFXBP_MAX_DISTANCE,
                                     (selCh->flags() & SFXCF_NO_ATTENUATION)? 20000 :
                                     ::soundMaxDist);
    }

    // This'll commit all the deferred properties.
    App_AudioSystem().sfx()->Listener(SFXLP_UPDATE, 0);

    // Start playing.
    App_AudioSystem().sfx()->Play(&sbuf);

    END_COP;

    // Take note of the start time.
    selCh->setStartTime(nowTime);

    // Sound successfully started.
    return true;
}

void Sfx_Update()
{
    // If the display player doesn't have a mobj, no positioning is done.
    ::listener = S_GetListenerMobj();

    // Update channels.
    App_AudioSystem().sfxChannels().forAll([] (SfxChannel &ch)
    {
        if(ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
        {
            ch.updatePriority();
        }
        return LoopContinue;
    });

    // Update listener.
    Sfx_ListenerUpdate();
}

/**
 * Start the sound channel refresh thread. It will stop on its own when it notices that
 * the rest of the sound system is going down.
 */
void Sfx_StartRefresh()
{
    LOG_AS("Sfx_StartRefresh");

    dint disableRefresh = false;

    ::refreshing   = false;
    ::allowRefresh = true;

    // Nothing to refresh?
    if(!App_AudioSystem().sfx())
        goto noRefresh;

    if(App_AudioSystem().sfx()->Getv)
    {
        App_AudioSystem().sfx()->Getv(SFXIP_DISABLE_CHANNEL_REFRESH, &disableRefresh);
    }

    if(!disableRefresh)
    {
        // Start the refresh thread. It will run until the Sfx module is shut down.
        refreshHandle = Sys_StartThread(Sfx_ChannelRefreshThread, nullptr);
        if(!refreshHandle)
        {
            throw Error("Sfx_StartRefresh", "Failed to start refresh thread");
        }
    }
    else
    {
noRefresh:
        LOGDEV_AUDIO_NOTE("Audio driver does not require a refresh thread");
    }
}

bool Sfx_Init()
{
    // Already initialized?
    if(::sfxAvail) return true;

    // Check if sound has been disabled with a command line option.
    if(CommandLine_Exists("-nosfx"))
    {
        LOG_AUDIO_NOTE("Sound Effects disabled");
        return true;
    }

    LOG_AUDIO_VERBOSE("Initializing Sound Effects subsystem...");

    // No interface for SFX playback?
    if(!App_AudioSystem().sfx()) return false;

    // This is based on the scientific calculations that if the DOOM marine
    // is 56 units tall, 60 is about two meters.
    //// @todo Derive from the viewheight.
    App_AudioSystem().sfx()->Listener(SFXLP_UNITS_PER_METER, 30);
    App_AudioSystem().sfx()->Listener(SFXLP_DOPPLER, 1.5f);

    // The audio driver is working, let's create the channels.
    App_AudioSystem().initSfxChannels();

    // (Re)Init the sample cache.
    App_AudioSystem().sfxSampleCache().clear();

    // The Sfx module is now available.
    ::sfxAvail = true;

    // Initialize reverb effects to off.
    Sfx_ListenerNoReverb();

    // Finally, start the refresh thread.
    Sfx_StartRefresh();
    return true;
}

void Sfx_Shutdown()
{
    // Not initialized?
    if(!::sfxAvail) return;

    // These will stop further refreshing.
    ::sfxAvail     = false;
    ::allowRefresh = false;

    if(refreshHandle)
    {
        // Wait for the sfx refresh thread to stop.
        Sys_WaitThread(refreshHandle, 2000, nullptr);
        refreshHandle = nullptr;
    }

    // Clear the sample cache.
    App_AudioSystem().sfxSampleCache().clear();

    // Destroy channels.
    App_AudioSystem().shutdownSfxChannels();
}

void Sfx_Reset()
{
    if(!::sfxAvail) return;

    ::listenerCluster = nullptr;

    // Stop all channels.
    App_AudioSystem().sfxChannels().forAll([] (SfxChannel &ch)
    {
        ch.stop();
        return LoopContinue;
    });

    // Clear the sample cache.
    App_AudioSystem().sfxSampleCache().clear();
}

void Sfx_3DMode(dd_bool activate)
{
    static dint old3DMode = false;

    if(old3DMode == activate)
        return; // No change; do nothing.

    ::sfx3D = old3DMode = activate;
    // To make the change effective, re-create all channels.
    App_AudioSystem().recreateSfxChannels();

    // If going to 2D, make sure the reverb is off.
    if(!::sfx3D)
    {
        Sfx_ListenerNoReverb();
    }
}

void Sfx_SampleFormat(dint newBits, dint newRate)
{
    if(::sfxBits == newBits && ::sfxRate == newRate)
        return;  // No change; do nothing.

    // Set the new buffer format.
    ::sfxBits = newBits;
    ::sfxRate = newRate;
    App_AudioSystem().recreateSfxChannels();

    // The cache just became useless, clear it.
    App_AudioSystem().sfxSampleCache().clear();
}

void Sfx_MapChange()
{
    // Mobjs are about to be destroyed so stop all sound channels using one as an emitter.
    App_AudioSystem().sfxChannels().forAll([] (SfxChannel &ch)
    {
        if(ch.emitter())
        {
            ch.setEmitter(nullptr);
            ch.stop();
        }
        return LoopContinue;
    });

    // Sectors, too, for that matter.
    ::listenerCluster = nullptr;
}
