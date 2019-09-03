/** @file localserverdialog.h  Dialog for starting a local server.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "localserverdialog.h"
#include "folderselection.h"
#include "guishellapp.h"

#include <de/ChoiceWidget>
#include <de/CommandLine>
#include <de/Config>
#include <de/LineEditWidget>
#include <de/Socket>
#include <de/TextValue>
#include <de/ToggleWidget>
#include <de/FoldPanelWidget>
#include <doomsday/DoomsdayInfo>

#include "preferences.h"

using namespace de;

DE_GUI_PIMPL(LocalServerDialog)
, DE_OBSERVES(ServerFinder, Update)
{
    ButtonWidget *   yes;
    LineEditWidget * name;
    ChoiceWidget *   games;
    LineEditWidget * port;
    LabelWidget *    portMsg;
    ToggleWidget *   announce;
    LineEditWidget * password;
    LabelWidget *    passwordMsg;
    LineEditWidget * options;
    FolderSelection *runtime;
    FoldPanelWidget *advanced;
    bool             portChanged = false;

    Impl(Public *i) : Base(i)
    {
        auto &cfg   = Config::get();
        auto &area  = self().area();
        auto &rect  = area.contentRule();
        auto &width = rule("unit") * 100;

        GridLayout layout(rect.left(), rect.top());
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight);

        name = &area.addNew<LineEditWidget>();
        name->rule().setInput(Rule::Width, width);
        name->setText(cfg.gets("LocalServer.name", "Doomsday"));
        layout << *LabelWidget::newWithText("Name:", &area) << *name;

        games = &area.addNew<ChoiceWidget>();
        for (const DoomsdayInfo::Game &mode : DoomsdayInfo::allGames())
        {
            games->items() << new ChoiceItem(mode.title, TextValue(mode.option));
        }
        games->setSelected(
            games->items().findData(TextValue(cfg.gets("LocalServer.gameMode", "doom1-share"))));
        layout << *LabelWidget::newWithText("Game Mode:", &area)
               << *games;

        /*QPushButton *opt = new QPushButton(tr("Game &Options..."));
        opt->setDisabled(true);
        form->addRow(0, opt);*/

        port = &area.addNew<LineEditWidget>();
        port->rule().setInput(Rule::Width, rule("unit") * 50);
        port->setText(String::asText(cfg.getui("LocalServer.port", DEFAULT_PORT)));

        /*
        // Find an unused port.
        if (isPortInUse())
        {
            for (int tries = 20; tries > 0; --tries)
            {
                port->setText(QString::number(portNumber() + 1));
                if (!isPortInUse())
                {
                    break;
                }
            }
        }
        */

        portChanged = false;
//        port->setToolTip(tr("The default port is %1.").arg(DEFAULT_PORT));
        portMsg = &area.addNew<LabelWidget>();
//        portMsg->setPalette(pal);
//        hb->addWidget(port, 0);
//        hb->addWidget(portMsg, 1);
        portMsg->hide();
//        form->addRow(tr("TCP port:"), hb);
        auto *tcpLabel = LabelWidget::newWithText("TCP Port:", &area);
        layout << *tcpLabel << *port
               << tcpLabel->rule().width() << *portMsg;

        announce = &area.addNew<ToggleWidget>();
        announce->setText("Public server: visible to all");
        announce->setActive(cfg.getb("LocalServer.announce", false));
        layout << Const(0) << *announce;

        password = &area.addNew<LineEditWidget>();
        password->rule().setInput(Rule::Width, rule("unit") * 80);
        password->setText(cfg.gets("LocalServer.password", ""));

        passwordMsg = &area.addNew<LabelWidget>();
        passwordMsg->setTextColor("accent");
        passwordMsg->hide();
        layout << *LabelWidget::newWithText("Shell Password:", &area) << *password
               << Const(0) << *passwordMsg;

        // Fold panel for advanced settings.
        {
            advanced = &area.addNew<FoldPanelWidget>();
            auto *content = new GuiWidget;
            advanced->setContent(content);

            GridLayout adLayout(content->rule().left(), content->rule().top());
            adLayout.setGridSize(2, 0);
            adLayout.setColumnAlignment(0, ui::AlignRight);

            runtime = &content->addNew<FolderSelection>("Select Runtime Folder");
            runtime->setPath(cfg.gets("LocalServer.runtime", ""));
            if (runtime->path().isEmpty())
            {
                runtime->setPath(DoomsdayInfo::defaultServerRuntimeFolder());
            }
            adLayout << *LabelWidget::newWithText("Runtime Folder:", content)
                     << *runtime;
            runtime->audienceForSelection() += [this](){ self().validate(); };

            options = &content->addNew<LineEditWidget>();
            options->rule().setInput(Rule::Width, width);
            options->setText(cfg.gets("LocalServer.options", ""));
            adLayout << *LabelWidget::newWithText("Options:", content)
                     << *options;

            content->rule().setSize(adLayout);

            auto *foldButton = advanced->makeTitle("Advanced Options");
            area.add(foldButton);

            layout.append(*foldButton, 2);
            advanced->rule().setLeftTop(foldButton->rule().left(),
                                        foldButton->rule().bottom());
        }

        area.setContentSize(layout.width(), layout.height() + advanced->rule().height());
    }

    int portNumber() const
    {
        return port->text().strip().toInt();
    }

    bool isPortInUse() const
    {
        const int portNum = portNumber();
        for (const auto &sv : GuiShellApp::app().serverFinder().foundServers())
        {
            if (sv.isLocal() && sv.port() == portNum)
            {
                return true;
            }
        }
        return false;
    }

    void foundServersUpdated() override
    {
        self().validate();
    }
};

LocalServerDialog::LocalServerDialog()
    : DialogWidget("startlocalserver", WithHeading)
    , d(new Impl(this))
{
    heading().setText("Start Local Server");

    buttons()
        << new DialogButtonItem(Id1 | Default | Accept, "Start Server")
        << new DialogButtonItem(Reject, "Cancel");
    d->yes = buttonWidget(Id1);

    GuiShellApp::app().serverFinder().audienceForUpdate() += d;
    d->port->audienceForContentChange() += [this]() {
        d->portChanged = true;
        validate();
    };
    d->announce->audienceForToggle() += [this]() { validate(); };
    d->password->audienceForContentChange() += [this]() { validate(); };
    audienceForAccept() += [this]() { saveState(); };
    validate();
}

uint16_t LocalServerDialog::port() const
{
    return uint16_t(d->port->text().toInt());
}

String LocalServerDialog::name() const
{
    return d->name->text();
}

String LocalServerDialog::gameMode() const
{
    return d->games->selectedItem().data().asText();
}

StringList LocalServerDialog::additionalOptions() const
{
    StringList opts;
    opts << "-cmd" << Stringf("server-password \"%s\"", d->password->text().escaped().c_str());
    opts << "-cmd" << Stringf("server-public %d", d->announce->isActive() ? 1 : 0);

    // Parse the provided options using libcore so quotes and other special
    // behavior matches Doomsday.
    CommandLine cmdLine;
    cmdLine.parse(d->options->text());
    for (dsize i = 0; i < cmdLine.count(); ++i)
    {
        opts << cmdLine.at(i);
    }
    return opts;
}

NativePath LocalServerDialog::runtimeFolder() const
{
    return d->runtime->path();
}

void LocalServerDialog::portChanged()
{
    d->portChanged = true;
}

void LocalServerDialog::configureGameOptions()
{
}

void LocalServerDialog::saveState()
{
    auto &cfg = Config::get();

    cfg.set("LocalServer.name", d->name->text());
    cfg.set("LocalServer.gameMode", d->games->selectedItem().data().asText());
    if (d->portChanged)
    {
        cfg.set("LocalServer.port", d->port->text().toInt());
    }
    cfg.set("LocalServer.announce", d->announce->isActive());
    cfg.set("LocalServer.password", d->password->text());
    cfg.set("LocalServer.runtime", d->runtime->path().toString());
    cfg.set("LocalServer.options", d->options->text());
}

void LocalServerDialog::validate()
{
    bool isValid = true;

    // Check port.
    int port = d->portNumber();
    if (d->port->text().isEmpty() || port < 0 || port >= 0x10000)
    {
        isValid = false;
        d->portMsg->setText("Must be between 0 and 65535.");
        d->portMsg->show();
    }
    else
    {
        // Check known running servers.
        bool inUse = d->isPortInUse();
        if (inUse)
        {
            isValid = false;
            d->portMsg->setText("Port already in use.");
        }
        d->portMsg->show(inUse);
    }

    if (d->announce->isActive() && d->password->text().isEmpty())
    {
        isValid = false;
        d->passwordMsg->show();
        d->passwordMsg->setText("Required.");
    }
    else
    {
        d->passwordMsg->hide();
    }

    if (d->runtime->path().isEmpty()) isValid = false;

    d->yes->enable(isValid);
//    if (isValid) d->yes->setDefault(true);
}
