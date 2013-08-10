/** @file variabletogglewidget.cpp Toggles the value of a variable.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/variabletogglewidget.h"

#include <de/NumberValue>

using namespace de;

DENG2_PIMPL(VariableToggleWidget),
DENG2_OBSERVES(Variable,     Deletion),
DENG2_OBSERVES(Variable,     Change  ),
DENG2_OBSERVES(ToggleWidget, Toggle  )
{
    Variable *var;

    Instance(Public *i, Variable &variable) : Base(i), var(&variable)
    {
        updateFromVariable();

        self.audienceForToggle += this;
        var->audienceForDeletion += this;
        var->audienceForChange += this;
    }

    ~Instance()
    {
        if(var)
        {
            var->audienceForDeletion -= this;
            var->audienceForChange -= this;
            self.audienceForToggle -= this;
        }
    }

    void updateFromVariable()
    {
        if(!var) return;

        self.setToggleState(var->value().isTrue()? Active : Inactive, false /*don't notify*/);
    }

    void setVariableFromWidget()
    {
        if(!var) return;

        var->audienceForChange -= this;
        var->set(NumberValue(self.isActive()? true : false));
        var->audienceForChange += this;
    }

    void toggleStateChanged(ToggleWidget &)
    {
        setVariableFromWidget();
    }

    void variableValueChanged(Variable &, Value const &)
    {
        updateFromVariable();
    }

    void variableBeingDeleted(Variable &)
    {
        var = 0;
        self.disable();
    }
};

VariableToggleWidget::VariableToggleWidget(Variable &variable, String const &name)
    : ToggleWidget(name), d(new Instance(this, variable))
{}

VariableToggleWidget::VariableToggleWidget(String const &label, Variable &variable, String const &name)
    : ToggleWidget(name), d(new Instance(this, variable))
{
    setText(label);
}

Variable &VariableToggleWidget::variable() const
{
    if(!d->var)
    {
        throw VariableMissingError("VariableToggleWidget::variable",
                                   "Widget is not associated with a variable");
    }
    return *d->var;
}

