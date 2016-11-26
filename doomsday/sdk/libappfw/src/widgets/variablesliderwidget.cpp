/** @file variablesliderwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/VariableSliderWidget"

#include <de/NumberValue>
#include <de/NativePointerValue>
#include <de/Animation>

namespace de {

DENG2_PIMPL(VariableSliderWidget)
, DENG2_OBSERVES(Variable, Deletion)
, DENG2_OBSERVES(Variable, Change  )
{
    ValueType valueType = VariableSliderWidget::Number;
    Variable *var;

    Impl(Public *i, Variable &variable) : Base(i), var(&variable)
    {
        var->audienceForDeletion() += this;
        var->audienceForChange() += this;
    }

    void init()
    {
        self().updateFromVariable();
        QObject::connect(thisPublic, SIGNAL(valueChangedByUser(double)),
                         thisPublic, SLOT(setVariableFromWidget()));
    }

    void updateFromVariable()
    {
        if (!var) return;

        switch (valueType)
        {
        case VariableSliderWidget::Number:
            self().setValue(var->value<NumberValue>().asNumber());
            break;

        case VariableSliderWidget::Animation:
            self().setValue(var->value<NativePointerValue>().nativeObject<de::Animation>()->target());
            break;
        }
    }

    void setVariableFromWidget()
    {
        if (!var) return;

        var->audienceForChange() -= this;
        switch (valueType)
        {
        case VariableSliderWidget::Number:
            var->set(NumberValue(self().value()));
            break;

        case VariableSliderWidget::Animation:
            var->value<NativePointerValue>().nativeObject<de::Animation>()->setValue(float(self().value()));
            break;
        }
        var->audienceForChange() += this;
    }

    void variableValueChanged(Variable &, Value const &)
    {
        updateFromVariable();
    }

    void variableBeingDeleted(Variable &)
    {
        var = 0;
        self().disable();
    }
};

VariableSliderWidget::VariableSliderWidget(Variable &variable, Ranged const &range,
                                           ddouble step, String const &name)
    : SliderWidget(name)
    , d(new Impl(this, variable))
{
    if (!variable.value().is<NumberValue>())
    {
        d->valueType = VariableSliderWidget::Animation;
    }
    setRange(range, step);
    d->init();
}

VariableSliderWidget::VariableSliderWidget(ValueType valueType,
                                           Variable &variable, Ranged const &range,
                                           ddouble step, String const &name)
    : SliderWidget(name)
    , d(new Impl(this, variable))
{
    d->valueType = valueType;
    setRange(range, step);
    d->init();
}

Variable &VariableSliderWidget::variable() const
{
    if (!d->var)
    {
        throw VariableMissingError("VariableSliderWidget::variable",
                                   "Widget is not associated with a variable");
    }
    return *d->var;
}

void VariableSliderWidget::updateFromVariable()
{
    d->updateFromVariable();
}

void VariableSliderWidget::setVariableFromWidget()
{
    d->setVariableFromWidget();
}

} // namespace de
