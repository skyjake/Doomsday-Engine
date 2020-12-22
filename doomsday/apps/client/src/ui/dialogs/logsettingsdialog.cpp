/** @file logsettingsdialog.cpp  Dialog for Log settings.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "clientapp.h"
#include "configprofiles.h"

#include <de/config.h>
#include <de/foldpanelwidget.h>
#include <de/logfilter.h>
#include <de/variablechoicewidget.h>
#include <de/variabletogglewidget.h>

using namespace de;

struct DomainText {
    const char *name;
    const char *label;
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

DE_PIMPL(LogSettingsDialog)
, DE_OBSERVES(ToggleWidget, Toggle)
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

    Impl(Public *i) : Base(i)
    {
        try
        {
            zap(domWidgets);

            columnWidth = new IndirectRule;

            separately = new VariableToggleWidget("Filter by Subsystem",
                                                  App::config("log.filterBySubsystem"));

            levels << new ChoiceItem(      "1 - X.Verbose", NumberValue(LogEntry::XVerbose))
                   << new ChoiceItem(      "2 - Verbose",   NumberValue(LogEntry::Verbose ))
                   << new ChoiceItem(      "3 - Message",   NumberValue(LogEntry::Message ))
                   << new ChoiceItem(      "4 - Note",      NumberValue(LogEntry::Note    ))
                   << new ChoiceItem(_E(D) "5 - Warning",   NumberValue(LogEntry::Warning ))
                   << new ChoiceItem(_E(D) "6 - Error",     NumberValue(LogEntry::Error   ))
                   << new ChoiceItem(_E(D) "7 - Critical",  NumberValue(LogEntry::Critical));

            // Folding panel for the per-domain settings.
            fold = new FoldPanelWidget;
            fold->setContent(new GuiWidget);

            foldLayout.setLeftTop(fold->content().rule().left(),
                                  fold->content().rule().top());
            foldLayout.setGridSize(4, 0);
            foldLayout.setColumnFixedWidth(1, *columnWidth);
            foldLayout.setColumnAlignment(0, ui::AlignRight);

            for (uint i = 0; i < NUM_DOMAINS; ++i)
            {
                initDomain(domainText[i],
                           domWidgets[i],
                           i == 0? &self().area() : &fold->content());
            }
            self().area().add(separately);
            self().area().add(fold);

            // This'll keep the dialog's size fixed even though the choices change size.
            columnWidth->setSource(domWidgets[0].level->maximumWidth());

            separately->audienceForToggle() += this;
        }
        catch (const Error &er)
        {
            LOGDEV_ERROR("") << er.asText();
            deinit();
            throw;
        }
    }

    ~Impl()
    {
        deinit();
        releaseRef(columnWidth);
    }

    void initDomain(const DomainText &dom, DomainWidgets &wgt, GuiWidget *parent)
    {
        // Text label.
        wgt.label = LabelWidget::newWithText(CString(dom.label) + ":", parent);

        // Minimum level for log entries.
        parent->add(wgt.level = new VariableChoiceWidget(
                        Config::get(Stringf("log.filter.%s.minLevel", dom.name)),
                        VariableChoiceWidget::Number));
        wgt.level->setItems(levels);
        wgt.level->updateFromVariable();
        wgt.level->audienceForUserSelection() += [this](){ self().updateLogFilter(); };

        // Developer messages?
        parent->add(
            wgt.dev = new VariableToggleWidget(
                "Dev", Config::get(Stringf("log.filter.%s.allowDev", dom.name))));
        wgt.dev->audienceForUserToggle() += [this](){ self().updateLogFilter(); };

        // Raise alerts?
        parent->add(wgt.alert =
                new VariableToggleWidget("Alerts", Config::get(String("alert.") + dom.name)));
        wgt.alert->setActiveValue(LogEntry::Warning);
        wgt.alert->setInactiveValue(LogEntry::HighestLogLevel + 1);

        // Lay out the folding panel's contents.
        if (parent == &fold->content())
        {
            foldLayout << *wgt.label << *wgt.level << *wgt.dev << *wgt.alert;
        }
    }

    void deinit()
    {
        // The common 'levels' will be deleted soon.
        for (uint i = 0; i < NUM_DOMAINS; ++i)
        {
            if (domWidgets[i].level)
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
        LogEntry::Level minLevel = LogEntry::Level(domWidgets[0].level->selectedItem().data().asInt());
        bool allowDev = domWidgets[0].dev->isActive();
        bool alerts = domWidgets[0].alert->isActive();

        // Override the individual filters with the generic one.
        logf.setMinLevel(LogEntry::AllDomains, minLevel);
        logf.setAllowDev(LogEntry::AllDomains, allowDev);

        // Update the variables (UI updated automatically).
        logf.write(cfg.objectNamespace().subrecord("log.filter"));
        for (uint i = 0; i < NUM_DOMAINS; ++i)
        {
            const char *name = domainText[i].name;
            cfg.set(String("alert.") + name, int(alerts? LogEntry::Warning : (LogEntry::HighestLogLevel + 1)));
        }
    }

    void toggleStateChanged(ToggleWidget &toggle)
    {
        if (toggle.isActive())
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
        if (separately->isInactive())
        {
            overrideWithGeneric();
        }
        // Re-read from Config, which has been changed via the widgets.
        applyFilterFromConfig();
    }

    void applyFilterFromConfig()
    {
        App::logFilter().read(App::config().subrecord("log.filter"));
    }
};

LogSettingsDialog::LogSettingsDialog(const String &name)
    : DialogWidget(name, WithHeading), d(new Impl(this))
{
    heading().setText("Log Filter & Alerts");
    heading().setImage(style().images().image("log"));

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
    layout.append(*d->fold, 4)
           << Const(0);

    // Fold's layout is complete.
    d->fold->content().rule().setSize(d->foldLayout.width(), d->foldLayout.height());

    // Dialog content size.
    area().setContentSize(layout);

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, "Close")
            << new DialogButtonItem(DialogWidget::Action, "Reset to Defaults",
                                    [this](){ resetToDefaults(); });

    if (d->separately->isActive())
    {
        d->fold->open();
    }
}

void LogSettingsDialog::resetToDefaults()
{
    ClientApp::logSettings().resetToDefaults();

    d->applyFilterFromConfig();
}

void LogSettingsDialog::updateLogFilter()
{
    d->updateLogFilter();
}
