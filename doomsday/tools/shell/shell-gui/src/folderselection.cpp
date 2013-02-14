/** @file folderselection.cpp  Widget for selecting a folder.
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

#include "folderselection.h"
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLabel>

DENG2_PIMPL(FolderSelection)
{
    QString prompt;
    QLineEdit *edit;
    QPushButton *button;

    Instance(Public &i, QString extraLabel) : Base(i),
        edit(0),
        button(0)
    {
        /*
        // What's up with the extra spacing?
        QPalette pal = self.palette();
        pal.setColor(self.backgroundRole(), Qt::red);
        self.setPalette(pal);
        self.setAutoFillBackground(true);
        */

        QHBoxLayout *layout = new QHBoxLayout;
        layout->setContentsMargins(0, 0, 0, 0);
        self.setLayout(layout);

        if(!extraLabel.isEmpty())
        {
            QLabel *lab = new QLabel(extraLabel);
            layout->addWidget(lab, 0);
        }

        edit = new QLineEdit;
        edit->setMinimumWidth(280);
#ifdef WIN32
        button = new QPushButton(tr("&Browse..."));
#else
        button = new QPushButton(tr("..."));
#endif

        layout->addWidget(edit, 1);
        layout->addWidget(button, 0);
    }
};

FolderSelection::FolderSelection(QString const &prompt, QWidget *parent)
    : QWidget(parent), d(new Instance(*this, ""))
{
    d->prompt = prompt;
    connect(d->button, SIGNAL(clicked()), this, SLOT(selectFolder()));
}

FolderSelection::FolderSelection(QString const &prompt, QString const &extraLabel, QWidget *parent)
    : QWidget(parent), d(new Instance(*this, extraLabel))
{
    d->prompt = prompt;
    connect(d->button, SIGNAL(clicked()), this, SLOT(selectFolder()));
}

FolderSelection::~FolderSelection()
{
    delete d;
}

void FolderSelection::setPath(de::NativePath const &path)
{
    d->edit->setText(path.toString());
}

de::NativePath FolderSelection::path() const
{
    return d->edit->text();
}

void FolderSelection::selectFolder()
{
    QString initial = d->edit->text();
    if(initial.isEmpty()) initial = QDir::homePath();
    QString dir = QFileDialog::getExistingDirectory(0, d->prompt, initial);
    if(!dir.isEmpty())
    {
        d->edit->setText(dir);
        emit selected();
    }
}
