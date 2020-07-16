/** @file sprite.cpp  Sprite definition accessor.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "doomsday/urivalue.h"

#include <de/record.h>
#include <de/recordvalue.h>
#include <de/dictionaryvalue.h>

using namespace de;

namespace defn {

static String const VAR_VIEWS     ("views");
static String const VAR_FRONT_ONLY("frontOnly");
static String const VAR_MATERIAL  ("material"); // UriValue
static String const VAR_MIRROR_X  ("mirrorX");

//---------------------------------------------------------------------------------------

CompiledSprite::CompiledSprite()
{}

CompiledSprite::CompiledSprite(const Record &spriteDef)
{
    frontOnly = spriteDef.getb(VAR_FRONT_ONLY);

    // Compile the views into a vector.
    const auto &viewsDict = spriteDef.getdt(VAR_VIEWS).elements();
    for (auto iter = viewsDict.begin(); iter != viewsDict.end(); ++iter)
    {
        ++viewCount;

        int angle = iter->first.value->asInt();
        if (views.sizei() <= angle) views.resize(angle + 1);

        const Record &viewDef = iter->second->as<RecordValue>().dereference();
        auto &view = views[angle];

        view.uri     = viewDef.get(VAR_MATERIAL).as<UriValue>().uri();
        view.mirrorX = viewDef.getb(VAR_MIRROR_X);
    }
}

//---------------------------------------------------------------------------------------

CompiledSpriteRecord &Sprite::def()
{
    return static_cast<CompiledSpriteRecord &>(Definition::def());
}

const CompiledSpriteRecord &Sprite::def() const
{
    return static_cast<const CompiledSpriteRecord &>(Definition::def());
}

void Sprite::resetToDefaults()
{
    Definition::resetToDefaults();

    def().resetCompiled();

    // Add all expected fields with their default values.
    def().addBoolean(VAR_FRONT_ONLY, true);        ///< @c true= only use the front View.
    def().addDictionary(VAR_VIEWS);
}

DictionaryValue &Sprite::viewsDict()
{
    return def()[VAR_VIEWS].value<DictionaryValue>();
}

/*const DictionaryValue &Sprite::viewsDict() const
{
    return def().getdt(VAR_VIEWS);
}*/

Record &Sprite::addView(String material, dint angle, bool mirrorX)
{
    def().resetCompiled();

    if (angle <= 0)
    {
        def().addDictionary(VAR_VIEWS);
    }
    def().set(VAR_FRONT_ONLY, angle <= 0);

    auto *view = new Record;
    view->add(VAR_MATERIAL).set(new UriValue(res::makeUri(material)));
    view->addBoolean(VAR_MIRROR_X, mirrorX);
    viewsDict().add(new NumberValue(de::max(0, angle - 1)), new RecordValue(view, RecordValue::OwnsRecord));
    return *view;
}

dint Sprite::viewCount() const
{
    return def().compiled().viewCount;
}

bool Sprite::hasView(dint angle) const
{
    const auto &cmpl = def().compiled();

    if (cmpl.frontOnly) angle = 0;

    //return viewsDict().contains(NumberValue(angle));

    return (angle < cmpl.views.sizei() && !cmpl.views.at(angle).uri.isEmpty());

    /*for (const Value *val : geta("views").elements())
    {
        const Record &view = val->as<RecordValue>().dereference();
        if (view.geti("angle") == angle)
            return true;
    }
    return false;*/
}

#if 0
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
    throw MissingViewError("Sprite::view", "Unknown view:" + String::asText(angle));*/
}

const Record *Sprite::tryFindView(dint angle) const
{
    if (angle && getb(VAR_FRONT_ONLY)) angle = 0;

    NumberValue const ang(angle);
    const auto &elems = viewsDict().elements();
    auto found = elems.find(&ang);
    if (found != elems.end())
    {
        return found->second->as<RecordValue>().record();
    }
    return nullptr;
}
#endif

static res::Uri nullUri;

Sprite::View Sprite::view(int angle) const
{
    const auto &cmpl = def().compiled();

    if (cmpl.frontOnly) angle = 0;

    View v;
    if (angle < cmpl.views.sizei())
    {
        v.material = &cmpl.views.at(angle).uri;
        v.mirrorX  = cmpl.views.at(angle).mirrorX;
    }
    else
    {
        v.material = &nullUri;
        v.mirrorX  = false;
    }
    return v;
}

const res::Uri &Sprite::viewMaterial(int angle) const
{
    const auto &cmpl = def().compiled();
    if (angle < cmpl.views.sizei())
    {
        return cmpl.views.at(angle).uri;
    }
    return nullUri;
}

Sprite::View Sprite::nearestView(angle_t mobjAngle, angle_t angleToEye, bool noRotation) const
{
    dint angle = 0;  // Use the front view (default).

    if (!noRotation)
    {
        // Choose a view according to the relative angle with viewer (the eye).
        angle = ((angleToEye - mobjAngle + (unsigned) (ANG45 / 2) * 9) - (unsigned) (ANGLE_180 / 16)) >> 28;
    }

    return view(angle);
}

}  // namespace defn
