/** @file cvarlineeditwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "con_main.h"

using namespace de;
using namespace ui;

DENG2_PIMPL_NOREF(CVarLineEditWidget)
{
    char const *cvar;

    cvar_t *var() const
    {
        cvar_t *cv = Con_FindVariable(cvar);
        DENG2_ASSERT(cv != 0);
        return cv;
    }
};

CVarLineEditWidget::CVarLineEditWidget(char const *cvarPath)
    : d(new Instance)
{
    setSignalOnEnter(true);
    connect(this, SIGNAL(enterPressed(QString)), this, SLOT(endEditing()));

    d->cvar = cvarPath;
    updateFromCVar();
}

void CVarLineEditWidget::contentChanged()
{
    LineEditWidget::contentChanged();

    if(String(CVar_String(d->var())) != text())
    {
        setCVarValueFromWidget();
    }
}

char const *CVarLineEditWidget::cvarPath() const
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
    CVar_SetString(d->var(), text().toUtf8());
}

