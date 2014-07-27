/** @file defs/sky.cpp  Sky definition accessor.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/defs/sky.h"
#include "doomsday/defs/ded.h"

#include <de/Record>
#include <de/RecordValue>

using namespace de;

namespace defn {

void Sky::resetToDefaults()
{
    DENG2_ASSERT(_def);

    // Add all expected fields with their default values.
    _def->addText  ("id", "");
    _def->addNumber("flags", 0);
    _def->addNumber("height", .666667f);
    _def->addNumber("horizonOffset", 0);
    _def->addArray ("color", new ArrayValue(Vector3f()));
    _def->addArray ("layer", new ArrayValue);
    _def->addArray ("model", new ArrayValue);
}

Sky &Sky::operator = (Record *d)
{
    setAccessedRecord(*d);
    _def = d;
    return *this;
}

int Sky::order() const
{
    if(!accessedRecordPtr()) return -1;
    return geti("__order__");
}

Sky::operator bool() const
{
    return accessedRecordPtr() != 0;
}

Record &Sky::addLayer()
{
    DENG2_ASSERT(_def);

    Record *def = new Record;

    def->addNumber("flags", 0);
    def->addText  ("material", "");
    def->addNumber("offset", 0);
    def->addNumber("colorLimit", .3f);

    (*_def)["layer"].value<ArrayValue>()
            .add(new RecordValue(def, RecordValue::OwnsRecord));

    return *def;
}

int Sky::layerCount() const
{
    return int(geta("layer").size());
}

bool Sky::hasLayer(int index) const
{
    return index >= 0 && index < layerCount();
}

Record &Sky::layer(int index)
{
    DENG2_ASSERT(_def);
    return *_def->geta("layer")[index].as<RecordValue>().record();
}

Record const &Sky::layer(int index) const
{
    return *geta("layer")[index].as<RecordValue>().record();
}

Record &Sky::addModel()
{
    DENG2_ASSERT(_def);

    Record *def = new Record;

    def->addText  ("id", "");
    def->addNumber("layer", -1);
    def->addNumber("frameInterval", 1);
    def->addNumber("yaw", 0);
    def->addNumber("yawSpeed", 0);
    def->addArray ("originOffset", new ArrayValue(Vector3f()));
    def->addArray ("rotate", new ArrayValue(Vector2f()));
    def->addText  ("exectute", "");
    def->addArray ("color", new ArrayValue(Vector4f(1, 1, 1, 1)));

    (*_def)["model"].value<ArrayValue>()
            .add(new RecordValue(def, RecordValue::OwnsRecord));

    return *def;
}

int Sky::modelCount() const
{
    return int(geta("model").size());
}

bool Sky::hasModel(int index) const
{
    return index >= 0 && index < modelCount();
}

Record &Sky::model(int index)
{
    DENG2_ASSERT(_def);
    return *_def->geta("model")[index].as<RecordValue>().record();
}

Record const &Sky::model(int index) const
{
    return *geta("model")[index].as<RecordValue>().record();
}

} // namespace defn
