/** @file render/model.cpp  Drawable model.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/model.h"

using namespace de;

namespace render {

DE_STATIC_STRING(DEF_TIMELINE, "timeline");

Model::AnimSequence::AnimSequence(const String &name, const Record &def)
    : name(name)
    , def(&def)
{
    // Parse timeline events.
    if (def.hasSubrecord(DEF_TIMELINE()))
    {
        timeline = new Timeline;
        timeline->addFromInfo(def.subrecord(DEF_TIMELINE()));
    }
    else if (def.hasMember(DEF_TIMELINE()))
    {
        // Uses a shared timeline in the definition. This will be looked up when
        // the animation starts.
        sharedTimeline = def.gets(DEF_TIMELINE());
    }
}

Model::~Model()
{
    // The commit group will be deleted now.
    unsetAtlas();

    timelines.deleteAll();
}

} // namespace render
