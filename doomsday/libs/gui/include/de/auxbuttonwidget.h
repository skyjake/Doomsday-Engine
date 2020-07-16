/** @file auxbuttonwidget.h  Button with an auxiliary button inside.
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

#ifndef LIBAPPFW_AUXBUTTONWIDGET_H
#define LIBAPPFW_AUXBUTTONWIDGET_H

#include <de/buttonwidget.h>

namespace de {

/**
 * Button with another small auxiliary button inside.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC AuxButtonWidget : public ButtonWidget
{
public:
    AuxButtonWidget(const String &name = String());

    ButtonWidget &auxiliary();

    void useNormalStyle();
    void invertStyle();

protected:
    void updateStyle();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_AUXBUTTONWIDGET_H
