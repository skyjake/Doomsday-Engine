/** @file aboutdialog.cpp  Dialog for information about the program.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "aboutdialog.h"
#include <de/shell/LabelWidget>

using namespace de;
using namespace de::shell;

AboutDialog::AboutDialog()
{
    LabelWidget *label = new LabelWidget;
    label->setLabel(tr("Doomsday Shell %1\nCopyright (c) %2\n\n"
                       "The Shell is a utility for controlling and monitoring "
                       "Doomsday servers using a text-based (curses) user interface.")
                    .arg(SHELL_VERSION)
                    .arg("2013 Deng Team"));

    label->setExpandsToFitLines(true);
    label->rule()
            .setLeftTop(rule().left(), rule().top())
            .setInput(Rule::Width, rule().width());

    add(label);

    rule().setSize(Const(40), label->rule().height());
}

bool AboutDialog::handleEvent(Event const &event)
{
    if(event.type() == Event::KeyPress)
    {
        accept();
        return true;
    }

    return DialogWidget::handleEvent(event);
}
