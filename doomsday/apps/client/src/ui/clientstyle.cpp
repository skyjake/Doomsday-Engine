/** @file clientstyle.cpp  Client UI style.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/clientstyle.h"
#include "ui/clientwindow.h"

using namespace de;

DENG2_PIMPL_NOREF(ClientStyle)
{
    struct EmptyMenuLabelStylist : public ui::Stylist
    {
        void applyStyle(GuiWidget &widget) override
        {
            LabelWidget &label = widget.as<LabelWidget>();
            label.setFont("menu.empty");
            label.setOpacity(0.5f);
        }
    }
    emptyMenuLabelStylist;
};

ClientStyle::ClientStyle() : d(new Impl)
{}

GuiWidget *ClientStyle::sharedBlurWidget() const
{
    if (!ClientWindow::mainExists()) return nullptr;
    return &ClientWindow::main().taskBarBlur();
}

ui::Stylist &ClientStyle::emptyMenuLabelStylist() const
{
    return d->emptyMenuLabelStylist;
}

