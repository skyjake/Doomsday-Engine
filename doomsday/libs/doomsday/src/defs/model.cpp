/** @file defs/model.cpp  Model definition accessor.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/defs/model.h"
#include "doomsday/defs/ded.h"

#include <de/record.h>
#include <de/recordvalue.h>

using namespace de;

namespace defn {

void Model::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText  (VAR_ID, "");
    def().addText  ("state", "");
    def().addNumber("off", 0);
    def().addText  ("sprite", "");
    def().addNumber("spriteFrame", 0);
    def().addNumber("group", 0);
    def().addNumber("selector", 0);
    def().addNumber("flags", 0);
    def().addNumber("interMark", 0);
    def().addArray ("interRange", new ArrayValue(Vec2i(0, 1)));
    def().addNumber("skinTics", 0);
    def().addArray ("scale", new ArrayValue(Vec3i(1, 1, 1)));
    def().addNumber("resize", 0);
    def().addArray ("offset", new ArrayValue(Vec3f()));
    def().addNumber("shadowRadius", 0);
    def().addArray ("sub", new ArrayValue);
}

Record &Model::addSub()
{
    Record *sub = new Record;

    sub->addBoolean("custom", false);

    sub->addText  ("filename", "");
    sub->addText  ("skinFilename", "");
    sub->addText  ("frame", "");
    sub->addNumber("frameRange", 0);
    sub->addNumber("flags", 0);
    sub->addNumber("skin", 0);
    sub->addNumber("skinRange", 0);
    sub->addArray ("offset", new ArrayValue(Vec3f()));
    sub->addNumber("alpha", 0);
    sub->addNumber("parm", 0);
    sub->addNumber("selSkinMask", 0);
    sub->addNumber("selSkinShift", 0);

    ArrayValue *skins = new ArrayValue;
    for (int i = 0; i < 8; ++i) *skins << NumberValue(0);
    sub->addArray ("selSkins", skins);

    sub->addText  ("shinySkin", "");
    sub->addNumber("shiny", 0);
    sub->addArray ("shinyColor", new ArrayValue(Vec3f(1)));
    sub->addNumber("shinyReact", 1);
    sub->addNumber("blendMode", BM_NORMAL);

    def()["sub"].array().add(new RecordValue(sub, RecordValue::OwnsRecord));

    return *sub;
}

int Model::subCount() const
{
    return int(geta("sub").size());
}

bool Model::hasSub(int index) const
{
    return index >= 0 && index < subCount();
}

Record &Model::sub(int index)
{
    return *def().geta("sub")[index].as<RecordValue>().record();
}

const Record &Model::sub(int index) const
{
    return *geta("sub")[index].as<RecordValue>().record();
}

void Model::cleanupAfterParsing(const Record &prev)
{
    if (gets("state") == "-")
    {
        def().set("state", prev.gets("state"));
    }
    if (gets("sprite") == "-")
    {
        def().set("sprite", prev.gets("sprite"));
    }

    for (int i = 0; i < subCount(); ++i)
    {
        Record &subDef = sub(i);

        if (subDef.gets("filename") == "-")     subDef.set("filename", "");
        if (subDef.gets("skinFilename") == "-") subDef.set("skinFilename", "");
        if (subDef.gets("shinySkin") == "-")    subDef.set("shinySkin", "");
        if (subDef.gets("frame") == "-")        subDef.set("frame", "");
    }
}

} // namespace defn
