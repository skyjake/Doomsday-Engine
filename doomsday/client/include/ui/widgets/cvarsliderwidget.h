/** @file cvarsliderwidget.h  Slider for adjusting a cvar.
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

#ifndef DENG_CLIENT_CVARSLIDERWIDGET_H
#define DENG_CLIENT_CVARSLIDERWIDGET_H

#include <de/SliderWidget>
#include "icvarwidget.h"

/**
 * Console variable slider.
 */
class CVarSliderWidget : public de::SliderWidget, public ICVarWidget
{
    Q_OBJECT

public:
    CVarSliderWidget(char const *cvarPath);

    char const *cvarPath() const;

public slots:
    void updateFromCVar();
    void setCVarValueFromWidget();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_CVARSLIDERWIDGET_H
