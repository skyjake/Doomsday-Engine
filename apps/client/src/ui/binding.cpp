/** @file binding.h  Base class for binding record accessors.
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/recordvalue.h>

using namespace de;

static int idCounter = 0;

static const char *VAR_CONDITION = "condition";

Record &Binding::def()
{
    return const_cast<Record &>(accessedRecord());
}

const Record &Binding::def() const
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
    def().addArray(VAR_CONDITION, new ArrayValue);
}

Binding::CompiledConditionRecord &Binding::addCondition()
{
    auto *cond = new CompiledConditionRecord;
    cond->addNumber("type", Invalid);
    cond->addNumber("test", None);
    cond->addNumber("device", -1);
    cond->addNumber("id", -1);
    cond->addNumber("pos", 0);
    cond->addBoolean("negate", false);
    cond->addBoolean("multiplayer", false);
    def()[VAR_CONDITION].array().add(new RecordValue(cond, RecordValue::OwnsRecord));

    cond->resetCompiled();

    return *cond;
}

int Binding::conditionCount() const
{
    return int(geta(VAR_CONDITION).size());
}

bool Binding::hasCondition(int index) const
{
    return index >= 0 && index < conditionCount();
}

Binding::CompiledConditionRecord &Binding::condition(int index)
{
    return *static_cast<CompiledConditionRecord *>(def().geta(VAR_CONDITION)[index].as<RecordValue>().record());
}

const Binding::CompiledConditionRecord &Binding::condition(int index) const
{
    return *static_cast<const CompiledConditionRecord *>(geta(VAR_CONDITION)[index].as<RecordValue>().record());
}

bool Binding::equalConditions(const Binding &other) const
{
    // Quick test (assumes there are no duplicated conditions).
    if (def()[VAR_CONDITION].array().elements().count() != other.geta(VAR_CONDITION).elements().count())
    {
        return false;
    }

    const ArrayValue &conds = def().geta(VAR_CONDITION);
    DE_FOR_EACH_CONST(ArrayValue::Elements, i, conds.elements())
    {
        const auto &a = *static_cast<const CompiledConditionRecord *>((*i)->as<RecordValue>().record());

        bool found = false;
        const ArrayValue &conds2 = other.geta(VAR_CONDITION);
        DE_FOR_EACH_CONST(ArrayValue::Elements, i, conds2.elements())
        {
            const auto &b = *static_cast<const CompiledConditionRecord *>((*i)->as<RecordValue>().record());
            if (a.compiled() == b.compiled())
            {
                found = true;
                break;
            }
        }
        if (!found) return false;
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
    while (!id) { id = ++idCounter; }
    return id;
}

Binding::CompiledCondition::CompiledCondition(const Record &rec)
    : type(ConditionType(rec.geti("type")))
    , test(ControlTest(rec.geti("test")))
    , device(rec.geti("device"))
    , id(rec.geti("id"))
    , pos(rec.getf("pos"))
    , negate(rec.getb("negate"))
    , multiplayer(rec.getb("multiplayer"))
{}

bool Binding::CompiledCondition::operator == (const CompiledCondition &other) const
{
    return (type        == other.type &&
            test        == other.test &&
            device      == other.device &&
            id          == other.id &&
            de::fequal(pos, other.pos) &&
            negate      == other.negate &&
            multiplayer == other.multiplayer);
}
