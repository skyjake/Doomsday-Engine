/** @file logsettingsdialog.cpp  Dialog for Log settings.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/dialogs/logsettingsdialog.h"
#include "ui/widgets/variablechoicewidget.h"
#include "ui/widgets/variabletogglewidget.h"
#include "SignalAction"

#include <de/App>

using namespace de;

DENG2_PIMPL(LogSettingsDialog)
{
    ui::ListData levels;
    VariableToggleWidget *separately;
    struct Domain {
        VariableChoiceWidget *level;
        VariableToggleWidget *alert;
    };
    Domain generic;
    Domain resource;
    Domain map;
    Domain script;
    Domain gfx;
    Domain audio;
    Domain input;
    Domain network;

    Instance(Public *i) : Base(i)
    {
        self.area().add(separately =
                new VariableToggleWidget(tr("Filter Separately"), App::config()["log.filterSeparately"]));

        levels << new ChoiceItem(tr("X-Verbose"), LogEntry::XVerbose)
               << new ChoiceItem(tr("Verbose"),   LogEntry::Verbose)
               << new ChoiceItem(tr("Message"),   LogEntry::Message)
               << new ChoiceItem(tr("Note"),      LogEntry::Note)
               << new ChoiceItem(tr("Warning"),   LogEntry::Warning)
               << new ChoiceItem(tr("Error"),     LogEntry::Error)
               << new ChoiceItem(tr("Critical"),  LogEntry::Critical);

        initDomain(generic,  "generic");
        initDomain(resource, "resource");
        initDomain(map,      "map");
        initDomain(script,   "script");
        initDomain(gfx,      "gl");
        initDomain(audio,    "audio");
        initDomain(input,    "input");
        initDomain(network,  "network");
    }

    void initDomain(Domain &dom, String const &name)
    {
        self.area().add(dom.level =
                new VariableChoiceWidget(App::config()["log.filter." + name + ".minLevel"]));
        dom.level->popup().menu().setItems(levels);
        dom.level->updateFromVariable();

        self.area().add(dom.alert =
                new VariableToggleWidget(tr("Alerts"), App::config()["alert." + name]));
        dom.alert->setActiveValue(LogEntry::Warning);
        dom.alert->setInactiveValue(LogEntry::MAX_LOG_LEVELS);
    }

    ~Instance()
    {
        // The common 'levels' will be deleted soon.
        generic.level->popup().menu().useDefaultItems();
        resource.level->popup().menu().useDefaultItems();
        map.level->popup().menu().useDefaultItems();
        script.level->popup().menu().useDefaultItems();
        gfx.level->popup().menu().useDefaultItems();
        audio.level->popup().menu().useDefaultItems();
        input.level->popup().menu().useDefaultItems();
        network.level->popup().menu().useDefaultItems();
    }
};

LogSettingsDialog::LogSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Log Filter & Alerts"));

    LabelWidget *levelLabel  = LabelWidget::newWithText(tr("Level:"), &area());
    LabelWidget *resLabel    = LabelWidget::newWithText(tr("Resources:"), &area());
    LabelWidget *mapLabel    = LabelWidget::newWithText(tr("Map:"), &area());
    LabelWidget *scriptLabel = LabelWidget::newWithText(tr("Scripting:"), &area());
    LabelWidget *glLabel     = LabelWidget::newWithText(tr("Graphics:"), &area());
    LabelWidget *audioLabel  = LabelWidget::newWithText(tr("Audio:"), &area());
    LabelWidget *inputLabel  = LabelWidget::newWithText(tr("Input:"), &area());
    LabelWidget *netLabel    = LabelWidget::newWithText(tr("Network:"), &area());

    /*
    LabelWidget *modeLabel     = LabelWidget::newWithText(tr("Stereo Mode:"), &area());
    LabelWidget *heightLabel   = LabelWidget::newWithText(tr("Height (m):"), &area());
    LabelWidget *ipdLabel      = LabelWidget::newWithText(tr("IPD (mm):"), &area());
    LabelWidget *dominantLabel = LabelWidget::newWithText(tr("Dominant Eye:"), &area());
    */

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(3, 0);
    layout.setColumnAlignment(0, ui::AlignRight);

    layout << *levelLabel << *d->generic.level << *d->generic.alert
           << Const(0);
    layout.append(*d->separately, 2);

    layout << *resLabel    << *d->resource.level << *d->resource.alert
           << *mapLabel    << *d->map.level      << *d->map.alert
           << *scriptLabel << *d->script.level   << *d->script.alert
           << *glLabel     << *d->gfx.level      << *d->gfx.alert
           << *audioLabel  << *d->audio.level    << *d->audio.alert
           << *inputLabel  << *d->input.level    << *d->input.alert
           << *netLabel    << *d->network.level  << *d->network.alert;

    /*
    layout << *modeLabel     << *d->mode
           << *heightLabel   << *d->humanHeight
           << *ipdLabel      << *d->ipd
           << *dominantLabel << *d->dominantEye
           << Const(0)       << *d->swapEyes;
           */

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));
}

void LogSettingsDialog::resetToDefaults()
{
}
