/** @file cvarlineeditwidget.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/cvarlineeditwidget.h"
#include <doomsday/console/var.h>

using namespace de;
using namespace de::ui;

DE_PIMPL_NOREF(CVarLineEditWidget)
{
    const char *cvar;

    cvar_t *var() const
    {
        cvar_t *cv = Con_FindVariable(cvar);
        DE_ASSERT(cv != 0);
        return cv;
    }
};

CVarLineEditWidget::CVarLineEditWidget(const char *cvarPath)
    : d(new Impl)
{
    setSignalOnEnter(true);
    audienceForEnter() += [this](){ endEditing(); };

    d->cvar = cvarPath;
    updateFromCVar();
}

void CVarLineEditWidget::contentChanged()
{
    LineEditWidget::contentChanged();

    if (String(CVar_String(d->var())) != text())
    {
        setCVarValueFromWidget();
    }
}

const char *CVarLineEditWidget::cvarPath() const
{
    return d->cvar;
}

void CVarLineEditWidget::updateFromCVar()
{
    setText(CVar_String(d->var()));
}

void CVarLineEditWidget::endEditing()
{
    root().setFocus(0);
}

void CVarLineEditWidget::setCVarValueFromWidget()
{
    CVar_SetString(d->var(), text());
}

