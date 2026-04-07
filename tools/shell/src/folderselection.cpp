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

#include <de/buttonwidget.h>
#include <de/filedialog.h>
#include <de/lineeditwidget.h>

using namespace de;

DE_PIMPL(FolderSelection)
{
    String prompt;
    LineEditWidget *edit;
    ButtonWidget *button;

    Impl(Public *i)
        : Base(i)
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
    }

    DE_PIMPL_AUDIENCE(Selection)
};

DE_AUDIENCE_METHOD(FolderSelection, Selection)

FolderSelection::FolderSelection(const String &prompt)
    : GuiWidget("folderselection")
    , d(new Impl(this))
{
    d->prompt = prompt;

    rule().setInput(Rule::Height, d->edit->rule().height());

    d->button->setActionFn([this]() { selectFolder(); });
    d->edit->audienceForContentChange() += [this]() {
        if (path().exists())
        {
            DE_NOTIFY(Selection, i)
            {
                i->folderSelected(path());
            }
        }
    };
}

void FolderSelection::setPath(const NativePath &path)
{
    d->edit->setText(path);
}

void FolderSelection::setEnabled(bool yes)
{
    enable(yes);
}

NativePath FolderSelection::path() const
{
    return d->edit->text();
}

void FolderSelection::selectFolder()
{
    FileDialog dlg;
    dlg.setBehavior(FileDialog::AcceptDirectories, ReplaceFlags);
    dlg.setTitle("Select Folder");
    dlg.setPrompt("Select");
    dlg.setInitialLocation(path());
    if (dlg.exec(root()))
    {
        setPath(dlg.selectedPath());
        DE_NOTIFY(Selection, i)
        {
            i->folderSelected(dlg.selectedPath());
        }
    }
}
