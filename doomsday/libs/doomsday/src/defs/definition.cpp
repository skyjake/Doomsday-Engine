/** @file definition.cpp  Base class for definition record accessors.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/defs/definition.h"
#include <de/record.h>

using namespace de;

namespace defn {

const String Definition::VAR_ID("id");
const String Definition::VAR_ORDER("__order__");

Definition::~Definition()
{}

Record &Definition::def()
{
    return const_cast<Record &>(accessedRecord());
}

const Record &Definition::def() const
{
    return accessedRecord();
}

String Definition::id() const
{
    if (!accessedRecordPtr()) return {};
    return gets(VAR_ID);
}

int Definition::order() const
{
    if (!accessedRecordPtr()) return -1;
    return geti(VAR_ORDER);
}

Definition::operator bool() const
{
    return accessedRecordPtr() != 0;
}

void Definition::resetToDefaults()
{
    def().addBoolean("custom", false);
}

} // namespace defn
