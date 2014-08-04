/** @file modelrenderer.cpp  Model renderer.
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

#include "render/modelrenderer.h"

#include <de/filesys/AssetObserver>
#include <de/App>
#include <de/ModelBank>
#include <de/ScriptedInfo>

using namespace de;

static String const DEF_ANIMATION("animation");

DENG2_PIMPL(ModelRenderer)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
, DENG2_OBSERVES(Bank, Load)
{
    filesys::AssetObserver observer;
    ModelBank bank;

    Instance(Public *i)
        : Base(i)
        , observer("model\\..*") // all model assets
    {
        observer.audienceForAvailability() += this;
        bank.audienceForLoad() += this;
    }

    void assetAvailabilityChanged(String const &identifier, filesys::AssetObserver::Event event)
    {
        //qDebug() << "loading model:" << identifier << event;

        if(event == filesys::AssetObserver::Added)
        {
            bank.add(identifier, App::asset(identifier).absolutePath("path"));

            // Begin loading the model right away.
            bank.load(identifier);
        }
        else
        {
            bank.remove(identifier);
        }
    }

    /**
     * When model assets have been loaded, we can parse their metadata to see if there
     * are any animation sequences defined. If so, we'll set up a shared lookup table
     * that determines which sequences to start in which mobj states.
     *
     * @param path  Model asset id.
     */
    void bankLoaded(DotPath const &path)
    {
        Package::Asset const asset = App::asset(path);

        // Set up the animation sequences for states.
        if(asset.has(DEF_ANIMATION))
        {
            // Prepare the animations for the model.
            QScopedPointer<StateAnims> anims(new StateAnims);

            auto states = ScriptedInfo::subrecordsOfType("state", asset.subrecord(DEF_ANIMATION));
            DENG2_FOR_EACH_CONST(Record::Subrecords, state, states)
            {
                auto seqs = ScriptedInfo::subrecordsOfType("sequence", *state.value());
                DENG2_FOR_EACH_CONST(Record::Subrecords, seq, seqs)
                {
                    (*anims)[state.key()] << AnimSequence(seq.key(), *seq.value());
                }
            }

            // TODO: Check for a possible timeline and calculate time factors accordingly.

            // Store the animation sequence lookup in the bank.
            bank.setAnimation(path, anims.take());
        }
    }
};

ModelRenderer::ModelRenderer() : d(new Instance(this))
{}

ModelBank &ModelRenderer::bank()
{
    return d->bank;
}

ModelRenderer::StateAnims const *ModelRenderer::animations(DotPath const &modelId) const
{
    return d->bank.animation(modelId)->maybeAs<StateAnims>();
}
