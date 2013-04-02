/** @file localserverdialog.h  Dialog for starting a local server.
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

#include "localserverdialog.h"
#include "folderselection.h"
#include "guishellapp.h"
#include <de/libdeng2.h>
#include <de/Socket>
#include <de/shell/DoomsdayInfo>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QTabWidget>
#include <QFileDialog>
#include <QSettings>
#include "preferences.h"

using namespace de;
using namespace de::shell;

DENG2_PIMPL(LocalServerDialog)
{
    QPushButton *yes;
    QLineEdit *name;
    QComboBox *games;
    QLineEdit *port;
    QLabel *portMsg;
    QTextEdit *options;
    FolderSelection *runtime;
    bool portChanged;

    Instance(Public &i) : Base(i), portChanged(false)
    {
#ifdef WIN32
        self.setWindowFlags(self.windowFlags() & ~Qt::WindowContextHelpButtonHint);
#endif

        QSettings st;

        self.setWindowTitle(tr("Start Local Server"));

        QVBoxLayout *mainLayout = new QVBoxLayout;
        self.setLayout(mainLayout);

        QTabWidget *tabs = new QTabWidget;
        mainLayout->addWidget(tabs, 1);

        QWidget *gameTab = new QWidget;
        QFormLayout *form = new QFormLayout;
        gameTab->setLayout(form);
        tabs->addTab(gameTab, tr("&Settings"));

        name = new QLineEdit;
        name->setMinimumWidth(240);
        name->setText(st.value("LocalServer/name", "Doomsday").toString());
        form->addRow(tr("Name:"), name);

        games = new QComboBox;
        games->setEditable(false);
        foreach(DoomsdayInfo::GameMode const &mode, DoomsdayInfo::allGameModes())
        {
            games->addItem(mode.title, mode.option);
        }
        games->setCurrentIndex(games->findData(st.value("LocalServer/gameMode", "doom1-share")));
        form->addRow(tr("&Game mode:"), games);

        QPushButton *opt = new QPushButton(tr("Game &Options..."));
        opt->setDisabled(true);
        form->addRow(0, opt);

        QHBoxLayout *hb = new QHBoxLayout;
        port = new QLineEdit;
        port->setMinimumWidth(80);
        port->setMaximumWidth(80);
        port->setText(QString::number(st.value("LocalServer/port", 13209).toInt()));
        /*
        // Find an unused port.
        if(isPortInUse())
        {
            for(int tries = 20; tries > 0; --tries)
            {
                port->setText(QString::number(portNumber() + 1));
                if(!isPortInUse())
                {
                    break;
                }
            }
        }
        */
        portChanged = false;
        port->setToolTip(tr("The default port is 13209."));
        portMsg = new QLabel;
        QPalette pal = portMsg->palette();
        pal.setColor(portMsg->foregroundRole(), Qt::red);
        portMsg->setPalette(pal);
        hb->addWidget(port, 0);
        hb->addWidget(portMsg, 1);
        portMsg->hide();
        form->addRow(tr("TCP port:"), hb);

        QWidget *advancedTab = new QWidget;
        form = new QFormLayout;
        advancedTab->setLayout(form);
        tabs->addTab(advancedTab, tr("&Advanced"));

        runtime = new FolderSelection(tr("Select Runtime Folder"));
        runtime->setPath(st.value("LocalServer/runtime").toString());
        if(runtime->path().isEmpty())
        {
            runtime->setPath(DoomsdayInfo::defaultServerRuntimeFolder().toString());
        }
        form->addRow(tr("Runtime folder:"), runtime);
        QObject::connect(runtime, SIGNAL(selected()), &self, SLOT(validate()));

        options = new QTextEdit;
        options->setTabChangesFocus(true);
        options->setAcceptRichText(false);
        options->setMinimumWidth(300);
        options->setMaximumHeight(QFontMetrics(options->font()).lineSpacing() * 5);
        options->setText(st.value("LocalServer/options").toString());
        form->addRow(tr("Options:"), options);

        QDialogButtonBox *bbox = new QDialogButtonBox;
        mainLayout->addWidget(bbox);
        yes = bbox->addButton(tr("&Start Server"), QDialogButtonBox::YesRole);
        QPushButton* no = bbox->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
        QObject::connect(yes, SIGNAL(clicked()), &self, SLOT(accept()));
        QObject::connect(no, SIGNAL(clicked()), &self, SLOT(reject()));
        QObject::connect(opt, SIGNAL(clicked()), &self, SLOT(configureGameOptions()));
        yes->setDefault(true);
    }

    int portNumber() const
    {
        QString txt = port->text().trimmed();
        return txt.toInt();
    }

    bool isPortInUse() const
    {
        int const portNum = portNumber();
        foreach(Address const &sv, GuiShellApp::app().serverFinder().foundServers())
        {
            if(sv.isLocal() && sv.port() == portNum)
            {
                return true;
            }
        }
        return false;
    }
};

LocalServerDialog::LocalServerDialog(QWidget *parent)
    : QDialog(parent), d(new Instance(*this))
{
    connect(d->port, SIGNAL(textChanged(QString)), this, SLOT(validate()));
    connect(d->port, SIGNAL(textEdited(QString)), this, SLOT(portChanged())); // causes port to be saved
    connect(this, SIGNAL(accepted()), this, SLOT(saveState()));
    connect(&GuiShellApp::app().serverFinder(), SIGNAL(updated()), this, SLOT(validate()));

    validate();
}

quint16 LocalServerDialog::port() const
{
    return d->port->text().toInt();
}

String LocalServerDialog::name() const
{
    return d->name->text();
}

QString LocalServerDialog::gameMode() const
{
    return d->games->itemData(d->games->currentIndex()).toString();
}

QStringList LocalServerDialog::additionalOptions() const
{
    QStringList opts = d->options->toPlainText().split(' ', QString::SkipEmptyParts);
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
    QSettings st;
    st.setValue("LocalServer/name", d->name->text());
    st.setValue("LocalServer/gameMode", d->games->itemData(d->games->currentIndex()).toString());
    if(d->portChanged)
    {
        st.setValue("LocalServer/port", d->port->text().toInt());
    }
    st.setValue("LocalServer/runtime", d->runtime->path().toString());
    st.setValue("LocalServer/options", d->options->toPlainText());
}

void LocalServerDialog::validate()
{
    bool isValid = true;

    // Check port.
    int port = d->portNumber();
    if(d->port->text().isEmpty() || port < 0 || port >= 0x10000)
    {
        isValid = false;
        d->portMsg->setText(tr("Must be between 0 and 65535."));
        d->portMsg->show();
    }
    else
    {
        // Check known running servers.
        bool inUse = d->isPortInUse();
        if(inUse)
        {
            isValid = false;
            d->portMsg->setText(tr("Port already in use."));
        }
        d->portMsg->setVisible(inUse);
    }

    if(d->runtime->path().isEmpty()) isValid = false;

    d->yes->setEnabled(isValid);
    if(isValid) d->yes->setDefault(true);
}
