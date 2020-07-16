/** @file variablelineeditwidget.cpp  Edits the text of a variable.
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

#include "de/variablelineeditwidget.h"

#include <de/textvalue.h>

namespace de {

DE_PIMPL(VariableLineEditWidget),
DE_OBSERVES(Variable, Deletion),
DE_OBSERVES(Variable, Change  )
{
    Variable *var;

    Impl(Public *i, Variable &variable) : Base(i), var(&variable)
    {
        updateFromVariable();

        var->audienceForDeletion() += this;
        var->audienceForChange() += this;
    }

    void updateFromVariable()
    {
        if (!var) return;

        self().setText(var->value().asText());
    }

    void setVariableFromWidget()
    {
        if (!var) return;

        var->audienceForChange() -= this;
        var->set(TextValue(self().text()));
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

VariableLineEditWidget::VariableLineEditWidget(Variable &variable, const String &name)
    : LineEditWidget(name), d(new Impl(this, variable))
{
    audienceForContentChange() += [this](){ setVariableFromWidget(); };
}

Variable &VariableLineEditWidget::variable() const
{
    if (!d->var)
    {
        throw VariableMissingError("VariableLineEditWidget::variable",
                                   "Widget is not associated with a variable");
    }
    return *d->var;
}

void VariableLineEditWidget::updateFromVariable()
{
    d->updateFromVariable();
}

void VariableLineEditWidget::setVariableFromWidget()
{
    d->setVariableFromWidget();
}

} // namespace de
