/** @file clientmobjthinkerdata.cpp  Private client-side data for mobjs.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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
#include "dd_loop.h"
#include "clientapp.h"
#include "world/generator.h"
#include "world/clientmobjthinkerdata.h"
#include "render/rendersystem.h"
#include "render/modelrenderer.h"
#include "render/stateanimator.h"

#include <de/dscript.h>

using namespace de;
using namespace world;

namespace internal {

enum Flag {
    Initialized  = 0x1, ///< Thinker data has been initialized.
    StateChanged = 0x2  ///< State has changed during the current tick.
};

} // namespace internal

using namespace ::internal;

DE_PIMPL(ClientMobjThinkerData)
, DE_OBSERVES(Asset, Deletion)
{
    enum SerialFlagUInt16 {
        HasAnimator = 0x0001,
    };

    Flags flags;
    std::unique_ptr<RemoteSync> sync;
    std::unique_ptr<render::StateAnimator> animator;
    Mat4f modelMatrix;

    Impl(Public *i) : Base(i)
    {}

    Impl(Public *i, const Impl &other) : Base(i)
    {
        if (other.sync)
        {
            sync.reset(new RemoteSync(*other.sync));
        }
    }

    ~Impl()
    {
        deinit();
    }

    String thingName() const
    {
        return DED_Definitions()->getMobjName(self().mobj()->type);
    }

    String stateName() const
    {
        return Def_GetStateName(self().mobj()->state);
    }

    bool isStateInCurrentSequence(const state_t *previous)
    {
        if (!previous) return false;
        return Def_GetState(previous->nextState) == self().mobj()->state;
    }

    String modelId() const
    {
        return "model.thing." + thingName().lower();
    }

    static ModelBank &modelBank()
    {
        return ClientApp::render().modelRenderer().bank();
    }

    void assetBeingDeleted(Asset &)
    {
        de::trash(animator.release());
    }

    /**
     * Initializes the client-specific mobj data. This is performed once, during the
     * first time the object thinks.
     */
    void initOnce()
    {
        // Initialization is only done once.
        if (flags & Initialized) return;
        flags |= Initialized;

        // Check for an available model asset.
        if (modelBank().has(modelId()))
        {
            // Prepare the animation state of the model.
            const auto &model = modelBank().model<render::Model>(modelId());
            try
            {
                model.audienceForDeletion() += this;

                animator.reset(new render::StateAnimator(modelId(), model));
                animator->setOwnerNamespace(self().objectNamespace(), DE_STR("__thing__"));

                // Apply possible scaling operations on the model.
                modelMatrix = model.transformation;
                if (model.flags & render::Model::AutoscaleToThingHeight)
                {
                    const Vec3f dims = modelMatrix * model.dimensions();
                    modelMatrix = Mat4f::scale(self().mobj()->height / dims.y * 1.2f /*aspect correct*/) * modelMatrix;
                }
            }
            catch (const Error &er)
            {
                model.audienceForDeletion() -= this;

                LOG_RES_ERROR("Failed to set up asset '%s' for map object %i: %s")
                    << modelId() << self().mobj()->thinker.id << er.asText();
            }
        }
    }

    void deinit()
    {
        flags &= ~Initialized;

        if (animator)
        {
            animator->model().audienceForDeletion() -= this;
        }
    }

    /**
     * Checks if there are any animations defined to start in the current state. All
     * animation sequences associated with the state are checked. A sequence may specify
     * a less than 1.0 probability for starting. The sequence may be identified either by
     * name ("walk") or index (for example, "#3").
     */
    void triggerStateAnimations(const state_t *state = nullptr)
    {
        if (flags & StateChanged)
        {
            flags &= ~StateChanged;
            if (animator)
            {
                animator->triggerByState(state? Def_GetStateName(state) : stateName());
            }
        }
    }

    /**
     * Checks the motion of the object and triggers suitable animations (for standing,
     * walking, or running). These animations are defined separately from the state
     * based animations.
     */
    void triggerMovementAnimations()
    {
        if (!animator) return;


    }

    void advanceAnimations(TimeSpan delta)
    {
        if (animator)
        {
            animator->advanceTime(delta);
        }
    }

    void triggerParticleGenerators(bool justSpawned)
    {
        // Check for a ptcgen trigger.
        for (ded_ptcgen_t *pg = runtimeDefs.stateInfo[self().stateIndex()].ptcGens;
            pg; pg = pg->stateNext)
        {
            if (!(pg->flags & Generator::SpawnOnly) || justSpawned)
            {
                // We are allowed to spawn the generator.
                Mobj_SpawnParticleGen(self().mobj(), pg);
            }
        }
    }
};

ClientMobjThinkerData::ClientMobjThinkerData(const de::Id &id)
    : MobjThinkerData(id)
    , d(new Impl(this))
{}

ClientMobjThinkerData::ClientMobjThinkerData(const ClientMobjThinkerData &other)
    : MobjThinkerData(other)
    , d(new Impl(this, *other.d))
{}

void ClientMobjThinkerData::think()
{
    MobjThinkerData::think();

    d->initOnce();
    d->triggerStateAnimations(); // with current state
    d->triggerMovementAnimations();
    d->advanceAnimations(SECONDSPERTIC); // mobjs think only on sharp ticks
}

Thinker::IData *ClientMobjThinkerData::duplicate() const
{
    return new ClientMobjThinkerData(*this);
}

int ClientMobjThinkerData::stateIndex() const
{
    return runtimeDefs.states.indexOf(mobj()->state);
}

bool ClientMobjThinkerData::hasRemoteSync() const
{
    return bool(d->sync);
}

ClientMobjThinkerData::RemoteSync &ClientMobjThinkerData::remoteSync()
{
    if (!hasRemoteSync())
    {
        d->sync.reset(new RemoteSync);
    }
    return *d->sync;
}

render::StateAnimator *ClientMobjThinkerData::animator()
{
    return d->animator.get();
}

const render::StateAnimator *ClientMobjThinkerData::animator() const
{
    return d->animator.get();
}

const Mat4f &ClientMobjThinkerData::modelTransformation() const
{
    return d->modelMatrix;
}

void ClientMobjThinkerData::stateChanged(const state_t *previousState)
{
    MobjThinkerData::stateChanged(previousState);

    const bool justSpawned = !previousState;

    d->initOnce();
    if ((d->flags & StateChanged) && d->isStateInCurrentSequence(previousState))
    {
        /*
         * Mobj state has already been flagged as changed, but triggers for the
         * previous state haven't fired yet. Because it's the same sequence, it
         * might be the one that triggers an animation, so let's not miss the
         * trigger.
         */
        d->triggerStateAnimations(previousState);
    }
    /*
     * Trigger animations later during think(). This is done to avoid multiple
     * state changes during a single tick from interrupting longer sequences,
     * for instance allowing consecutive attack sequences to play as a single,
     * long sequence (e.g., Hexen's Ettin).
     */
    if (mobj()->state != previousState)
    {
        d->flags |= StateChanged;
    }
    d->triggerParticleGenerators(justSpawned);
}

void ClientMobjThinkerData::damageReceived(int damage, const mobj_t *inflictor)
{
    MobjThinkerData::damageReceived(damage, inflictor);

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    Mobj_SpawnDamageParticleGen(mobj(), inflictor, damage);

    if (d->animator)
    {
        d->animator->triggerDamage(damage, inflictor);
    }
}

void ClientMobjThinkerData::operator << (Reader &from)
{
    world::InternalSerialId sid;
    from >> sid;
    if (sid != world::CLIENT_MOBJ_THINKER_DATA)
    {
        throw DeserializationError("ClientMobjThinkerData::operator <<",
                                   "Invalid serial identifier " +
                                   String::asText(sid));
    }

    MobjThinkerData::operator << (from);

    duint16 flags = 0;
    from >> flags;

    d->deinit();

    if (flags & Impl::HasAnimator) // Animator
    {
        d->initOnce();
        if (d->animator)
        {
            from >> *d->animator;
        }
        else
        {
            render::StateAnimator temp;
            from >> temp; // Not used.
        }
    }
}

void ClientMobjThinkerData::operator >> (Writer &to) const
{
    to << world::InternalSerialId(world::CLIENT_MOBJ_THINKER_DATA);

    MobjThinkerData::operator >> (to);

    const duint16 flags = (d->animator? Impl::HasAnimator : 0);
    to << flags;

    if (d->animator)
    {
        // Serialize the animator state.
        to << *d->animator;
    }
}

