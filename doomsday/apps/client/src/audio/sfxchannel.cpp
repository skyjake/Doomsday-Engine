/** @file sfxchannel.cpp  Logical sound channel (for sound effects).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "audio/sfxchannel.h"

#include "dd_main.h"     // remove me
#include "def_main.h"    // ::defs

// Debug visual headers:
#include "audio/s_cache.h"
#include "gl/gl_main.h"
#include "api_fontrender.h"
#include "render/rend_font.h"
#include "ui/ui_main.h"

#include <doomsday/world/thinkers.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/timer.h>    // TICSPERSEC
#include <de/legacy/vector1.h>  // remove me
#include <de/glinfo.h>
#include <de/list.h>
#include <de/log.h>

using namespace de;

namespace audio {

DE_PIMPL_NOREF(SfxChannel)
{
    dint flags = 0;                 ///< SFXCF_* flags.
    dfloat frequency = 0;           ///< Frequency adjustment: 1.0 is normal.
    dfloat volume = 0;              ///< Sound volume: 1.0 is max.

    const mobj_t *emitter = nullptr;///< Mobj emitter for the sound, if any (not owned).
    coord_t origin[3];              ///< Emit from here (synced with emitter).

    sfxbuffer_t *buffer = nullptr;  ///< Assigned sound buffer, if any (not owned).
    dint startTime = 0;             ///< When the assigned sound sample was last started.

    Impl() { zap(origin); }

    Vec3d findOrigin() const
    {
        // Originless sounds have no fixed/moveable emission point.
        if (flags & SFXCF_NO_ORIGIN)
        {
            return Vec3d();
        }

        // When tracking an emitter - use it's origin.
        if (emitter)
        {
            Vec3d point(emitter->origin);

            // Position on the Z axis at the map-object's center?
            if (Thinker_IsMobj(&emitter->thinker))
            {
                point.z += emitter->height / 2;
            }
            return point;
        }

        // Use the fixed origin.
        return Vec3d(origin);
    }

    inline void updateOrigin()
    {
        findOrigin().decompose(origin);
    }
};

SfxChannel::SfxChannel() : d(new Impl)
{}

SfxChannel::~SfxChannel()
{}

bool SfxChannel::hasBuffer() const
{
    return d->buffer != nullptr;
}

sfxbuffer_t &SfxChannel::buffer()
{
    if (d->buffer) return *d->buffer;
    /// @throw MissingBufferError  No sound buffer is currently assigned.
    throw MissingBufferError("SfxChannel::buffer", "No sound buffer is assigned");
}

const sfxbuffer_t &SfxChannel::buffer() const
{
    return const_cast<SfxChannel *>(this)->buffer();
}

void SfxChannel::setBuffer(sfxbuffer_t *newBuffer)
{
    d->buffer = newBuffer;
}

void SfxChannel::stop()
{
    if (!d->buffer) return;

    /// @todo AudioSystem should observe. -ds
    App_AudioSystem().sfx()->Stop(d->buffer);
}

dint SfxChannel::flags() const
{
    return d->flags;
}

/// @todo Use QFlags -ds
void SfxChannel::setFlags(dint newFlags)
{
    d->flags = newFlags;
}

dfloat SfxChannel::frequency() const
{
    return d->frequency;
}

void SfxChannel::setFrequency(dfloat newFrequency)
{
    d->frequency = newFrequency;
}

dfloat SfxChannel::volume() const
{
    return d->volume;
}

void SfxChannel::setVolume(dfloat newVolume)
{
    d->volume = newVolume;
}

const mobj_t *SfxChannel::emitter() const
{
    return d->emitter;
}

void SfxChannel::setEmitter(const mobj_t *newEmitter)
{
    d->emitter = newEmitter;
}

void SfxChannel::setFixedOrigin(const Vec3d &newOrigin)
{
    d->origin[0] = newOrigin.x;
    d->origin[1] = newOrigin.y;
    d->origin[2] = newOrigin.z;
}

Vec3d SfxChannel::origin() const
{
    return Vec3d(d->origin);
}

dfloat SfxChannel::priority() const
{
    if (!d->buffer || !(d->buffer->flags & SFXBF_PLAYING))
        return SFX_LOWEST_PRIORITY;

    if (d->flags & SFXCF_NO_ORIGIN)
        return App_AudioSystem().rateSoundPriority(0, 0, d->volume, d->startTime);

    // d->origin is set to emitter->xyz during updates.
    return App_AudioSystem().rateSoundPriority(0, d->origin, d->volume, d->startTime);
}

/// @todo AudioSystem should observe. -ds
void SfxChannel::updatePriority()
{
    // If no sound buffer is assigned we've no need to update.
    sfxbuffer_t *sbuf = d->buffer;
    if (!sbuf) return;

    // Disabled?
    if (d->flags & SFXCF_NO_UPDATE) return;

    // Update the sound origin if needed.
    if (d->emitter)
    {
        d->updateOrigin();
    }

    // Frequency is common to both 2D and 3D sounds.
    App_AudioSystem().sfx()->Set(sbuf, SFXBP_FREQUENCY, d->frequency);

    if (sbuf->flags & SFXBF_3D)
    {
        // Volume is affected only by maxvol.
        App_AudioSystem().sfx()->Set(sbuf, SFXBP_VOLUME, d->volume * ::sfxVolume / 255.0f);
        if (d->emitter && d->emitter == App_AudioSystem().sfxListener())
        {
            // Emitted by the listener object. Go to relative position mode
            // and set the position to (0,0,0).
            dfloat vec[3]; vec[0] = vec[1] = vec[2] = 0;
            App_AudioSystem().sfx()->Set(sbuf, SFXBP_RELATIVE_MODE, true);
            App_AudioSystem().sfx()->Setv(sbuf, SFXBP_POSITION, vec);
        }
        else
        {
            // Use the channel's map space origin.
            dfloat origin[3];
            V3f_Copyd(origin, d->origin);
            App_AudioSystem().sfx()->Set(sbuf, SFXBP_RELATIVE_MODE, false);
            App_AudioSystem().sfx()->Setv(sbuf, SFXBP_POSITION, origin);
        }

        // If the sound is emitted by the listener, speed is zero.
        if (d->emitter && d->emitter != App_AudioSystem().sfxListener() &&
            Thinker_IsMobj(&d->emitter->thinker))
        {
            dfloat vec[3];
            vec[0] = d->emitter->mom[0] * TICSPERSEC;
            vec[1] = d->emitter->mom[1] * TICSPERSEC;
            vec[2] = d->emitter->mom[2] * TICSPERSEC;
            App_AudioSystem().sfx()->Setv(sbuf, SFXBP_VELOCITY, vec);
        }
        else
        {
            // Not moving.
            dfloat vec[3]; vec[0] = vec[1] = vec[2] = 0;
            App_AudioSystem().sfx()->Setv(sbuf, SFXBP_VELOCITY, vec);
        }
    }
    else
    {
        dfloat dist = 0;
        dfloat pan  = 0;

        // This is a 2D buffer.
        if ((d->flags & SFXCF_NO_ORIGIN) ||
           (d->emitter && d->emitter == App_AudioSystem().sfxListener()))
        {
            dist = 1;
            pan = 0;
        }
        else
        {
            // Calculate roll-off attenuation. [.125/(.125+x), x=0..1]
            dist = Mobj_ApproxPointDistance(App_AudioSystem().sfxListener(), d->origin);
            if (dist < ::soundMinDist || (d->flags & SFXCF_NO_ATTENUATION))
            {
                // No distance attenuation.
                dist = 1;
            }
            else if (dist > ::soundMaxDist)
            {
                // Can't be heard.
                dist = 0;
            }
            else
            {
                const dfloat normdist = (dist - ::soundMinDist) / (::soundMaxDist - ::soundMinDist);

                // Apply the linear factor so that at max distance there
                // really is silence.
                dist = .125f / (.125f + normdist) * (1 - normdist);
            }

            // And pan, too. Calculate angle from listener to emitter.
            if (mobj_t *listener = App_AudioSystem().sfxListener())
            {
                dfloat angle = (M_PointToAngle2(listener->origin, d->origin) - listener->angle) / (dfloat) ANGLE_MAX * 360;

                // We want a signed angle.
                if (angle > 180)
                    angle -= 360;

                // Front half.
                if (angle <= 90 && angle >= -90)
                {
                    pan = -angle / 90;
                }
                else
                {
                    // Back half.
                    pan = (angle + (angle > 0 ? -180 : 180)) / 90;
                    // Dampen sounds coming from behind.
                    dist *= (1 + (pan > 0 ? pan : -pan)) / 2;
                }
            }
            else
            {
                // No listener mobj? Can't pan, then.
                pan = 0;
            }
        }

        App_AudioSystem().sfx()->Set(sbuf, SFXBP_VOLUME, d->volume * dist * ::sfxVolume / 255.0f);
        App_AudioSystem().sfx()->Set(sbuf, SFXBP_PAN, pan);
    }
}

int SfxChannel::startTime() const
{
    return d->startTime;
}

void SfxChannel::setStartTime(dint newStartTime)
{
    d->startTime = newStartTime;
}

DE_PIMPL(SfxChannels)
{
    List<SfxChannel *> all;

    Impl(Public *i) : Base(i) {}
    ~Impl() { clearAll(); }

    void clearAll()
    {
        deleteAll(all);
    }

    /// @todo support dynamically resizing in both directions. -ds
    void resize(dint newSize)
    {
        if (newSize < 0) newSize = 0;

        clearAll();
        for (dint i = 0; i < newSize; ++i)
        {
            all << new SfxChannel;
        }
    }
};

SfxChannels::SfxChannels(dint count) : d(new Impl(this))
{
    d->resize(count);
}

dint SfxChannels::count() const
{
    return d->all.count();
}

dint SfxChannels::countPlaying(dint id)
{
    DE_ASSERT( App_AudioSystem().sfxIsAvailable() );  // sanity check

    dint count = 0;
    forAll([&id, &count] (SfxChannel &ch)
    {
        if (ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();
            if ((sbuf.flags & SFXBF_PLAYING) && sbuf.sample && sbuf.sample->id == id)
            {
                count += 1;
            }
        }
        return LoopContinue;
    });
    return count;
}

SfxChannel *SfxChannels::tryFindVacant(bool use3D, dint bytes, dint rate, dint sampleId) const
{
    for (SfxChannel *ch : d->all)
    {
        if (!ch->hasBuffer()) continue;
        const sfxbuffer_t &sbuf = ch->buffer();

        if ((sbuf.flags & SFXBF_PLAYING)
           || use3D != ((sbuf.flags & SFXBF_3D) != 0)
           || sbuf.bytes != bytes
           || sbuf.rate  != rate)
            continue;

        // What about the sample?
        if (sampleId > 0)
        {
            if (!sbuf.sample || sbuf.sample->id != sampleId)
                continue;
        }
        else if (sampleId == 0)
        {
            // We're trying to find a channel with no sample already loaded.
            if (sbuf.sample)
                continue;
        }

        // This is perfect, take this!
        return ch;
    }

    return nullptr;  // None suitable.
}

void SfxChannels::refreshAll()
{
    forAll([] (SfxChannel &ch)
    {
        if (ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
        {
            App_AudioSystem().sfx()->Refresh(&ch.buffer());
        }
        return LoopContinue;
    });
}

LoopResult SfxChannels::forAll(const std::function<LoopResult (SfxChannel &)>& func) const
{
    for (SfxChannel *ch : d->all)
    {
        if (auto result = func(*ch)) return result;
    }
    return LoopContinue;
}

}  // namespace audio

using namespace audio;

// Debug visual: -----------------------------------------------------------------

dint showSoundInfo;
byte refMonitor;

void Sfx_ChannelDrawer()
{
    if (!::showSoundInfo) return;

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(1, 1, 0, 1);

    const dint lh = FR_SingleLineHeight("Q");
    if (!App_AudioSystem().sfxIsAvailable())
    {
        FR_DrawTextXY("Sfx disabled", 0, 0);
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    if (::refMonitor)
        FR_DrawTextXY("!", 0, 0);

    // Sample cache information.
    duint cachesize, ccnt;
    App_AudioSystem().sfxSampleCache().info(&cachesize, &ccnt);
    char buf[200]; sprintf(buf, "Cached:%i (%i)", cachesize, ccnt);

    FR_SetColor(1, 1, 1);
    FR_DrawTextXY(buf, 10, 0);

    // Print a line of info about each channel.
    dint idx = 0;
    App_AudioSystem().sfxChannels().forAll([&lh, &idx] (audio::SfxChannel &ch)
    {
        if (ch.hasBuffer() && (ch.buffer().flags & SFXBF_PLAYING))
        {
            FR_SetColor(1, 1, 1);
        }
        else
        {
            FR_SetColor(1, 1, 0);
        }

        Block emitterText;
        if (ch.emitter())
        {
            emitterText = (  " mobj:" + String::asText(ch.emitter()->thinker.id)
                           + " pos:"  + ch.origin().asText()
                          ).toLatin1();
        }

        char buf[200];
        sprintf(buf, "%02i: %c%c%c v=%3.1f f=%3.3f st=%i et=%u%s",
                idx,
                !(ch.flags() & SFXCF_NO_ORIGIN     ) ? 'O' : '.',
                !(ch.flags() & SFXCF_NO_ATTENUATION) ? 'A' : '.',
                ch.emitter() ? 'E' : '.',
                ch.volume(), ch.frequency(), ch.startTime(),
                ch.hasBuffer() ? ch.buffer().endTime : 0,
                emitterText.constData());
        FR_DrawTextXY(buf, 5, lh * (1 + idx * 2));

        if (ch.hasBuffer())
        {
            sfxbuffer_t &sbuf = ch.buffer();

            sprintf(buf, "    %c%c%c%c id=%03i/%-8s ln=%05i b=%i rt=%2i bs=%05i (C%05i/W%05i)",
                    (sbuf.flags & SFXBF_3D     ) ? '3' : '.',
                    (sbuf.flags & SFXBF_PLAYING) ? 'P' : '.',
                    (sbuf.flags & SFXBF_REPEAT ) ? 'R' : '.',
                    (sbuf.flags & SFXBF_RELOAD ) ? 'L' : '.',
                    sbuf.sample ? sbuf.sample->id : 0,
                    sbuf.sample ? DED_Definitions()->sounds[sbuf.sample->id].id : "",
                    sbuf.sample ? sbuf.sample->size : 0,
                    sbuf.bytes, sbuf.rate / 1000, sbuf.length,
                    sbuf.cursor, sbuf.written);
            FR_DrawTextXY(buf, 5, lh * (2 + idx * 2));
        }

        idx += 1;
        return LoopContinue;
    });

    DGL_Disable(DGL_TEXTURE_2D);

    // Back to the original.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}
