/** @file clientmobjthinkerdata.h  Private client-side data for mobjs.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_WORLD_CLIENTMOBJTHINKERDATA_H
#define DE_WORLD_CLIENTMOBJTHINKERDATA_H

#include <doomsday/world/mobjthinkerdata.h>
#include <de/legacy/timer.h>
#include <de/modeldrawable.h>
#include <de/glstate.h>

#include "render/modelrenderer.h"

/**
 * @defgroup clMobjFlags Client Mobj Flags
 * @ingroup flags
 */
///@{
#define CLMF_HIDDEN         0x01 ///< Not officially created yet
#define CLMF_UNPREDICTABLE  0x02 ///< Temporarily hidden (until next delta)
#define CLMF_SOUND          0x04 ///< Sound is queued for playing on unhide.
#define CLMF_NULLED         0x08 ///< Once nulled, it can't be updated.
#define CLMF_STICK_FLOOR    0x10 ///< Mobj will stick to the floor.
#define CLMF_STICK_CEILING  0x20 ///< Mobj will stick to the ceiling.
#define CLMF_LOCAL_ACTIONS  0x40 ///< Allow local action execution.
///@}

// Clmobj knowledge flags. This keeps track of the information that has been
// received.
#define CLMF_KNOWN_X        0x10000
#define CLMF_KNOWN_Y        0x20000
#define CLMF_KNOWN_Z        0x40000
#define CLMF_KNOWN_STATE    0x80000
#define CLMF_KNOWN          0xf0000 ///< combination of all the KNOWN-flags

// Magic number for client mobj information.
//#define CLM_MAGIC1          0xdecafed1
//#define CLM_MAGIC2          0xcafedeb8

namespace render { class StateAnimator; }

/**
 * Private client-side data for mobjs. This includes any per-object state for rendering
 * and client-side network state.
 *
 * @todo Lumobj objects (light source interfaces) should be moved into this class, so
 * that they can exist across frames. -jk
 */
class ClientMobjThinkerData : public MobjThinkerData
{
public:
    struct RemoteSync
    {
        int flags;
        uint time; ///< Time of last update.
        int sound; ///< Queued sound ID.
        float volume; ///< Volume for queued sound.

        RemoteSync()
            : flags(0)
            , time(Timer_RealMilliseconds())
            , sound(0)
            , volume(0)
        {}
    };

public:
    ClientMobjThinkerData(const de::Id &id = de::Id::none());
    ClientMobjThinkerData(const ClientMobjThinkerData &other);

    void think() override;
    IData *duplicate() const override;

    void stateChanged(const state_t *previousState) override;
    void damageReceived(int damage, const mobj_t *inflictor) override;

    int stateIndex() const;

    bool hasRemoteSync() const;

    /**
     * Returns the network state of the mobj. This state is not allocated until this is
     * called for the first time.
     */
    RemoteSync &remoteSync();

    /**
     * If the object is represented by a model, returns the current state of the
     * object's animation.
     *
     * @return Animation state, or @c NULL if not drawn as a model.
     */
    render::StateAnimator *animator();

    const render::StateAnimator *animator() const;

    const de::Mat4f &modelTransformation() const;

    void operator << (de::Reader &from) override;
    void operator >> (de::Writer &to) const override;

private:
    DE_PRIVATE(d)
};

#endif // DE_WORLD_CLIENTMOBJTHINKERDATA_H
