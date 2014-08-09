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

using namespace de;

static String const DEF_PROBABILITY("prob");
static String const DEF_ROOT_NODE  ("node");

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
        // TODO: Only restart if the current state is not the expected one.
        if(isRunning(animId, node)) continue;

        start(animId, node);

        qDebug() << "starting" << seq.name;
    }
}

void MobjAnimator::advanceTime(TimeDelta const &elapsed)
{
    ModelDrawable::Animator::advanceTime(elapsed);

    for(int i = 0; i < count(); ++i)
    {
        Animation &anim = at(i);
        ddouble factor = 1.0;
        // TODO: Determine actual time factor.

        // Advance the sequence.
        anim.time += factor * elapsed;

        //qDebug() << "advancing" << anim.animId << "time" << anim.time;
    }
}

ddouble MobjAnimator::currentTime(int index) const
{
    // Mobjs think on sharp ticks only, however we need to ensure time advances on
    // every frame for smooth animation.
    return ModelDrawable::Animator::currentTime(index);// + frameTimePos;

    /// @todo Should prevent time from passing the end of the sequence?
}
