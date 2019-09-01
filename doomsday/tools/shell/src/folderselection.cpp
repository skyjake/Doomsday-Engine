/** @file folderselection.cpp  Widget for selecting a folder.
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

#include "folderselection.h"

#include <de/ButtonWidget>
#include <de/FileDialog>
#include <de/LineEditWidget>

using namespace de;

DE_PIMPL(FolderSelection)
{
    String prompt;
    LineEditWidget *edit;
    ButtonWidget *button;

    Impl(Public *i)
        : Base(i)
//        , edit(0)
//        , button(0)
    {
        edit = &i->addNew<LineEditWidget>();

        button = &i->addNew<ButtonWidget>();
        button->setSizePolicy(ui::Expand, ui::Expand);
        button->setText("Browse...");

        edit->rule()
            .setInput(Rule::Width, i->rule().width() - button->rule().width())
            .setInput(Rule::Top, i->rule().top())
            .setInput(Rule::Left, i->rule().left());

        button->rule()
            .setInput(Rule::Top, i->rule().top())
            .setInput(Rule::Right, i->rule().right());

        //        if (!extraLabel.isEmpty())
        //        {
        //            QLabel *lab = new QLabel(extraLabel);
        //            layout->addWidget(lab, 0);
        //        }

        //        edit = new QLineEdit;
        //        edit->setMinimumWidth(280);
        //#ifdef WIN32
        //        button = new QPushButton(tr("&Browse..."));
        //#else
        //        button = new QPushButton(tr("..."));
        //#endif
        //        button->setAutoDefault(false);

        //        layout->addWidget(edit, 1);
        //        layout->addWidget(button, 0);
    }
};

//FolderSelection::FolderSelection(const String &prompt)
//    : d(new Impl(this, ""))
//{
//    d->prompt = prompt;
//    connect(d->button, SIGNAL(clicked()), this, SLOT(selectFolder()));
//    connect(d->edit, SIGNAL(textEdited(QString)), this, SIGNAL(selected()));
//}

FolderSelection::FolderSelection(const String &prompt)
    : GuiWidget("folderselection")
    , d(new Impl(this))
{
    d->prompt = prompt;

    rule().setInput(Rule::Height, d->edit->rule().height());

//    connect(d->button, SIGNAL(clicked()), this, SLOT(selectFolder()));
}

void FolderSelection::setPath(const NativePath &path)
{
//    d->edit->setText(QString::fromUtf8(path));
}

void FolderSelection::setEnabled(bool yes)
{
//    d->edit->setEnabled(yes);
//    d->button->setEnabled(yes);

//    if (yes)
//    {
//        d->edit->setStyleSheet("");
//    }
//    else
//    {
//        d->edit->setStyleSheet("background-color:#eee; color:#888;");
//    }
}

de::NativePath FolderSelection::path() const
{
//    return convert(d->edit->text());
    return {};
}

void FolderSelection::selectFolder()
{
//    QString initial = d->edit->text();
//    if (initial.isEmpty()) initial = QDir::homePath();
//    QString dir = QFileDialog::getExistingDirectory(0, d->prompt, initial);
//    if (!dir.isEmpty())
//    {
//        d->edit->setText(dir);
//        emit selected();
//    }
}
