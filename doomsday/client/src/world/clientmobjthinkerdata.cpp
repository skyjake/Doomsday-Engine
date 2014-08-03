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
#include "def_main.h"
#include "clientapp.h"
#include "render/modelrenderer.h"
//#include <de/ModelBank>
#include <QFlags>

using namespace de;

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
    Flags flags;
    QScopedPointer<RemoteSync> sync;
    QScopedPointer<ModelDrawable::Animator> animator;

    Instance(Public *i) : Base(i)
    {}

    Instance(Public *i, Instance const &other) : Base(i)
    {
        if(!other.sync.isNull())
        {
            sync.reset(new RemoteSync(*other.sync));
        }
    }

    String thingType() const
    {
        return Def_GetMobjName(self.mobj()->type);
    }

    String modelId() const
    {
        return String("model.thing.%1").arg(thingType().toLower());
    }

    static ModelBank &modelBank()
    {
        return ClientApp::renderSystem().modelRenderer().bank();
    }

    /**
     * Initializes the client-specific mobj data. This is performed once, during the
     * first time the object thinks.
     */
    void initIfNeeded()
    {
        // Initialization is only done once.
        if(flags & Initialized) return;
        flags |= Initialized;

        // Check for an available model asset.
        if(modelBank().has(modelId()))
        {
            // Prepare the animation state of the model.
            ModelDrawable const &model = modelBank().model(modelId());
            animator.reset(new ModelDrawable::Animator(model));

            // Set up the initial animation.

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
    d->initIfNeeded();
}

Thinker::IData *ClientMobjThinkerData::duplicate() const
{
    return new ClientMobjThinkerData(*this);
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
