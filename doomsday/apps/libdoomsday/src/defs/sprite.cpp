/** @file sprite.cpp  Sprite definition accessor.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/defs/sprite.h"
#include "doomsday/defs/ded.h"

#include <de/Record>
#include <de/RecordValue>

using namespace de;

namespace defn {

void Sprite::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addBoolean("frontOnly", true);        ///< @c true= only use the front View.
    def().addArray  ("views", new ArrayValue);
}

Record &Sprite::addView(String material, dint angle, bool mirrorX)
{
    auto *view = new Record;
    view->addNumber ("angle",    de::max(0, angle - 1));  // Make 0-based.
    view->addText   ("material", material);
    view->addBoolean("mirrorX",  mirrorX);

    if(angle <= 0)
    {
        def()["views"] = new ArrayValue;
    }

    def()["frontOnly"].set(NumberValue(angle <= 0));
    def()["views"    ].array().add(new RecordValue(view, RecordValue::OwnsRecord));

    return *view;
}

dint Sprite::viewCount() const
{
    return geta("views").size();
}

bool Sprite::hasView(dint angle) const
{
    if(getb("frontOnly")) angle = 0;

    for(Value const *val : geta("views").elements())
    {
        Record const &view = val->as<RecordValue>().dereference();
        if(view.geti("angle") == angle)
            return true;
    }
    return false;
}

Record &Sprite::view(dint angle)
{
    if(getb("frontOnly")) angle = 0;

    for(Value *val : def().geta("views").elements())
    {
        Record &view = val->as<RecordValue>().dereference();
        if(view.geti("angle") == angle)
            return view;
    }
    /// @throw MissingViewError  Invalid angle specified.
    throw MissingViewError("Sprite::view", "Unknown view:" + String::number(angle));
}

Record &Sprite::nearestView(angle_t mobjAngle, angle_t angleToEye, bool noRotation)
{
    dint angle = 0;  // Use the front view (default).

    if(!noRotation && !def().getb("frontOnly"))
    {
        // Choose a view according to the relative angle with viewer (the eye).
        angle = ((angleToEye - mobjAngle + (unsigned) (ANG45 / 2) * 9) - (unsigned) (ANGLE_180 / 16)) >> 28;
    }

    return view(angle);
}

}  // namespace defn
