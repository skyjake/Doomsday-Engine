/** @file variablechoicewidget.cpp  Choice widget for Variable values.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/variablechoicewidget.h"

#include <de/numbervalue.h>
#include <de/textvalue.h>

namespace de {

DE_PIMPL(VariableChoiceWidget),
DE_OBSERVES(Variable, Deletion),
DE_OBSERVES(Variable, Change  )
{
    Variable *var;
    VariableType variableType;

    Impl(Public *i, Variable &variable, VariableType vt)
        : Base(i)
        , var(&variable)
        , variableType(vt)
    {
        updateFromVariable();

        var->audienceForDeletion() += this;
        var->audienceForChange() += this;
    }

    void updateFromVariable()
    {
        if (!var || self().items().isEmpty()) return;

        //if (variableType == Text)
        //{
            self().setSelected(self().items().findData(var->value()));
//        }
//        else
//        {
//            self().setSelected(self().items().findData(var->value()));
//        }
        }

    void setVariableFromWidget()
    {
        if (!var) return;

        var->audienceForChange() -= this;
        if (variableType == Text)
        {
            var->set(TextValue(self().selectedItem().data().asText()));
        }
        else
        {
            var->set(NumberValue(self().selectedItem().data().asNumber()));
        }
        var->audienceForChange() += this;
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

VariableChoiceWidget::VariableChoiceWidget(Variable &variable, VariableType variableType,
                                           const String &name)
    : ChoiceWidget(name)
    , d(new Impl(this, variable, variableType))
{
    audienceForUserSelection() += [this](){ setVariableFromWidget(); };
}

Variable &VariableChoiceWidget::variable() const
{
    if (!d->var)
    {
        throw VariableMissingError("VariableChoiceWidget::variable",
                                   "Widget is not associated with a variable");
    }
    return *d->var;
}

void VariableChoiceWidget::updateFromVariable()
{
    d->updateFromVariable();
}

void VariableChoiceWidget::setVariableFromWidget()
{
    d->setVariableFromWidget();
}

} // namespace de
