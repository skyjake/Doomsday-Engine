/** @file nogamescolumnwidget.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "clientapp.h"

#include <de/buttonwidget.h>
#include <de/config.h>
#include <de/filedialog.h>
#include <de/textvalue.h>

using namespace de;

NoGamesColumnWidget::NoGamesColumnWidget()
    : ColumnWidget("nogames-column")
{
    header().hide();

    LabelWidget *notice = LabelWidget::newWithText(
                _E(b) "No playable games were found.\n" _E(.)
                "Please select the folder where you have one or more game WAD files.",
                this);
    notice->setMaximumTextWidth(maximumContentWidth());
    notice->setTextColor("text");
    notice->setSizePolicy(ui::Expand, ui::Expand);
    notice->rule()
            .setMidAnchorX(rule().midX())
            .setInput(Rule::Bottom, rule().midY());

    ButtonWidget *chooseIwad = new ButtonWidget;
    chooseIwad->setText("Select WAD Folder...");
    chooseIwad->setSizePolicy(ui::Expand, ui::Expand);
    chooseIwad->rule()
            .setMidAnchorX(rule().midX())
            .setInput(Rule::Top, notice->rule().bottom());
    chooseIwad->setActionFn([this](){ browseForDataFiles(); });
    add(chooseIwad);
}

String NoGamesColumnWidget::tabHeading() const
{
    return "Data Files?";
}

void NoGamesColumnWidget::browseForDataFiles()
{
    bool reload = false;

    auto &cfg = Config::get();

    FileDialog dlg;
    dlg.setTitle("Select IWAD Folder");
    if (const auto folders = cfg.getStringList("resource.packageFolder"))
    {
        dlg.setInitialLocation(folders.back());
    }
    dlg.setBehavior(FileDialog::AcceptDirectories, ReplaceFlags);
    dlg.setPrompt("Select");
    if (dlg.exec(root()))
    {
        Variable &var = Config::get("resource.packageFolder");
        const TextValue selDir{dlg.selectedPath().toString()};
        if (is<ArrayValue>(var.value()))
        {
            auto &array = var.value<ArrayValue>();
            int found = array.indexOf(TextValue(selDir));
            if (found >= 0)
            {
                array.remove(found);
            }
            array.add(selDir);
        }
        else
        {
            var.set(selDir);
        }
        reload = true;
    }

    // Reload packages and recheck for game availability.
    if (reload)
    {
        ClientWindow::main().console().closeLogAndUnfocusCommandLine();
        DoomsdayApp::app().initPackageFolders();
    }
}
