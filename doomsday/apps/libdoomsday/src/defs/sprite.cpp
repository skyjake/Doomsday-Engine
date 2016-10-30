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
#include "doomsday/UriValue"

#include <de/Record>
#include <de/RecordValue>
#include <de/DictionaryValue>

using namespace de;

namespace defn {

static QString const VAR_VIEWS     ("views");
static QString const VAR_FRONT_ONLY("frontOnly");
static QString const VAR_MATERIAL  ("material"); // UriValue
static QString const VAR_MIRROR_X  ("mirrorX");

void Sprite::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addBoolean(VAR_FRONT_ONLY, true);        ///< @c true= only use the front View.
    def().addDictionary(VAR_VIEWS);
}

DictionaryValue &Sprite::viewsDict()
{
    return def()[VAR_VIEWS].value().as<DictionaryValue>();
}

DictionaryValue const &Sprite::viewsDict() const
{
    return def().getdt(VAR_VIEWS);
}

Record &Sprite::addView(String material, dint angle, bool mirrorX)
{
    if (angle <= 0)
    {
        def().addDictionary(VAR_VIEWS);
    }
    def().set(VAR_FRONT_ONLY, angle <= 0);

    auto *view = new Record;
    view->add(VAR_MATERIAL).set(new UriValue(de::Uri(material, RC_NULL)));
    view->addBoolean(VAR_MIRROR_X, mirrorX);
    viewsDict().add(new NumberValue(de::max(0, angle - 1)), new RecordValue(view, RecordValue::OwnsRecord));
    return *view;
}

dint Sprite::viewCount() const
{
    return viewsDict().size();
}

bool Sprite::hasView(dint angle) const
{
    if (angle && getb(VAR_FRONT_ONLY)) angle = 0;

    return viewsDict().contains(NumberValue(angle));

    /*for (Value const *val : geta("views").elements())
    {
        Record const &view = val->as<RecordValue>().dereference();
        if (view.geti("angle") == angle)
            return true;
    }
    return false;*/
}

Record &Sprite::findView(dint angle)
{
    if (angle && getb(VAR_FRONT_ONLY)) angle = 0;

    return viewsDict().element(NumberValue(angle)).as<RecordValue>().dereference();

    /*for (Value *val : def().geta("views").elements())
    {
        Record &view = val->as<RecordValue>().dereference();
        if (view.geti("angle") == angle)
            return view;
    }
    /// @throw MissingViewError  Invalid angle specified.
    throw MissingViewError("Sprite::view", "Unknown view:" + String::number(angle));*/
}

Record const *Sprite::tryFindView(dint angle) const
{
    if (angle && getb(VAR_FRONT_ONLY)) angle = 0;

    NumberValue const ang(angle);
    auto const &elems = viewsDict().elements();
    auto found = elems.find(&ang);
    if (found != elems.end())
    {
        return found->second->as<RecordValue>().record();
    }
    return nullptr;
}

static de::Uri const nullUri;

Sprite::View Sprite::view(de::dint angle) const
{
    View v;
    if (Record const *rec = tryFindView(angle))
    {
        v.material = &rec->get(VAR_MATERIAL).as<UriValue>().uri();
        v.mirrorX  = rec->getb(VAR_MIRROR_X);
    }
    else
    {
        v.material = &nullUri;
        v.mirrorX  = false;
    }
    return v;
}

de::Uri const &Sprite::viewMaterial(de::dint angle) const
{
    if (Record const *rec = tryFindView(angle))
    {
        return rec->get(VAR_MATERIAL).as<UriValue>().uri();
    }
    return nullUri;
}

Sprite::View Sprite::nearestView(angle_t mobjAngle, angle_t angleToEye, bool noRotation) const
{
    dint angle = 0;  // Use the front view (default).

    if (!noRotation && !def().getb(VAR_FRONT_ONLY))
    {
        // Choose a view according to the relative angle with viewer (the eye).
        angle = ((angleToEye - mobjAngle + (unsigned) (ANG45 / 2) * 9) - (unsigned) (ANGLE_180 / 16)) >> 28;
    }

    return view(angle);
}

}  // namespace defn
