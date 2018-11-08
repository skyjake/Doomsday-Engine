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
#include "ConfigProfiles"

#include <de/CallbackAction>
#include <de/Config>
#include <de/GridLayout>
#include <de/VariableChoiceWidget>
#include <de/VariableToggleWidget>

using namespace de;

DENG2_PIMPL(UISettingsDialog)
{
    VariableChoiceWidget *uiScale;
    VariableToggleWidget *uiTranslucency;
    VariableToggleWidget *showAnnotations;
//    VariableToggleWidget *showColumnDescription;
//    VariableToggleWidget *showUnplayable;
    VariableToggleWidget *showDoom;
    VariableToggleWidget *showHeretic;
    VariableToggleWidget *showHexen;
    VariableToggleWidget *showOther;
    VariableToggleWidget *showMultiplayer;

    Impl(Public *i) : Base(i)
    {
        auto &area = self().area();

        area.add(uiScale               = new VariableChoiceWidget(Config::get("ui.scaleFactor"), VariableChoiceWidget::Number));
        area.add(uiTranslucency        = new VariableToggleWidget(tr("Background Translucency"), Config::get("ui.translucency")));
        area.add(showAnnotations       = new VariableToggleWidget(tr("Menu Annotations"),  Config::get("ui.showAnnotations")));
//        area.add(showColumnDescription = new VariableToggleWidget(tr("Game Descriptions"), Config::get("home.showColumnDescription")));
//        area.add(showUnplayable        = new VariableToggleWidget(tr("Unplayable Games"),  Config::get("home.showUnplayableGames")));
        area.add(showDoom              = new VariableToggleWidget(tr("Doom"),              Config::get("home.columns.doom")));
        area.add(showHeretic           = new VariableToggleWidget(tr("Heretic"),           Config::get("home.columns.heretic")));
        area.add(showHexen             = new VariableToggleWidget(tr("Hexen"),             Config::get("home.columns.hexen")));
        area.add(showOther             = new VariableToggleWidget(tr("Other Games"),       Config::get("home.columns.otherGames")));
        area.add(showMultiplayer       = new VariableToggleWidget(tr("Multiplayer"),       Config::get("home.columns.multiplayer")));

        uiScale->items()
                << new ChoiceItem(tr("Huge (200%)"),   2.0)
                << new ChoiceItem(tr("Large (150%)"),  1.5)
                << new ChoiceItem(tr("Normal (100%)"), 1.0)
                << new ChoiceItem(tr("Small (90%)"),   0.9)
                << new ChoiceItem(tr("Tiny (80%)"),    0.8);
        uiScale->updateFromVariable();
    }

    void resetToDefaults()
    {
        ClientApp::uiSettings().resetToDefaults();
    }
};

UISettingsDialog::UISettingsDialog(String const &name)
    : DialogWidget(name, WithHeading)
    , d(new Impl(this))
{
    heading().setText(tr("UI Settings"));
    heading().setImage(style().images().image("home.icon"));

//    auto *library = LabelWidget::appendSeparatorWithText(_E(D) + tr("Game Library"), &area());
//    library->setFont("separator.label");

    auto *restartNotice = LabelWidget::newWithText(tr("Changes take effect only after restarting."), &area());
    restartNotice->margins().setTop("");
    restartNotice->setFont("separator.annotation");
    restartNotice->setTextColor("altaccent");

    d->showAnnotations->margins().setBottom(RuleBank::UNIT);

    auto *annots = LabelWidget::newWithText(tr("Annotations briefly describe menu functions."), &area());
    annots->margins().setTop("");
    annots->setFont("separator.annotation");
    annots->setTextColor("altaccent");

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);
    layout << *LabelWidget::newWithText(tr("Scale:"), &area()) << *d->uiScale
           << Const(0) << *d->uiTranslucency
           << Const(0) << *restartNotice
           << Const(0) << *d->showAnnotations
           << Const(0) << *annots;
//    layout.setCellAlignment(Vector2i(0, layout.gridSize().y), ui::AlignLeft);
//    layout.append(*library, 2);
    auto *library = LabelWidget::appendSeparatorWithText("Game Library", &area(), &layout);

    auto *showLabel = LabelWidget::newWithText(tr("Enabled Tabs:"), &area());
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
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new CallbackAction([this] () { d->resetToDefaults(); }));
}
