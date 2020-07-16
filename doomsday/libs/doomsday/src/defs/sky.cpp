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

#include <de/record.h>
#include <de/recordvalue.h>

using namespace de;

namespace defn {

void Sky::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText  (VAR_ID, "");
    def().addNumber("flags", 0);
    def().addNumber("height", DEFAULT_SKY_HEIGHT);
    def().addNumber("horizonOffset", DEFAULT_SKY_HORIZON_OFFSET);
    def().addArray ("color", new ArrayValue(Vec3f()));
    def().addArray ("layer", new ArrayValue);
    def().addArray ("model", new ArrayValue);

    // Skies have two layers by default.
    addLayer();
    addLayer();
}

Record &Sky::addLayer()
{
    Record *layer = new Record;

    layer->addBoolean("custom", false);

    layer->addNumber("flags", 0);
    layer->addText  ("material", "");
    layer->addNumber("offset", DEFAULT_SKY_SPHERE_XOFFSET);
    layer->addNumber("offsetSpeed", 0);
    layer->addNumber("colorLimit", DEFAULT_SKY_SPHERE_FADEOUT_LIMIT);

    def()["layer"].array().add(new RecordValue(layer, RecordValue::OwnsRecord));

    return *layer;
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
    return *def().geta("layer")[index].as<RecordValue>().record();
}

const Record &Sky::layer(int index) const
{
    return *geta("layer")[index].as<RecordValue>().record();
}

Record &Sky::addModel()
{
    Record *model = new Record;

    model->addBoolean("custom", false);

    model->addText  (VAR_ID, "");
    model->addNumber("layer", -1);
    model->addNumber("frameInterval", 1);
    model->addNumber("yaw", 0);
    model->addNumber("yawSpeed", 0);
    model->addArray ("originOffset", new ArrayValue(Vec3f()));
    model->addArray ("rotate", new ArrayValue(Vec2f()));
    model->addText  ("execute", "");
    model->addArray ("color", new ArrayValue(Vec4f(1)));

    def()["model"].array().add(new RecordValue(model, RecordValue::OwnsRecord));

    return *model;
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
    return *def().geta("model")[index].as<RecordValue>().record();
}

const Record &Sky::model(int index) const
{
    return *geta("model")[index].as<RecordValue>().record();
}

} // namespace defn

