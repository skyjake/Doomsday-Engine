/** @file conditionaltrigger.h  Conditional trigger configurable via a Variable.
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

#ifndef LIBCORE_CONDITIONALTRIGGER_H
#define LIBCORE_CONDITIONALTRIGGER_H

#include "de/variable.h"

namespace de {

/**
 * Conditional trigger that calls a method.
 */
class DE_PUBLIC ConditionalTrigger
{
public:
    ConditionalTrigger();

    virtual ~ConditionalTrigger() = default;

    bool isValid() const;

    /**
     * Sets the variable that defines the condition for the trigger. The variable's
     * value can be a Text value or an Array with multiple Text values. If any of the
     * values is a single asterisk ("*"), the trigger will be activated with any input.
     *
     * @param variable  Variable for configuring the conditional trigger.
     */
    void setCondition(const Variable &variable);

    const Variable &condition() const;

    /**
     * Checks if a trigger will cause activation, and if so, call the appropriate
     * callback methods.
     *
     * @param trigger  Trigger to check.
     * @return @c true if the trigger was activated, otherwise @c false.
     */
    bool tryTrigger(const String &trigger);

    /**
     * Called when the trigger is activated.
     * @param trigger  Trigger that caused activation.
     */
    virtual void handleTriggered(const String &trigger) = 0;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_CONDITIONALTRIGGER_H
