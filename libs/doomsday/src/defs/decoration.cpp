/** @file decoration.cpp  Decoration definition accessor.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/defs/decoration.h"

#include <de/record.h>
#include <de/recordvalue.h>
#include "doomsday/defs/ded.h"
#include "doomsday/defs/material.h"

using namespace de;

namespace defn {

static const String VAR_TEXTURE = "texture";
static const String VAR_FLAGS   = "flags";
static const String VAR_LIGHT   = "light";

void Decoration::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText  (VAR_TEXTURE, "");  // URI. Unknown.
    def().addNumber(VAR_FLAGS, 0);
    def().addArray (VAR_LIGHT, new ArrayValue);
}

Record &Decoration::addLight()
{
    auto *decor = new Record;
    MaterialDecoration(*decor).resetToDefaults();
    def()[VAR_LIGHT].array().add(new RecordValue(decor, RecordValue::OwnsRecord));
    return *decor;
}

int Decoration::lightCount() const
{
    return int(geta(VAR_LIGHT).size());
}

bool Decoration::hasLight(int index) const
{
    return index >= 0 && index < lightCount();
}

Record &Decoration::light(int index)
{
    return *def().geta(VAR_LIGHT)[index].as<RecordValue>().record();
}

const Record &Decoration::light(int index) const
{
    return *geta(VAR_LIGHT)[index].as<RecordValue>().record();
}

} // namespace defn
