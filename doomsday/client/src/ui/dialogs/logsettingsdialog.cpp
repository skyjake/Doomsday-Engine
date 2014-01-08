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
#include "ui/widgets/foldpanelwidget.h"
#include "SignalAction"
#include "clientapp.h"

using namespace de;

struct DomainText {
    char const *name;
    char const *label;
};
static DomainText const domainText[] = {
    { "generic",    "Minimum Level" },
    { "resource",   "Resources" },
    { "map",        "Map"       },
    { "script",     "Scripting" },
    { "gl",         "Graphics"  },
    { "audio",      "Audio"     },
    { "input",      "Input"     },
    { "network",    "Network"   }
};

#define NUM_DOMAINS (sizeof(domainText)/sizeof(domainText[0]))

DENG2_PIMPL(LogSettingsDialog)
, DENG2_OBSERVES(ToggleWidget, Toggle)
{
    ui::ListData levels;
    VariableToggleWidget *separately;
    FoldPanelWidget *fold;
    GridLayout foldLayout;
    IndirectRule *columnWidth; ///< Sync column width in and out of the fold.
    struct DomainWidgets
    {
        LabelWidget *label;
        VariableChoiceWidget *level;
        VariableToggleWidget *dev;
        VariableToggleWidget *alert;
    };
    DomainWidgets domWidgets[NUM_DOMAINS];

    Instance(Public *i) : Base(i)
    {
        try
        {
            zap(domWidgets);

            columnWidth = new IndirectRule;

            self.area().add(separately =
                    new VariableToggleWidget(tr("Filter by Subsystem"),
                                             App::config()["log.filterBySubsystem"]));

            levels << new ChoiceItem(        tr("1 - X.Verbose"), LogEntry::XVerbose)
                   << new ChoiceItem(        tr("2 - Verbose"),   LogEntry::Verbose )
                   << new ChoiceItem(        tr("3 - Message"),   LogEntry::Message )
                   << new ChoiceItem(        tr("4 - Note"),      LogEntry::Note    )
                   << new ChoiceItem(_E(D) + tr("5 - Warning"),   LogEntry::Warning )
                   << new ChoiceItem(_E(D) + tr("6 - Error"),     LogEntry::Error   )
                   << new ChoiceItem(_E(D) + tr("7 - Critical"),  LogEntry::Critical);

            // Folding panel for the per-domain settings.
            self.area().add(fold = new FoldPanelWidget);
            fold->setContent(new GuiWidget);
            fold->title().hide();

            foldLayout.setLeftTop(fold->content().rule().left(),
                                  fold->content().rule().top());
            foldLayout.setGridSize(4, 0);
            foldLayout.setColumnFixedWidth(1, *columnWidth);
            foldLayout.setColumnAlignment(0, ui::AlignRight);

            for(uint i = 0; i < NUM_DOMAINS; ++i)
            {
                initDomain(domainText[i],
                           domWidgets[i],
                           i == 0? &self.area() : &fold->content());
            }

            // This'll keep the dialog's size fixed even though the choices change size.
            columnWidth->setSource(domWidgets[0].level->maximumWidth());

            separately->audienceForToggle += this;
        }
        catch(Error const &er)
        {
            LOGDEV_ERROR("") << er.asText();
            deinit();
            throw;
        }
    }

    ~Instance()
    {
        deinit();
        releaseRef(columnWidth);
    }

    void initDomain(DomainText const &dom, DomainWidgets &wgt, GuiWidget *parent)
    {
        // Text label.
        wgt.label = LabelWidget::newWithText(tr(dom.label) + ":", parent);

        // Minimum level for log entries.
        parent->add(wgt.level =
                new VariableChoiceWidget(App::config()[String("log.filter.%1.minLevel").arg(dom.name)]));
        wgt.level->setItems(levels);
        wgt.level->updateFromVariable();
        QObject::connect(wgt.level, SIGNAL(selectionChangedByUser(uint)),
                         thisPublic, SLOT(updateLogFilter()));

        // Developer messages?
        parent->add(wgt.dev =
                new VariableToggleWidget(tr("Dev"), App::config()[String("log.filter.%1.allowDev").arg(dom.name)]));
        QObject::connect(wgt.dev, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)),
                         thisPublic, SLOT(updateLogFilter()));

        // Raise alerts?
        parent->add(wgt.alert =
                new VariableToggleWidget(tr("Alerts"), App::config()[String("alert.") + dom.name]));
        wgt.alert->setActiveValue(LogEntry::Warning);
        wgt.alert->setInactiveValue(LogEntry::HighestLogLevel + 1);

        // Lay out the folding panel's contents.
        if(parent == &fold->content())
        {
            foldLayout << *wgt.label << *wgt.level << *wgt.dev << *wgt.alert;
        }
    }

    void deinit()
    {
        // The common 'levels' will be deleted soon.
        for(uint i = 0; i < NUM_DOMAINS; ++i)
        {
            if(domWidgets[i].level)
            {
                domWidgets[i].level->useDefaultItems();
            }
        }
    }

    void overrideWithGeneric()
    {
        Config &cfg = App::config();
        LogFilter &logf = App::logFilter();

        // Check the generic filter settings.
        LogEntry::Level minLevel = LogEntry::Level(domWidgets[0].level->selectedItem().data().toInt());
        bool allowDev = domWidgets[0].dev->isActive();
        bool alerts = domWidgets[0].alert->isActive();

        // Override the individual filters with the generic one.
        logf.setMinLevel(LogEntry::AllDomains, minLevel);
        logf.setAllowDev(LogEntry::AllDomains, allowDev);

        // Update the variables (UI updated automatically).
        logf.write(cfg.names().subrecord("log.filter"));
        for(uint i = 0; i < NUM_DOMAINS; ++i)
        {
            char const *name = domainText[i].name;
            cfg.set(String("alert.") + name, int(alerts? LogEntry::Warning : (LogEntry::HighestLogLevel + 1)));
        }
    }

    void toggleStateChanged(ToggleWidget &toggle)
    {
        if(toggle.isActive())
        {
            overrideWithGeneric();
            fold->open();
        }
        else
        {
            fold->close();
            overrideWithGeneric();
        }
    }

    void updateLogFilter()
    {
        if(separately->isInactive())
        {
            overrideWithGeneric();
        }

        // Re-read from Config, which has been changed via the widgets.
        App::logFilter().read(App::config().names().subrecord("log.filter"));
    }
};

LogSettingsDialog::LogSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("Log Filter & Alerts"));

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(4, 0);
    layout.setColumnFixedWidth(1, *d->columnWidth);
    layout.setColumnAlignment(0, ui::AlignRight);

    layout << *d->domWidgets[0].label
           << *d->domWidgets[0].level
           << *d->domWidgets[0].dev
           << *d->domWidgets[0].alert
           << Const(0);
    layout.append(*d->separately, 3);
    layout.append(*d->fold, 4);

    // Fold's layout is complete.
    d->fold->content().rule().setSize(d->foldLayout.width(), d->foldLayout.height());

    // Dialog content size.
    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    if(d->separately->isActive())
    {
        d->fold->open();
    }
}

void LogSettingsDialog::resetToDefaults()
{
    ClientApp::logSettings().resetToDefaults();
}

void LogSettingsDialog::updateLogFilter()
{
    d->updateLogFilter();
}
