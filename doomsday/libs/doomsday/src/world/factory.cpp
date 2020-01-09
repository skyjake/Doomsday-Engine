/** @file factory.cpp  Factory for world objects.
 *
 * @authors Copyright (c) 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/factory.h"

// For Polyobj constructor:
#include "doomsday/world/polyobj.h"
#include "doomsday/DoomsdayApp"
#include <de/legacy/memory.h>

namespace world {

using namespace de;

static std::function<Material *(MaterialManifest &)> materialCtor;
static std::function<MobjThinkerData *(const Id &)>  mobjThinkerDataCtor;
static std::function<Sky *(const defn::Sky *)>       skyCtor;

void Factory::setMaterialConstructor(const std::function<Material *(MaterialManifest &)> &ctor)
{
    materialCtor = ctor;
}

Material *Factory::newMaterial(MaterialManifest &m)
{
    return materialCtor(m);
}

void Factory::setMobjThinkerDataConstructor(const std::function<MobjThinkerData *(const de::Id &)> &ctor)
{
    mobjThinkerDataCtor = ctor;
}

MobjThinkerData *Factory::newMobjThinkerData(const de::Id &id)
{
    return mobjThinkerDataCtor(id);
}

void Factory::setSkyConstructor(const std::function<Sky *(const defn::Sky *)> &ctor)
{
    skyCtor = ctor;
}

Sky *Factory::newSky(const defn::Sky *def)
{
    return skyCtor(def);
}

struct polyobj_s *Factory::newPolyobj(const de::Vec2d &origin)
{
    /// @todo The app should register their own constructor like with the others.
    const auto &gx = DoomsdayApp::app().plugins().gameExports();
    void *region = M_Calloc(gx.GetInteger(DD_POLYOBJ_SIZE));
    return new (region) Polyobj(origin);
}

} // namespace world
