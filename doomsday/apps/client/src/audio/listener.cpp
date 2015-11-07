/** @file listener.cpp  Logical model of "listener" in an audio sound "stage".
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "audio/listener.h"

#include "world/map.h"
#include "world/p_object.h"
#include "Sector"
#include "SectorCluster"

#include "clientapp.h"

#include <doomsday/console/var.h>
#include <de/timer.h>  // TICSPERSEC

using namespace de;

namespace audio {

static Ranged const volumeAttenuationRange(256, 2025);  ///< @todo should be cvars.

static dfloat reverbStrength = 0.5f;  ///< cvar

DENG2_PIMPL(Listener)
, DENG2_OBSERVES(Map,           MapObjectBspLeafChange)
, DENG2_OBSERVES(Deletable,     Deletion)
, DENG2_OBSERVES(SectorCluster, AudioEnvironmentChange)
, DENG2_OBSERVES(SectorCluster, Deletion)
{
    bool useEnvironment    = false;
    mobj_t *tracking       = nullptr;
    SectorCluster *cluster = nullptr;

    Instance(Public *i) : Base(i)
    {}

    void notifyEnvironmentChanged()
    {
        // Notify interested parties.
        DENG2_FOR_PUBLIC_AUDIENCE2(EnvironmentChange, i) i->listenerEnvironmentChanged(self);
    }

    void observeCluster(SectorCluster *newCluster)
    {
        // No change?
        if(cluster == newCluster) return;

        if(cluster)
        {
            cluster->audienceForDeletion()               -= this;
            cluster->audienceForAudioEnvironmentChange() -= this;
        }

        cluster = newCluster;

        if(cluster && useEnvironment)
        {
            cluster->audienceForAudioEnvironmentChange() += this;
            cluster->audienceForDeletion()               += this;
        }

        notifyEnvironmentChanged();
    }

    /// @todo MapObject should produce the notification we want. -ds
    void mapObjectBspLeafChanged(mobj_t &mob)
    {
        // Ignore if we aren't tracking this particular map-object.
        if(tracking != &mob) return;

        observeCluster(useEnvironment ? Mobj_ClusterPtr(mob) : nullptr);
    }

    /// @todo MapObject should produce the notification we actually want. -ds
    void objectWasDeleted(Deletable *)
    {
        if(tracking)
        {
            tracking = nullptr;
            cluster  = nullptr;

            if(useEnvironment)
            {
                notifyEnvironmentChanged();
            }
        }
    }

    void sectorClusterAudioEnvironmentChanged(SectorCluster &changed)
    {
        DENG2_ASSERT(useEnvironment && tracking && cluster == &changed);
        DENG2_UNUSED(changed);
        notifyEnvironmentChanged();
    }

    void sectorClusterBeingDeleted(SectorCluster const &deleting)
    {
        DENG2_ASSERT(useEnvironment && tracking && cluster == &deleting);
        DENG2_UNUSED(deleting);
        observeCluster(nullptr);
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(EnvironmentChange)
};

DENG2_AUDIENCE_METHOD(Listener, Deletion)
DENG2_AUDIENCE_METHOD(Listener, EnvironmentChange)

Listener::Listener() : d(new Instance(this))
{}

Listener::~Listener()
{
    // Notify interested parties.
    DENG2_FOR_AUDIENCE2(Deletion, i) i->listenerBeingDeleted(*this);
}

Environment Listener::environment()
{
    LOG_AS("audio::Listener");
    if(d->cluster)
    {
        DENG2_ASSERT(d->useEnvironment);

        // It may be necessary to recalculate the Environment (cached).
        Environment env = d->cluster->audioEnvironment();

        // Apply the global reverb strength factor.
        env.volume *= reverbStrength;

        return env;
    }
    return Environment();
}

Vector2d Listener::orientation() const
{
    if(mobj_t const *mob = d->tracking)
    {
        return Vector2d(mob->angle / (dfloat) ANGLE_MAX * 360,
                        (mob->dPlayer ? LOOKDIR2DEG(mob->dPlayer->lookDir) : 0));
    }
    return Vector2d();  // No rotation.
}

Vector3d Listener::position() const
{
    if(mobj_t const *mob = d->tracking)
    {
        Vector3d origin(mob->origin);
        origin.z += mob->height - 5;  /// @todo Make it exactly eye-level! (viewheight).
        return origin;
    }
    return Vector3d();  // No translation.
}

Vector3d Listener::velocity() const
{
    if(mobj_t const *mob = d->tracking)
    {
        return mob->mom;
    }
    return Vector3d();  // Not moving.
}

dfloat Listener::angleFrom(Vector3d const &point) const
{
    ddouble origin[3]; position().decompose(origin);
    ddouble pointv[3]; point     .decompose(pointv);

    angle_t angle = M_PointToAngle2(origin, pointv);
    if(d->tracking)
    {
        angle -= d->tracking->angle;
    }

    return (angle) / (dfloat) ANGLE_MAX * 360;
}

ddouble Listener::distanceFrom(Vector3d const &point) const
{
    if(mobj_t const *mob = d->tracking)
    {
        return Mobj_ApproxPointDistance(*mob, point);
    }
    return 0;
}

dfloat Listener::rateSoundPriority(dint startTime, dfloat volume, SoundFlags flags,
    Vector3d const &origin)
{
    // Deminish the rating over five seconds from the start time until zero.
    dfloat const timeoff = 1000 * (Timer_Ticks() - startTime) / (5.0f * TICSPERSEC);

    // Rate sounds without an origin simply by playback volume.
    if(!d->tracking || flags.testFlag(SoundFlag::NoOrigin))
    {
        return 1000 * volume - timeoff;
    }
    // Rate sounds with an origin by both distance and playback volume.
    else
    {
        return 1000 * volume - distanceFrom(origin) / 2 - timeoff;
    }
}

Ranged Listener::volumeAttenuationRange() const
{
    return ::audio::volumeAttenuationRange;
}

mobj_t const *Listener::trackedMapObject() const
{
    return d->tracking;
}

void Listener::setTrackedMapObject(mobj_t *mapObjectToTrack)
{
    // No change?
    if(d->tracking == mapObjectToTrack) return;

    if(d->tracking)
    {
        Mob_Map(*d->tracking).audienceForDeletion -= d;
        if(d->useEnvironment)
        {
            Mob_Map(*d->tracking).audienceForMapObjectBspLeafChange() -= d;
        }
    }

    d->tracking = mapObjectToTrack;

    if(d->tracking)
    {
        if(d->useEnvironment)
        {
            Mob_Map(*d->tracking).audienceForMapObjectBspLeafChange() += d;
        }
        Mob_Map(*d->tracking).audienceForDeletion += d;
    }

    d->observeCluster(d->useEnvironment && d->tracking ? Mobj_ClusterPtr(*d->tracking) : nullptr);
}

void Listener::useEnvironment(bool enableOrDisable)
{
    if(d->useEnvironment != enableOrDisable)
    {
        d->useEnvironment = enableOrDisable;
        d->observeCluster(d->useEnvironment && d->tracking ? Mobj_ClusterPtr(*d->tracking) : nullptr);
    }
}

void Listener::requestEnvironmentUpdate()
{
    if(!d->useEnvironment) return;

    d->notifyEnvironmentChanged();
}

static void reverbStrengthChanged()
{
    if(ClientApp::hasAudioSystem())
    {
        /// @todo Fixme: Listener should handle this internally. -ds
        ClientApp::audioSystem().worldStage().listener().requestEnvironmentUpdate();
    }
}

void Listener::consoleRegister()  // static
{
    C_VAR_FLOAT2("sound-reverb-volume", &reverbStrength, 0, 0, 1.5f, reverbStrengthChanged);
}

}  // namespace audio
