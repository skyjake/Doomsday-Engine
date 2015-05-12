/** @file binding.h  Base class for binding record accessors.
 *
 * @authors Copyright © 2009-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/binding.h"

#include <de/RecordValue>
#include "ui/b_util.h" // B_EqualConditions

using namespace de;

static int idCounter = 0;

Record &Binding::def()
{
    return const_cast<Record &>(accessedRecord());
}

Record const &Binding::def() const
{
    return accessedRecord();
}

Binding::operator bool() const
{
    return accessedRecordPtr() != 0;
}

void Binding::resetToDefaults()
{
    def().addNumber("id", 0);  ///< Unique identifier.
    def().addArray("condition", new ArrayValue);
}

Record &Binding::addCondition()
{
    Record *cond = new Record;

    cond->addNumber("type", Invalid);
    cond->addNumber("test", None);
    cond->addNumber("device", -1);
    cond->addNumber("id", -1);
    cond->addNumber("pos", 0);
    cond->addBoolean("negate", false);
    cond->addBoolean("multiplayer", false);

    def()["condition"].value<ArrayValue>()
            .add(new RecordValue(cond, RecordValue::OwnsRecord));

    return *cond;
}

int Binding::conditionCount() const
{
    return int(geta("condition").size());
}

bool Binding::hasCondition(int index) const
{
    return index >= 0 && index < conditionCount();
}

Record &Binding::condition(int index)
{
    return *def().geta("condition")[index].as<RecordValue>().record();
}

Record const &Binding::condition(int index) const
{
    return *geta("condition")[index].as<RecordValue>().record();
}

bool Binding::equalConditions(Binding const &other) const
{
    // Quick test (assumes there are no duplicated conditions).
    if(def()["condition"].value<ArrayValue>().elements().count() != other.geta("condition").elements().count())
    {
        return false;
    }

    ArrayValue const &conds = def().geta("condition");
    DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, conds.elements())
    {
        Record const &a = *(*i)->as<RecordValue>().record();

        bool found = false;
        ArrayValue const &conds2 = other.geta("condition");
        DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, conds2.elements())
        {
            Record const &b = *(*i)->as<RecordValue>().record();
            if(B_EqualConditions(a, b))
            {
                found = true;
                break;
            }
        }
        if(!found) return false;
    }

    return true;
}

void Binding::resetIdentifiers() // static
{
    idCounter = 0;
}

int Binding::newIdentifier() // static
{
    int id = 0;
    while(!id) { id = ++idCounter; }
    return id;
}
