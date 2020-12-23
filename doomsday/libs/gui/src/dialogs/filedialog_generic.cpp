/** @file filedialog_generic.cpp  File chooser dialog using Doomsday widgets.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#if defined (DE_USE_GENERIC_FILEDIALOG)

#include "de/filedialog.h"
#include "de/directorybrowserwidget.h"
#include "de/messagedialog.h"

namespace de {

DE_PIMPL_NOREF(FileDialog)
, DE_OBSERVES(DirectoryBrowserWidget, Selection)
{
    String           title    = "Select File";
    String           prompt   = "OK";
    Behaviors        behavior = AcceptFiles;
    List<NativePath> selection;
    NativePath       initialLocation;
    FileTypes        fileTypes; // empty list: eveything allowed

    SafeWidgetPtr<MessageDialog> dlg;
    DirectoryBrowserWidget *browser;

    // using Filters = List<std::pair<Block, Block>>;

    // Filters filters() const
    // {
    //     Filters list;
    //     for (const FileType &fileType : fileTypes)
    //     {
    //         const String exts =
    //             (fileType.extensions ? "*." + String::join(fileType.extensions, ";*.") : DE_STR("*"));
    //         list << std::make_pair(fileType.label.toUtf16(), exts.toUtf16());
    //     }
    //     return list;
    // }

    MessageDialog *makeDialog()
    {
        dlg.reset(new MessageDialog);
        dlg->setDeleteAfterDismissed(true);
        dlg->title().setText(title);
        dlg->message().hide();
        dlg->buttons() << new DialogButtonItem(DialogWidget::Id1 | DialogWidget::Default |
                                                   DialogWidget::Accept,
                                               prompt)
                       << new DialogButtonItem(DialogWidget::Reject);

        dlg->area().enableScrolling(false);
        dlg->area().enableIndicatorDraw(false);
        dlg->area().enablePageKeys(false);

        browser = new DirectoryBrowserWidget(
            (behavior & AcceptFiles ? DirectoryBrowserWidget::ShowFiles : 0));
        if (behavior & AcceptDirectories)
        {
            browser->setEmptyContentText("No Subdirs");
        }
        browser->rule().setInput(Rule::Height, browser->rule().width());
        browser->audienceForSelection() += this;
        browser->audienceForNavigation() += [this]() {
            if (behavior.testFlag(AcceptFiles))
            {
                dlg->buttonWidget(DialogWidget::Id1)->disable();
            }
        };
        browser->setCurrentPath(initialLocation);
        dlg->area().add(browser);
        dlg->updateLayout();

        return dlg;
    }

    void itemSelected(DirectoryBrowserWidget &, const DirectoryItem &item) override
    {
        if (behavior.testFlag(AcceptFiles))
        {
            if (!item.isSelected())
            {
                browser->setSelected(item);
            }
            dlg->buttonWidget(DialogWidget::Id1)->enable(!browser->selected().isEmpty());
        }
    }
};

FileDialog::FileDialog() : d(new Impl)
{}

void FileDialog::setTitle(const String &title)
{
    d->title = title;
}

void FileDialog::setPrompt(const String &prompt)
{
    d->prompt = prompt;
}

void FileDialog::setBehavior(Behaviors behaviors, FlagOp flagOp)
{
    applyFlagOperation(d->behavior, behaviors, flagOp);
}

void FileDialog::setInitialLocation(const NativePath &initialLocation)
{
    if (initialLocation.exists())
    {
        d->initialLocation = initialLocation;
        if (!initialLocation.isDirectory())
        {
            d->initialLocation = d->initialLocation.fileNamePath();
        }
    }
    else
    {
        d->initialLocation = NativePath::homePath();
    }
}

void FileDialog::setFileTypes(const FileTypes &fileTypes)
{
    d->fileTypes = fileTypes;
}

NativePath FileDialog::selectedPath() const
{
    return d->selection ? d->selection.front() : NativePath();
}

List<NativePath> FileDialog::selectedPaths() const
{
    return d->selection;
}

bool FileDialog::exec(GuiRootWidget &root)
{
    d->selection.clear();

    auto *dlg = d->makeDialog();
    if (dlg->exec(root))
    {
        // Get the selected items.
        if (d->behavior & AcceptDirectories)
        {
            d->selection << d->browser->currentDirectory();
        }
        else
        {
            for (auto path : d->browser->selectedPaths())
            {
                d->selection << path;
            }
        }
    }

    return !d->selection.empty();
}

} // namespace de

#endif // USE_GENERIC_FILEDIALOG
