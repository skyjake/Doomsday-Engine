/** @file mobjanimator.cpp
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

#include "render/mobjanimator.h"
#include "render/rendersystem.h"
#include "clientapp.h"
#include "dd_loop.h"

#include <de/ScriptedInfo>

using namespace de;

static String const DEF_PROBABILITY("prob");
static String const DEF_ROOT_NODE  ("node");
static String const DEF_LOOPING    ("looping");
static String const DEF_PRIORITY   ("priority");

static int const ANIM_DEFAULT_PRIORITY = 1;

struct MobjAnimator::Parameters
{
    enum LoopMode {
        NotLooping = 0,
        Looping = 1
    };

    LoopMode looping;
    int priority;

    Parameters(LoopMode loop = NotLooping,
               int prio = ANIM_DEFAULT_PRIORITY)
        : looping(loop)
        , priority(prio)
    {}
};

Q_DECLARE_METATYPE(MobjAnimator::Parameters)

DENG2_PIMPL(MobjAnimator)
{
    ModelRenderer::StateAnims const *stateAnims;
    struct Pending {
        int animId;
        Parameters params;
        Pending() : animId(-1) {}
        Pending(int id, Parameters const &p)
            : animId(id), params(p) {}
    };
    QHash<String, Pending> pendingAnimForNode;
    String currentStateName;

    Instance(Public *i, DotPath const &id)
        : Base(i)
        , stateAnims(ClientApp::renderSystem().modelRenderer().animations(id))
    {}

    static bool isRunning(Animation const &anim)
    {
        Parameters const &params = anim.data.value<Parameters>();
        if(params.looping == Parameters::Looping)
        {
            // Looping animations are always running.
            return true;
        }
        return !anim.isAtEnd();
    }

    int animationId(String const &name) const
    {
        return ModelRenderer::identifierFromText(name, [this] (String const &name) {
            return self.model().animationIdForName(name);
        });
    }
};

MobjAnimator::MobjAnimator(DotPath const &id, ModelDrawable const &model)
    : ModelDrawable::Animator(model)
    , d(new Instance(this, id))
{}

void MobjAnimator::triggerByState(String const &stateName)
{
    // No animations can be triggered if none are available.
    if(!d->stateAnims) return;

    auto found = d->stateAnims->constFind(stateName);
    if(found == d->stateAnims->constEnd()) return;

    //LOG_WIP("triggerByState: ") << stateName;
    d->currentStateName = stateName;

    foreach(ModelRenderer::AnimSequence const &seq, found.value())
    {
        try
        {
            // Test for the probability of this animation.
            float chance = seq.def->getf(DEF_PROBABILITY, 1.f);
            if(frand() > chance) continue;

            // Start the animation on the specified node (defaults to root),
            // unless it is already running.
            String const node = seq.def->gets(DEF_ROOT_NODE, "");
            int animId = d->animationId(seq.name);

            // Do not restart running sequences.
            // TODO: Only restart if the current state is not the expected one.
            if(isRunning(animId, node)) continue;

            int const priority = seq.def->geti(DEF_PRIORITY, ANIM_DEFAULT_PRIORITY);

            // Parameters for the new sequence.
            Parameters params(ScriptedInfo::isTrue(*seq.def, DEF_LOOPING)? Parameters::Looping :
                                                                           Parameters::NotLooping,
                              priority);

            // Do not override higher-priority animations.
            if(Animation *existing = find(node))
            {
                if(priority < existing->data.value<Parameters>().priority)
                {
                    // This will be started once the higher-priority animation
                    // has finished.
                    d->pendingAnimForNode[node] = Instance::Pending(animId, params);
                    continue;
                }
            }

            d->pendingAnimForNode.remove(node);

            // Start a new sequence.
            start(animId, node).data.setValue(params);
        }
        catch(ModelDrawable::Animator::InvalidError const &er)
        {
            LOGDEV_GL_WARNING("Failed to start animation \"%s\": %s")
                    << seq.name << er.asText();
            continue;
        }

        LOG_WIP("Starting anim: " _E(b)) << seq.name;
        break;
    }
}

void MobjAnimator::advanceTime(TimeDelta const &elapsed)
{
    ModelDrawable::Animator::advanceTime(elapsed);

    bool retrigger = false;

    for(int i = 0; i < count(); ++i)
    {
        Animation &anim = at(i);
        Parameters const &params = anim.data.value<Parameters>();
        ddouble factor = 1.0;
        // TODO: Determine actual time factor.

        // Advance the sequence.
        anim.time += factor * elapsed;

        if(params.looping == Parameters::NotLooping)
        {
            // Clamp at the end.
            anim.time = min(anim.time, anim.duration);
        }

        if(params.looping == Parameters::Looping)
        {
            // When a looping animation has completed a loop, it may still trigger
            // a variant.
            if(anim.isAtEnd())
            {
                retrigger = true;
            }
        }

        // Stop finished animations.
        if(!Instance::isRunning(anim))
        {
            String const node = anim.node;

            stop(i--); // `anim` gets deleted

            // Start a previously triggered pending animation.
            auto &pending = d->pendingAnimForNode;
            if(pending.contains(node))
            {
                LOG_WIP("Starting pending anim %i") << pending[node].animId;
                start(pending[node].animId, node)
                        .data.setValue(pending[node].params);
                pending.remove(node);
            }
        }
    }

    if(retrigger && !d->currentStateName.isEmpty())
    {
        LOG_WIP("retrigger: " _E(C)) << d->currentStateName;
        triggerByState(d->currentStateName);
    }
}

ddouble MobjAnimator::currentTime(int index) const
{
    // Mobjs think on sharp ticks only, however we need to ensure time advances on
    // every frame for smooth animation.
    return ModelDrawable::Animator::currentTime(index);// + frameTimePos;
}
