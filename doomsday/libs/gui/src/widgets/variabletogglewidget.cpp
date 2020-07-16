/** @file variabletogglewidget.cpp Toggles the value of a variable.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/variabletogglewidget.h"

#include <de/numbervalue.h>

namespace de {

DE_PIMPL(VariableToggleWidget),
DE_OBSERVES(Variable,     Deletion),
DE_OBSERVES(Variable,     Change  ),
DE_OBSERVES(ToggleWidget, Toggle  )
{
    Variable *var;
    NumberValue activeValue;
    NumberValue inactiveValue;

    Impl(Public *i, Variable &variable)
        : Base(i)
        , var(&variable)
        , activeValue(1)
        , inactiveValue(0)
    {
        updateFromVariable();

        self().audienceForToggle() += this;
        var->audienceForDeletion() += this;
        var->audienceForChange() += this;
    }

    void updateFromVariable()
    {
        if (!var) return;

        self().setToggleState(!var->value().compare(activeValue)? Active : Inactive,
                            false /*don't notify*/);
    }

    void setVariableFromWidget()
    {
        if (!var) return;

        var->audienceForChange() -= this;
        var->set(self().isActive()? activeValue : inactiveValue);
        var->audienceForChange() += this;
    }

    void toggleStateChanged(ToggleWidget &)
    {
        setVariableFromWidget();
    }

    void variableValueChanged(Variable &, const Value &)
    {
        updateFromVariable();
    }

    void variableBeingDeleted(Variable &)
    {
        var = nullptr;
        self().disable();
    }
};

VariableToggleWidget::VariableToggleWidget(Variable &variable, const String &name)
    : ToggleWidget(DefaultFlags, name)
    , d(new Impl(this, variable))
{}

VariableToggleWidget::VariableToggleWidget(const String &label, Variable &variable, const String &name)
    : ToggleWidget(DefaultFlags, name)
    , d(new Impl(this, variable))
{
    setText(label);
}

Variable &VariableToggleWidget::variable() const
{
    if (!d->var)
    {
        throw VariableMissingError("VariableToggleWidget::variable",
                                   "Widget is not associated with a variable");
    }
    return *d->var;
}

void VariableToggleWidget::setActiveValue(double val)
{
    d->activeValue = NumberValue(val);
    d->updateFromVariable();
}

void VariableToggleWidget::setInactiveValue(double val)
{
    d->inactiveValue = NumberValue(val);
    d->updateFromVariable();
}

} // namespace de
