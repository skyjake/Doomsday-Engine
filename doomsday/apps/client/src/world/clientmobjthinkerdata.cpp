/** @file clientmobjthinkerdata.cpp  Private client-side data for mobjs.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "world/clientmobjthinkerdata.h"

#include <QFlags>
#include "clientapp.h"
#include "dd_loop.h"
#include "render/modelrenderer.h"
#include "render/mobjanimator.h"
#include "world/generator.h"

using namespace de;

namespace internal
{
    enum Flag
    {
        Initialized = 0x1,      ///< Thinker data has been initialized.
        StateChanged = 0x2      ///< State has changed during the current tick.
    };
    Q_DECLARE_FLAGS(Flags, Flag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Flags)
}

using namespace ::internal;

DENG2_PIMPL(ClientMobjThinkerData)
, DENG2_OBSERVES(Asset, Deletion)
{
    Flags flags;
    std::unique_ptr<RemoteSync> sync;
    std::unique_ptr<MobjAnimator> animator;
    ModelRenderer::AuxiliaryData const *modelAuxData = nullptr;
    Matrix4f modelMatrix;

    Instance(Public *i) : Base(i)
    {}

    Instance(Public *i, Instance const &other) : Base(i)
    {
        if(other.sync)
        {
            sync.reset(new RemoteSync(*other.sync));
        }
    }

    ~Instance()
    {
        deinit();
    }

    String thingName() const
    {
        return ::defs.getMobjName(self.mobj()->type);
    }

    String stateName() const
    {
        return Def_GetStateName(self.mobj()->state);
    }

    String modelId() const
    {
        return String("model.thing.%1").arg(thingName().toLower());
    }

    static ModelBank &modelBank()
    {
        return ClientApp::renderSystem().modelRenderer().bank();
    }

    void assetBeingDeleted(Asset &)
    {
        de::trash(animator.release());
        modelAuxData = nullptr;
    }

    /**
     * Initializes the client-specific mobj data. This is performed once, during the
     * first time the object thinks.
     */
    void initOnce()
    {
        // Initialization is only done once.
        if(flags & Initialized) return;
        flags |= Initialized;

        // Check for an available model asset.
        if(modelBank().has(modelId()))
        {
            // Prepare the animation state of the model.
            ModelBank::ModelWithData loaded = modelBank().modelAndData(modelId());
            ModelDrawable &model = *loaded.first;
            model.audienceForDeletion() += this;
            animator.reset(new MobjAnimator(modelId(), model));
            animator->setOwnerNamespace(self.info());

            modelAuxData = &loaded.second->as<ModelRenderer::AuxiliaryData>();

            // Apply possible scaling operations on the model.
            modelMatrix = modelAuxData->transformation;
            if(modelAuxData->autoscaleToThingHeight)
            {
                Vector3f const dims = modelMatrix * model.dimensions();
                modelMatrix = Matrix4f::scale(self.mobj()->height / dims.y * 1.2f /*aspect correct*/) * modelMatrix;
            }
        }
    }

    void deinit()
    {
        if(animator)
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
    void triggerStateAnimations()
    {
        if(animator)
        {
            animator->triggerByState(stateName());
        }
    }

    /**
     * Checks the motion of the object and triggers suitable animations (for standing,
     * walking, or running). These animations are defined separately from the state
     * based animations.
     */
    void triggerMovementAnimations()
    {
        if(!animator) return;


    }

    void advanceAnimations(TimeDelta const &delta)
    {
        if(animator)
        {
            animator->advanceTime(delta);
        }
    }

    void triggerParticleGenerators(bool justSpawned)
    {
        // Check for a ptcgen trigger.
        for(ded_ptcgen_t *pg = runtimeDefs.stateInfo[self.stateIndex()].ptcGens;
            pg; pg = pg->stateNext)
        {
            if(!(pg->flags & Generator::SpawnOnly) || justSpawned)
            {
                // We are allowed to spawn the generator.
                Mobj_SpawnParticleGen(self.mobj(), pg);
            }
        }
    }
};

ClientMobjThinkerData::ClientMobjThinkerData()
    : d(new Instance(this))
{}

ClientMobjThinkerData::ClientMobjThinkerData(ClientMobjThinkerData const &other)
    : MobjThinkerData(other)
    , d(new Instance(this, *other.d))
{}

void ClientMobjThinkerData::think()
{
    d->initOnce();
    if(d->flags.testFlag(StateChanged))
    {
        d->flags &= ~StateChanged;
        d->triggerStateAnimations();
    }
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
    if(!hasRemoteSync())
    {
        d->sync.reset(new RemoteSync);
    }
    return *d->sync;
}

MobjAnimator *ClientMobjThinkerData::animator()
{
    return d->animator.get();
}

MobjAnimator const *ClientMobjThinkerData::animator() const
{
    return d->animator.get();
}

Matrix4f const &ClientMobjThinkerData::modelTransformation() const
{
    return d->modelMatrix;
}

ModelRenderer::AuxiliaryData const &ClientMobjThinkerData::auxiliaryModelData() const
{
    return *d->modelAuxData;
}

void ClientMobjThinkerData::stateChanged(state_t const *previousState)
{
    MobjThinkerData::stateChanged(previousState);

    bool const justSpawned = !previousState;

    d->initOnce();
    d->flags |= StateChanged; // Will trigger animations during think.
    d->triggerParticleGenerators(justSpawned);
}
