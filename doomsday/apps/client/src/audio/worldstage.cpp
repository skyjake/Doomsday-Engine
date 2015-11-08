/** @file worldstage.cpp  audio::Stage specialized for the world context.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "audio/worldstage.h"

#include "audio/listener.h"

#include "world/worldsystem.h"
#include "world/map.h"
#include "world/p_object.h"
#include "world/sector.h"

#include "clientapp.h"

using namespace de;

namespace audio {

DENG2_PIMPL(WorldStage)
, DENG2_OBSERVES(WorldSystem, MapChange)
, DENG2_OBSERVES(Deletable,   Deletion)
{
    Instance(Public *i) : Base(i)
    {
        ClientApp::worldSystem().audienceForMapChange() += this;
    }

    ~Instance()
    {
        ClientApp::worldSystem().audienceForMapChange() -= this;
    }

    void worldSystemMapChanged()
    {
        self.listener().setTrackedMapObject(nullptr);

        if(ClientApp::worldSystem().hasMap())
        {
            ClientApp::worldSystem().map().audienceForDeletion += this;
        }
    }

    void objectWasDeleted(Deletable *)
    {
        self.stopAllSounds();

        // Instruct the Listener to forget the map-object being tracked.
        /// @todo Should observe MapObject deletion. -ds
        self.listener().setTrackedMapObject(nullptr);
    }
};

WorldStage::WorldStage(Exclusion exclusion)
    : Stage(exclusion)
    , d(new Instance(this))
{}

void WorldStage::stopSound(dint effectId, SoundEmitter *emitter, dint flags)
{
    // Are we performing any special stop behaviors?
    if(emitter && flags)
    {
        // Sector-based sound stopping.
        if(emitter->thinker.id)
        {
            /// @var emitter is a map-object.
            emitter = &Mobj_Sector((mobj_t *)emitter)->soundEmitter();
        }
        else
        {
            // The head of the chain is the sector. Find it.
            while(emitter->thinker.prev)
            {
                emitter = (SoundEmitter *)emitter->thinker.prev;
            }
        }
    }

    // Stop Sounds emitted by the Sector's Emitter?
    if(!emitter || (flags & SSF_SECTOR))
    {
        Stage::stopSound(effectId, emitter);
    }

    // Also stop Sounds emitted by Sector-linked (plane/wall) Emitters?
    if(emitter && (flags & SSF_SECTOR_LINKED_SURFACES))
    {
        // Process the rest of the emitter chain.
        while((emitter = (SoundEmitter *)emitter->thinker.next))
        {
            // Stop sounds from this emitter, also.
            Stage::stopSound(effectId, emitter);
        }
    }
}

}  // namespace audio
