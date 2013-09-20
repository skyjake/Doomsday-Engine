/** @file cvartogglewidget.cpp Console variable toggle.
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

#include "ui/widgets/cvartogglewidget.h"
#include "con_main.h"

using namespace de;
using namespace ui;

DENG2_PIMPL_NOREF(CVarToggleWidget)
{
    char const *cvar;

    cvar_t *var() const
    {
        cvar_t *cv = Con_FindVariable(cvar);
        DENG2_ASSERT(cv != 0);
        return cv;
    }
};

CVarToggleWidget::CVarToggleWidget(char const *cvarPath, String const &labelText)
    : d(new Instance)
{
    setText(labelText);

    d->cvar = cvarPath;
    updateFromCVar();

    connect(this, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)),
            this, SLOT(setCVarValueFromWidget()));
}

char const *CVarToggleWidget::cvarPath() const
{
    return d->cvar;
}

void CVarToggleWidget::updateFromCVar()
{
    setActive(CVar_Integer(d->var()) != 0);
}

void CVarToggleWidget::setCVarValueFromWidget()
{
    CVar_SetInteger(d->var(), isActive()? 1 : 0);
}
