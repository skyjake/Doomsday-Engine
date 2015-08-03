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

struct MobjAnimator::Parameters
{
    enum LoopMode {
        NotLooping = 0,
        Looping = 1
    };

    LoopMode looping;

    Parameters(LoopMode loop = NotLooping) : looping(loop) {}
};

Q_DECLARE_METATYPE(MobjAnimator::Parameters)

struct MobjAnimator::Private
{
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
};

MobjAnimator::MobjAnimator(DotPath const &id, ModelDrawable const &model)
    : ModelDrawable::Animator(model)
    , _stateAnims(ClientApp::renderSystem().modelRenderer().animations(id))
{}

void MobjAnimator::triggerByState(String const &stateName)
{
    // No animations can be triggered if none are available.
    if(!_stateAnims) return;

    auto found = _stateAnims->constFind(stateName);
    if(found == _stateAnims->constEnd()) return;

    //LOG_WIP("triggerByState: ") << stateName;

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
            int animId = ModelRenderer::identifierFromText(seq.name, [this] (String const &name) {
                return model().animationIdForName(name);
            });

            // Do not restart running sequences.
            // TODO: Only restart if the current state is not the expected one.
            if(isRunning(animId, node)) continue;

            // Start a new sequence.
            Animation &anim = start(animId, node);

            Parameters params(ScriptedInfo::isTrue(*seq.def, DEF_LOOPING)?
                                  Parameters::Looping : Parameters::NotLooping);
            anim.data.setValue(params);
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

        // Stop finished animations.
        if(!Private::isRunning(anim))
        {
            stop(i--);
        }
    }
}

ddouble MobjAnimator::currentTime(int index) const
{
    // Mobjs think on sharp ticks only, however we need to ensure time advances on
    // every frame for smooth animation.
    return ModelDrawable::Animator::currentTime(index);// + frameTimePos;

    /// @todo Should prevent time from passing the end of the sequence?
}
