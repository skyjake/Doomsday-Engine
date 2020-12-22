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

#include <de/choicewidget.h>
#include <de/commandline.h>
#include <de/config.h>
#include <de/lineeditwidget.h>
#include <de/socket.h>
#include <de/textvalue.h>
#include <de/togglewidget.h>
#include <de/foldpanelwidget.h>
#include <doomsday/doomsdayinfo.h>

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

        // Restore previous selection.
        auto sel = games->items().findData(TextValue(cfg.gets("LocalServer.gameMode", "doom1-share")));
        if (sel == ui::Data::InvalidPos)
        {
            sel = games->items().findData(TextValue("doom1-share"));
        }
        games->setSelected(sel);

        layout << *LabelWidget::newWithText("Game Mode:", &area)
               << *games;

        /*QPushButton *opt = new QPushButton(tr("Game &Options..."));
        opt->setDisabled(true);
        form->addRow(0, opt);*/

        port = &area.addNew<LineEditWidget>();
        port->rule().setInput(Rule::Width, rule("unit") * 25);
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
        portMsg->setTextColor("accent");
//        portMsg->setPalette(pal);
//        hb->addWidget(port, 0);
//        hb->addWidget(portMsg, 1);
        portMsg->hide();
//        form->addRow(tr("TCP port:"), hb);
        auto *tcpLabel = LabelWidget::newWithText("TCP Port:", &area);
        layout << *tcpLabel << *port;
        portMsg->rule().setLeftTop(port->rule().right(), port->rule().top());

        announce = &area.addNew<ToggleWidget>();
        announce->setText("Public server: visible to all");
        announce->setActive(cfg.getb("LocalServer.announce", false));
        layout << Const(0) << *announce;

        password = &area.addNew<LineEditWidget>();
        password->rule().setInput(Rule::Width, rule("unit") * 50);
        password->setText(cfg.gets("LocalServer.password", ""));

        passwordMsg = &area.addNew<LabelWidget>();
        passwordMsg->setTextColor("accent");
        passwordMsg->hide();
        layout << *LabelWidget::newWithText("Shell Password:", &area) << *password;
        passwordMsg->rule().setLeftTop(password->rule().right(), password->rule().top());

        ButtonWidget *foldButton;

        // Fold panel for advanced settings.
        {
            advanced = &area.addNew<FoldPanelWidget>();
            auto *content = new GuiWidget;
            advanced->setContent(content);

            GridLayout adLayout(content->rule().left(), content->rule().top());
            adLayout.setGridSize(2, 0);
            adLayout.setColumnAlignment(0, ui::AlignRight);

            runtime = &content->addNew<FolderSelection>("Select Runtime Folder");
            runtime->rule().setInput(Rule::Width, width);
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
            foldButton = advanced->makeTitle("Advanced Options");
            foldButton->setFont("separator.label");
            foldButton->rule().setInput(Rule::Right, rect.right());
            area.add(foldButton);

            foldButton->rule().setLeftTop(rect.left(), password->rule().bottom());
            advanced->rule().setLeftTop(foldButton->rule().left(),
                                        foldButton->rule().bottom());
        }

        area.setContentSize(layout.width(),
                            layout.height() + foldButton->rule().height() +
                                advanced->rule().height());
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
    // Replace previous state completely.
    Record &vars = Config::get().objectNamespace().addSubrecord("LocalServer");

    vars.set("name", d->name->text());
    vars.set("gameMode", d->games->selectedItem().data().asText());
    vars.set("port", d->port->text().toInt());
    vars.set("announce", d->announce->isActive());
    vars.set("password", d->password->text());
    vars.set("runtime", d->runtime->path().toString());
    vars.set("options", d->options->text());
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
