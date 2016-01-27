/** @file nogamescolumnwidget.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/nogamescolumnwidget.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"

#include <de/ButtonWidget>
#include <de/SignalAction>

using namespace de;

NoGamesColumnWidget::NoGamesColumnWidget()
    : ColumnWidget("nogames-column")
{
    LabelWidget *notice = LabelWidget::newWithText(
                _E(b) + tr("No playable games were found.\n") + _E(.) +
                tr("Please select the folder where you have one or more game WAD files."),
                this);
    notice->setMaximumTextWidth(maximumContentWidth());
    notice->setTextColor("text");
    notice->setSizePolicy(ui::Expand, ui::Expand);
    notice->rule()
            .setMidAnchorX(rule().midX())
            .setInput(Rule::Bottom, rule().midY());

    ButtonWidget *chooseIwad = new ButtonWidget;
    chooseIwad->setText(tr("Select IWAD Folder..."));
    chooseIwad->setSizePolicy(ui::Expand, ui::Expand);
    chooseIwad->rule()
            .setMidAnchorX(rule().midX())
            .setInput(Rule::Top, notice->rule().bottom());
    chooseIwad->setAction(new SignalAction(this, SLOT(browseForDataFiles())));
    add(chooseIwad);
}

void NoGamesColumnWidget::browseForDataFiles()
{
    ClientWindow::main().taskBar().chooseIWADFolder();
}
