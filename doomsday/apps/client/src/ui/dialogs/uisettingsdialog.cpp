/** @file uisettingsdialog.cpp  User interface settings.
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

#include "ui/dialogs/uisettingsdialog.h"
#include "clientapp.h"
#include "configprofiles.h"

#include <de/callbackaction.h>
#include <de/config.h>
#include <de/gridlayout.h>
#include <de/variablechoicewidget.h>
#include <de/variabletogglewidget.h>

using namespace de;

DE_PIMPL(UISettingsDialog)
{
    VariableChoiceWidget *uiScale;
    VariableToggleWidget *uiTranslucency;
    VariableToggleWidget *showAnnotations;
    VariableToggleWidget *showDoom;
    VariableToggleWidget *showHeretic;
    VariableToggleWidget *showHexen;
    VariableToggleWidget *showOther;
    VariableToggleWidget *showMultiplayer;

    Impl(Public *i) : Base(i)
    {
        auto &area = self().area();

        area.add(uiScale               = new VariableChoiceWidget(Config::get("ui.scaleFactor"), VariableChoiceWidget::Number));
        area.add(uiTranslucency        = new VariableToggleWidget("Background Translucency", Config::get("ui.translucency")));
        area.add(showAnnotations       = new VariableToggleWidget("Menu Annotations",  Config::get("ui.showAnnotations")));
        area.add(showDoom              = new VariableToggleWidget("Doom",              Config::get("home.columns.doom")));
        area.add(showHeretic           = new VariableToggleWidget("Heretic",           Config::get("home.columns.heretic")));
        area.add(showHexen             = new VariableToggleWidget("Hexen",             Config::get("home.columns.hexen")));
        area.add(showOther             = new VariableToggleWidget("Other Games",       Config::get("home.columns.otherGames")));
        area.add(showMultiplayer       = new VariableToggleWidget("Multiplayer",       Config::get("home.columns.multiplayer")));

        uiScale->items() << new ChoiceItem("Double (200%)", 2.0) << new ChoiceItem("175%", 1.75)
                         << new ChoiceItem("150%", 1.5) << new ChoiceItem("125%", 1.25)
                         << new ChoiceItem("110%", 1.1) << new ChoiceItem("Normal (100%)", 1.0)
                         << new ChoiceItem("90%", 0.9) << new ChoiceItem("75%", 0.75)
                         << new ChoiceItem("Half (50%)", 0.5);
        uiScale->updateFromVariable();
    }

    void resetToDefaults()
    {
        ClientApp::uiSettings().resetToDefaults();
    }
};

UISettingsDialog::UISettingsDialog(const String &name)
    : DialogWidget(name, WithHeading)
    , d(new Impl(this))
{
    heading().setText("UI Settings");
    heading().setImage(style().images().image("home.icon"));

    d->showAnnotations->margins().setBottom(RuleBank::UNIT);

    auto *annots = LabelWidget::newWithText("Annotations briefly describe menu functions.", &area());
    annots->margins().setTop("");
    annots->setFont("separator.annotation");
    annots->setTextColor("altaccent");

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *LabelWidget::newWithText("Scale:", &area()) << *d->uiScale
           << Const(0) << *d->uiTranslucency
//           << Const(0) << *restartNotice
           << Const(0) << *d->showAnnotations
           << Const(0) << *annots;
//    layout.setCellAlignment(Vec2i(0, layout.gridSize().y), ui::AlignLeft);
//    layout.append(*library, 2);
    auto *library = LabelWidget::appendSeparatorWithText("Game Library", &area(), &layout);

    auto *showLabel = LabelWidget::newWithText("Enabled Tabs:", &area());
    showLabel->rule().setLeftTop(library->rule().left(), library->rule().bottom());

    GridLayout showLayout(showLabel->rule().right(), showLabel->rule().top(),
                          GridLayout::RowFirst);
    showLayout.setGridSize(2, 3);
    showLayout << *d->showDoom
               << *d->showHeretic
               << *d->showHexen
               << *d->showOther
               << *d->showMultiplayer;
//               << *d->showColumnDescription
//               << *d->showUnplayable;

    area().setContentSize(OperatorRule::maximum(layout.width(),
                                                showLabel->rule().width() + showLayout.width()),
                          layout.height() + showLayout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, "Close")
            << new DialogButtonItem(DialogWidget::Action, "Reset to Defaults",
                                    [this]() { d->resetToDefaults(); });
}
