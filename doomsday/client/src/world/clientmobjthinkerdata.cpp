/** @file clientmobjthinkerdata.cpp  Private client-side data for mobjs.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/clientmobjthinkerdata.h"
#include "render/modelrenderer.h"
#include "world/generator.h"
#include "clientapp.h"
#include "dd_loop.h"
#include "def_main.h"
#include <QFlags>

using namespace de;

static String const DEF_PROBABILITY("prob");
static String const DEF_ROOT_NODE  ("node");

namespace internal
{
    enum Flag
    {
        Initialized = 0x1       ///< Thinker data has been initialized.
    };
    Q_DECLARE_FLAGS(Flags, Flag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(Flags)
}

using namespace ::internal;

DENG2_PIMPL(ClientMobjThinkerData)
{
    /**
     * Mobj-specific model animator.
     */
    struct Animator : public ModelDrawable::Animator
    {
        ClientMobjThinkerData::Instance &d;
        ModelRenderer::StateAnims const *stateAnims;

        Animator(ClientMobjThinkerData::Instance *inst, ModelDrawable const &model)
            : ModelDrawable::Animator(model)
            , d(*inst)
            , stateAnims(ClientApp::renderSystem().modelRenderer().animations(inst->modelId()))
        {}

        void triggerByState(String const &stateName)
        {
            // No animations can be triggered if none are available.
            if(!stateAnims) return;

            ModelRenderer::StateAnims::const_iterator found = stateAnims->constFind(stateName);
            if(found == stateAnims->constEnd()) return;

            foreach(ModelRenderer::AnimSequence const &seq, found.value())
            {
                // Test for the probability of this animation.
                float chance = seq.def->getf(DEF_PROBABILITY, 1.f);
                if(frand() > chance) continue;

                // Start the animation on the specified node (defaults to root),
                // unless it is already running.
                String const node = seq.def->gets(DEF_ROOT_NODE, "");
                int animId;
                if(seq.name.startsWith('#'))
                {
                    // Animation sequence specified by index.
                    animId = seq.name.mid(1).toInt();
                }
                else
                {
                    animId = model().animationIdForName(seq.name);
                }

                // Do not restart running sequences.
                if(isRunning(animId, node)) continue;

                start(animId, node);

                qDebug() << "starting" << seq.name;
            }
        }

        /// Determines how fast each animation sequence is proceeding.
        void advanceTime(TimeDelta const &elapsed)
        {
            ModelDrawable::Animator::advanceTime(elapsed);

            for(int i = 0; i < count(); ++i)
            {
                Animation &anim = at(i);
                ddouble factor = 1.0;
                // TODO: Determine actual time factor.

                // Advance the sequence.
                anim.time += factor * elapsed;

                qDebug() << "advancing" << anim.animId << "time" << anim.time;
            }
        }
    };

    Flags flags;
    QScopedPointer<RemoteSync> sync;
    QScopedPointer<Animator> animator;

    Instance(Public *i) : Base(i)
    {}

    Instance(Public *i, Instance const &other) : Base(i)
    {
        if(!other.sync.isNull())
        {
            sync.reset(new RemoteSync(*other.sync));
        }
    }

    String thingName() const
    {
        return Def_GetMobjName(self.mobj()->type);
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

    void deinitModel()
    {
        animator.reset();
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
            ModelDrawable const &model = modelBank().model(modelId());
            animator.reset(new Animator(this, model));
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
        if(animator.isNull()) return;

        animator->triggerByState(stateName());
    }

    /**
     * Checks the motion of the object and triggers suitable animations (for standing,
     * walking, or running). These animations are defined separately from the state
     * based animations.
     */
    void triggerMovementAnimations()
    {
        if(animator.isNull()) return;


    }

    void advanceAnimations(TimeDelta const &delta)
    {
        if(animator.isNull()) return;

        animator->advanceTime(delta);
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
    d->triggerMovementAnimations();
    d->advanceAnimations(DD_CurrentTickDuration());
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
    return !d->sync.isNull();
}

ClientMobjThinkerData::RemoteSync &ClientMobjThinkerData::remoteSync()
{
    if(!hasRemoteSync())
    {
        d->sync.reset(new RemoteSync);
    }
    return *d->sync;
}

ModelDrawable::Animator *ClientMobjThinkerData::animator()
{
    return d->animator.data();
}

void ClientMobjThinkerData::stateChanged(state_t const *previousState)
{
    MobjThinkerData::stateChanged(previousState);

    bool const justSpawned = !previousState;

    d->initOnce();
    d->triggerStateAnimations();
    d->triggerParticleGenerators(justSpawned);
}
