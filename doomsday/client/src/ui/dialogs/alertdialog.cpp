/** @file alertdialog.cpp  Dialog for listing recent alerts.
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

#include "ui/dialogs/alertdialog.h"
#include "ui/widgets/notificationwidget.h"
#include "ui/clientwindow.h"

using namespace de;

DENG_GUI_PIMPL(AlertDialog)
{
    ButtonWidget *notification;

    Instance(Public *i) : Base(i)
    {
        notification = new ButtonWidget;
        notification->setSizePolicy(ui::Expand, ui::Expand);
        notification->setImage(style().images().image("alert"));
        notification->setOverrideImageSize(style().fonts().font("default").height().value());
    }

    NotificationWidget &notifs()
    {
        return ClientWindow::main().notifications();
    }
};

AlertDialog::AlertDialog(String const &name) : d(new Instance(this))
{
    d->notifs().showOrHide(d->notification, true);
}

GuiWidget &AlertDialog::notification()
{
    return *d->notification;
}
