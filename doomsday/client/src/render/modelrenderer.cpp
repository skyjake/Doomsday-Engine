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

using namespace de;

DENG2_PIMPL(ModelRenderer)
, DENG2_OBSERVES(filesys::AssetObserver, Availability)
{
    filesys::AssetObserver observer;
    ModelBank bank;

    Instance(Public *i)
        : Base(i)
        , observer("model\\..*") // all model assets
    {
        observer.audienceForAvailability() += this;
    }

    ~Instance()
    {
        observer.audienceForAvailability() -= this;
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
};

ModelRenderer::ModelRenderer() : d(new Instance(this))
{}

ModelBank &ModelRenderer::bank()
{
    return d->bank;
}
