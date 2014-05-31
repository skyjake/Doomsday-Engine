/** @file defs/model.cpp  Model definition accessor.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/Record>
#include <de/RecordValue>

using namespace de;

namespace defn {

void Model::resetToDefaults()
{
    DENG2_ASSERT(_def);

    // Add all expected fields with their default values.
    _def->addText  ("id", "");
    _def->addText  ("state", "");
    _def->addNumber("off", 0);
    _def->addText  ("sprite", "");
    _def->addNumber("spriteFrame", 0);
    _def->addNumber("group", 0);
    _def->addNumber("selector", 0);
    _def->addNumber("flags", 0);
    _def->addNumber("interMark", 0);
    _def->addArray ("interRange", new ArrayValue(Vector2i(0, 1)));
    _def->addNumber("skinTics", 0);
    _def->addArray ("scale", new ArrayValue(Vector3i(1, 1, 1)));
    _def->addNumber("resize", 0);
    _def->addArray ("offset", new ArrayValue(Vector3f()));
    _def->addNumber("shadowRadius", 0);
    _def->addArray ("sub", new ArrayValue);
}

Model &Model::operator = (Record *d)
{
    setAccessedRecord(*d);
    _def = d;
    return *this;
}

int Model::order() const
{
    if(!accessedRecordPtr()) return -1;
    return geti("__order__");
}

Model::operator bool() const
{
    return accessedRecordPtr() != 0;
}

Record &Model::addSub()
{
    DENG2_ASSERT(_def);

    Record *def = new Record;

    def->addText  ("filename", "");
    def->addText  ("skinFilename", "");
    def->addText  ("frame", "");
    def->addNumber("frameRange", 0);
    def->addNumber("flags", 0);
    def->addNumber("skin", 0);
    def->addNumber("skinRange", 0);
    def->addArray ("offset", new ArrayValue(Vector3f()));
    def->addNumber("alpha", 0);
    def->addNumber("parm", 0);
    def->addNumber("selSkinMask", 0);
    def->addNumber("selSkinShift", 0);

    ArrayValue *skins = new ArrayValue;
    for(int i = 0; i < 8; ++i) *skins << NumberValue(0);
    def->addArray ("selSkins", skins);

    def->addText  ("shinySkin", "");
    def->addNumber("shiny", 0);
    def->addArray ("shinyColor", new ArrayValue(Vector3f(1, 1, 1)));
    def->addNumber("shinyReact", 1);
    def->addNumber("blendMode", BM_NORMAL);

    (*_def)["sub"].value<ArrayValue>()
            .add(new RecordValue(def, RecordValue::OwnsRecord));

    return *def;
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
    DENG2_ASSERT(_def);
    return *_def->geta("sub")[index].as<RecordValue>().record();
}

Record const &Model::sub(int index) const
{
    return *geta("sub")[index].as<RecordValue>().record();
}

void Model::cleanupAfterParsing(Record const &prev)
{
    DENG2_ASSERT(_def);

    if(_def->gets("state") == "-")
    {
        _def->set("state", prev.gets("state"));
    }
    if(_def->gets("sprite") == "-")
    {
        _def->set("sprite", prev.gets("sprite"));
    }

    for(int i = 0; i < subCount(); ++i)
    {
        Record &subDef = sub(i);

        if(subDef.gets("filename") == "-")     subDef.set("filename", "");
        if(subDef.gets("skinFilename") == "-") subDef.set("skinFilename", "");
        if(subDef.gets("shinySkin") == "-")    subDef.set("shinySkin", "");
        if(subDef.gets("frame") == "-")        subDef.set("frame", "");
    }
}

} // namespace defn
