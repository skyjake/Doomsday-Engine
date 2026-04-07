/** @file conditionaltrigger.cpp  Conditional trigger.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/conditionaltrigger.h"
#include "de/set.h"
#include "de/value.h"

namespace de {

DE_PIMPL(ConditionalTrigger)
, DE_OBSERVES(Variable, Change)
{
    SafePtr<Variable const> condition;
    Set<String> activeTriggers;
    bool anyTrigger = false;

    Impl(Public *i) : Base(i)
    {}

    void variableValueChanged(Variable &, const Value &) override
    {
        update();
    }

    void update()
    {
        anyTrigger = false;
        activeTriggers.clear();

        if (!condition) return;

        // The condition can be a text string or an array of text strings.
        const StringList trigs = condition->value().asStringList();

        for (const String &trig : trigs)
        {
            if (trig == "*")
            {
                anyTrigger = true;
                activeTriggers.clear();
                break;
            }
            activeTriggers << trig;
        }
    }

    bool check(const String &trigger)
    {
        if (anyTrigger) return true;
        return activeTriggers.contains(trigger);
    }
};

ConditionalTrigger::ConditionalTrigger()
    : d(new Impl(this))
{}

bool ConditionalTrigger::isValid() const
{
    return d->condition;
}

void ConditionalTrigger::setCondition(const Variable &variable)
{
    if (d->condition)
    {
        d->condition->audienceForChange() -= d;
    }
    d->condition.reset(&variable);
    variable.audienceForChange() += d;
    d->update();
}

const Variable &ConditionalTrigger::condition() const
{
    return *d->condition;
}

bool ConditionalTrigger::tryTrigger(const String &trigger)
{
    if (d->check(trigger))
    {
        handleTriggered(trigger);
        return true;
    }
    return false;
}

} // namespace de
