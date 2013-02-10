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
#include <de/libdeng2.h>
#include <de/shell/DoomsdayInfo>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>

using namespace de;
using namespace de::shell;

DENG2_PIMPL(LocalServerDialog)
{
    QPushButton *yes;
    QComboBox *games;
    QLineEdit *port;
    QLineEdit *options;
    QLineEdit *folder;
    QLineEdit *iwadFolder;

    Instance(Public &i) : Private(i)
    {
        QSettings st;

        self.setWindowTitle(tr("Start Local Server"));

        QVBoxLayout *mainLayout = new QVBoxLayout;
        self.setLayout(mainLayout);

        QFormLayout *form = new QFormLayout;
        mainLayout->addLayout(form);

        games = new QComboBox;
        games->setEditable(false);
        foreach(DoomsdayInfo::GameMode const &mode, DoomsdayInfo::allGameModes())
        {
            games->addItem(mode.title, mode.option);
        }
        games->setCurrentIndex(games->findData(st.value("LocalServer/gameMode", "doom1-share")));
        form->addRow(tr("Game mode:"), games);

        port = new QLineEdit;
        port->setMaximumWidth(80);
        port->setText(QString::number(st.value("LocalServer/port", 13209).toInt()));
        port->setToolTip(tr("Port must be between 0 and 65535."));
        form->addRow(tr("TCP port:"), port);

        folder = new QLineEdit;
        folder->setMinimumWidth(300);
        folder->setText(st.value("LocalServer/runtime").toString());
        if(folder->text().isEmpty())
        {
            folder->setText(DoomsdayInfo::defaultServerRuntimeFolder().toString());
        }
        form->addRow(tr("Runtime folder:"), folder);

        QPushButton *folderButton = new QPushButton(tr("Select Folder"));
        connect(folderButton, SIGNAL(clicked()), &self, SLOT(pickFolder()));
        form->addRow(0, folderButton);
#ifdef WIN32
        folderButton->setMaximumWidth(100);
#endif

        iwadFolder = new QLineEdit;
        iwadFolder->setMinimumWidth(300);
        iwadFolder->setText(st.value("LocalServer/iwad").toString());
        form->addRow(tr("IWAD folder:"), iwadFolder);

        QPushButton *iwadFolderButton = new QPushButton(tr("Select Folder"));
        connect(iwadFolderButton, SIGNAL(clicked()), &self, SLOT(pickIwadFolder()));
        form->addRow(0, iwadFolderButton);
#ifdef WIN32
        iwadFolderButton->setMaximumWidth(100);
#endif

        options = new QLineEdit;
        options->setMinimumWidth(300);
        options->setText(st.value("LocalServer/options").toString());
        form->addRow(tr("Options:"), options);

        QDialogButtonBox *bbox = new QDialogButtonBox;
        mainLayout->addWidget(bbox);
        yes = bbox->addButton(tr("&Start Server"), QDialogButtonBox::YesRole);
        QPushButton* no = bbox->addButton(tr("&Cancel"), QDialogButtonBox::RejectRole);
        QPushButton *opt = bbox->addButton(tr("Game Options..."), QDialogButtonBox::ActionRole);
        QObject::connect(yes, SIGNAL(clicked()), &self, SLOT(accept()));
        QObject::connect(no, SIGNAL(clicked()), &self, SLOT(reject()));
        QObject::connect(opt, SIGNAL(clicked()), &self, SLOT(configureGameOptions()));
        yes->setDefault(true);
    }
};

LocalServerDialog::LocalServerDialog(QWidget *parent)
    : QDialog(parent), d(new Instance(*this))
{
    connect(d->port, SIGNAL(textChanged(QString)), this, SLOT(validate()));
    connect(this, SIGNAL(accepted()), this, SLOT(saveState()));

    validate();
}

LocalServerDialog::~LocalServerDialog()
{
    delete d;
}

quint16 LocalServerDialog::port() const
{
    return d->port->text().toInt();
}

QString LocalServerDialog::gameMode() const
{
    return d->games->itemData(d->games->currentIndex()).toString();
}

QStringList LocalServerDialog::additionalOptions() const
{
    QStringList opts = d->options->text().split(' ', QString::SkipEmptyParts);
    opts << "-iwad" << d->iwadFolder->text();
    return opts;
}

NativePath LocalServerDialog::runtimeFolder() const
{
    return d->folder->text();
}

void LocalServerDialog::pickFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select Runtime Folder"),
                                                    d->folder->text());
    if(!dir.isEmpty()) d->folder->setText(dir);

    validate();
}

void LocalServerDialog::pickIwadFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select IWAD Folder"),
                                                    d->iwadFolder->text());
    if(!dir.isEmpty()) d->iwadFolder->setText(dir);

    validate();
}

void LocalServerDialog::configureGameOptions()
{
}

void LocalServerDialog::saveState()
{
    QSettings st;
    st.setValue("LocalServer/gameMode", d->games->itemData(d->games->currentIndex()).toString());
    st.setValue("LocalServer/port", d->port->text().toInt());
    st.setValue("LocalServer/runtime", d->folder->text());
    st.setValue("LocalServer/iwad", d->iwadFolder->text());
    st.setValue("LocalServer/options", d->options->text());
}

void LocalServerDialog::validate()
{
    bool isValid = true;

    // Check port.
    QString txt = d->port->text().trimmed();
    int num = txt.toInt();
    if(txt.isEmpty() || num < 0 || num >= 0x10000)
    {
        isValid = false;
    }

    if(d->folder->text().isEmpty()) isValid = false;

    if(d->iwadFolder->text().isEmpty()) isValid = false;

    d->yes->setEnabled(isValid);
}
