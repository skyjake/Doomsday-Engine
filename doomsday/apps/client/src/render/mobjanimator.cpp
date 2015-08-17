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

DENG2_PIMPL(MobjAnimator)
{
    /**
     * Specialized animation sequence state for a running animation.
     */
    struct Animation : public ModelDrawable::Animator::Animation
    {
        enum LoopMode {
            NotLooping = 0,
            Looping = 1
        };

        LoopMode looping = NotLooping;
        int priority = ANIM_DEFAULT_PRIORITY;

        Animation() {}

        Animation(int animationId, String const &rootNode, LoopMode looping, int priority)
            : looping(looping)
            , priority(priority)
        {
            animId = animationId;
            node   = rootNode;
        }

        void apply(Animation const &other)
        {
            looping  = other.looping;
            priority = other.priority;
        }

        bool isRunning() const
        {
            if(looping == Animation::Looping)
            {
                // Looping animations are always running.
                return true;
            }
            return !isAtEnd();
        }

        static Animation *make() { return new Animation; }
    };

    ModelRenderer::StateAnims const *stateAnims;
    QHash<String, Animation> pendingAnimForNode;
    String currentStateName;

    Instance(Public *i, DotPath const &id)
        : Base(i)
        , stateAnims(ClientApp::renderSystem().modelRenderer().animations(id))
    {}

    int animationId(String const &name) const
    {
        return ModelRenderer::identifierFromText(name, [this] (String const &name) {
            return self.model().animationIdForName(name);
        });
    }

    Animation &start(Animation const &spec)
    {
        Animation &anim = self.start(spec.animId, spec.node).as<Animation>();
        anim.apply(spec);
        return anim;
    }
};

MobjAnimator::MobjAnimator(DotPath const &id, ModelDrawable const &model)
    : ModelDrawable::Animator(model, Instance::Animation::make)
    , d(new Instance(this, id))
{}

void MobjAnimator::triggerByState(String const &stateName)
{
    // No animations can be triggered if none are available.
    if(!d->stateAnims) return;

    auto found = d->stateAnims->constFind(stateName);
    if(found == d->stateAnims->constEnd()) return;

    LOG_AS("MobjAnimator");
    LOG_GL_XVERBOSE("triggerByState: ") << stateName;

    d->currentStateName = stateName;

    foreach(ModelRenderer::AnimSequence const &seq, found.value())
    {
        using Animation = Instance::Animation;

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
            Animation anim(animId, node,
                           ScriptedInfo::isTrue(*seq.def, DEF_LOOPING)? Animation::Looping :
                                                                        Animation::NotLooping,
                           priority);

            // Do not override higher-priority animations.
            if(auto *existing = find(node)->maybeAs<Animation>())
            {
                if(priority < existing->priority)
                {
                    // This will be started once the higher-priority animation
                    // has finished.
                    d->pendingAnimForNode[node] = anim;
                    continue;
                }
            }

            d->pendingAnimForNode.remove(node);

            // Start a new sequence.
            d->start(anim);
        }
        catch(ModelDrawable::Animator::InvalidError const &er)
        {
            LOGDEV_GL_WARNING("Failed to start animation \"%s\": %s")
                    << seq.name << er.asText();
            continue;
        }

        LOG_GL_VERBOSE("Starting anim: " _E(b)) << seq.name;
        break;
    }
}

void MobjAnimator::advanceTime(TimeDelta const &elapsed)
{
    ModelDrawable::Animator::advanceTime(elapsed);

    bool retrigger = false;

    for(int i = 0; i < count(); ++i)
    {
        using Animation = Instance::Animation;

        auto &anim = at(i).as<Animation>();
        ddouble factor = 1.0;
        // TODO: Determine actual time factor.

        // Advance the sequence.
        anim.time += factor * elapsed;

        if(anim.looping == Animation::NotLooping)
        {
            // Clamp at the end.
            anim.time = min(anim.time, anim.duration);
        }

        if(anim.looping == Animation::Looping)
        {
            // When a looping animation has completed a loop, it may still trigger
            // a variant.
            if(anim.isAtEnd())
            {
                retrigger = true;
                anim.time -= anim.duration; // Trigger only once per loop.
            }
        }

        // Stop finished animations.
        if(!anim.isRunning())
        {
            String const node = anim.node;

            stop(i--); // `anim` gets deleted

            // Start a previously triggered pending animation.
            auto &pending = d->pendingAnimForNode;
            if(pending.contains(node))
            {
                LOG_GL_VERBOSE("Starting pending anim %i") << pending[node].animId;
                d->start(pending[node]);
                pending.remove(node);
            }
        }
    }

    if(retrigger && !d->currentStateName.isEmpty())
    {
        triggerByState(d->currentStateName);
    }
}

ddouble MobjAnimator::currentTime(int index) const
{
    // Mobjs think on sharp ticks only, however we need to ensure time advances on
    // every frame for smooth animation.
    return ModelDrawable::Animator::currentTime(index);// + frameTimePos;
}
