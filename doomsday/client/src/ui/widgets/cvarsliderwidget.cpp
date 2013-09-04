/** @file cvarsliderwidget.cpp  Slider for adjusting a cvar.
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

#include "ui/widgets/cvarsliderwidget.h"
#include "con_main.h"

using namespace de;
using namespace ui;

DENG2_PIMPL_NOREF(CVarSliderWidget)
{
    char const *cvar;

    cvar_t *var() const
    {
        cvar_t *cv = Con_FindVariable(cvar);
        DENG2_ASSERT(cv != 0);
        return cv;
    }
};

CVarSliderWidget::CVarSliderWidget(char const *cvarPath) : d(new Instance)
{
    d->cvar = cvarPath;
    updateFromCVar();

    // Default range and precision for floating point variables (may be altered later).
    if(d->var()->type == CVT_FLOAT)
    {
        if(!(d->var()->flags & (CVF_NO_MIN | CVF_NO_MAX)))
        {
            setRange(Rangef(d->var()->min, d->var()->max));
        }
        setPrecision(2);
    }

    connect(this, SIGNAL(valueChangedByUser(double)), this, SLOT(setCVarValueFromWidget()));
}

void CVarSliderWidget::updateFromCVar()
{
    cvar_t *var = d->var();
    if(var->type == CVT_FLOAT)
    {
        setValue(CVar_Float(var));
    }
    else
    {
        setRange(Rangei(var->min, var->max));
        setValue(CVar_Integer(var));
    }
}

void CVarSliderWidget::setCVarValueFromWidget()
{
    cvar_t *var = d->var();
    if(var->type == CVT_FLOAT)
    {
        CVar_SetFloat(d->var(), value());
    }
    else
    {
        CVar_SetInteger(d->var(), round<int>(value()));
    }
}
