/** @file filedialog_x11.cpp  Native file chooser dialog.
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

#include "de/FileDialog"
#include "de/DirectoryBrowserWidget"
#include "de/MessageDialog"

namespace de {

DE_PIMPL_NOREF(FileDialog)
{
    String           title    = "Select File";
    String           prompt   = "OK";
    Behaviors        behavior = AcceptFiles;
    List<NativePath> selection;
    NativePath       initialLocation;
    FileTypes        fileTypes; // empty list: eveything allowed

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
        auto *dlg = new MessageDialog;
        dlg->setDeleteAfterDismissed(true);
        dlg->title().setText(title);
        dlg->message().hide();
        dlg->buttons() << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, prompt)
                       << new DialogButtonItem(DialogWidget::Reject);

        dlg->area().enableScrolling(false);
        dlg->area().enableIndicatorDraw(false);
        dlg->area().enablePageKeys(false);

        browser = new DirectoryBrowserWidget(
            (behavior & AcceptFiles ? DirectoryBrowserWidget::ShowFiles : 0));
        browser->setCurrentPath(initialLocation);
        browser->rule().setInput(Rule::Height, browser->rule().width());
        dlg->area().add(browser);
        dlg->updateLayout();

        return dlg;
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

    // IFileOpenDialog *dlg = nullptr;
    // if (FAILED(CoCreateInstance(
    //         CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
    // {
    //     return false;
    // }

    // // Configure the dialog according to user-specified options.
    // DWORD options;
    // dlg->GetOptions(&options);
    // options |= FOS_FORCEFILESYSTEM;
    // if (d->behavior & MultipleSelection)
    // {
    //     options |= FOS_ALLOWMULTISELECT;
    // }
    // if (d->behavior & AcceptFiles)
    // {
    //     options |= FOS_FILEMUSTEXIST;
    // }
    // if (d->behavior & AcceptDirectories)
    // {
    //     options |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
    // }
    // dlg->SetOptions(options);
    // {
    //     const auto path = d->initialLocation.toString().toUtf16();
    //     IShellItem *folder = nullptr;
    //     if (SUCCEEDED(SHCreateItemFromParsingName(path.c_wstr(), nullptr, IID_PPV_ARGS(&folder))))
    //     {
    //         dlg->SetDefaultFolder(folder);
    //         folder->Release();
    //     }
    // }
    // dlg->SetTitle(d->title.toUtf16().c_wstr());
    // dlg->SetOkButtonLabel(d->prompt.toUtf16().c_wstr());
    // if (d->fileTypes)
    // {
    //     List<COMDLG_FILTERSPEC> filters;
    //     const auto strings = d->filters();
    //     for (const auto &str : strings)
    //     {
    //         COMDLG_FILTERSPEC spec;
    //         spec.pszName = str.first.c_wstr();
    //         spec.pszSpec = str.second.c_wstr();
    //         filters << spec;
    //     }
    //     dlg->SetFileTypes(UINT(filters.size()), filters.data());
    // }

    // if (SUCCEEDED(dlg->Show(nullptr)))
    // {
    //     IShellItemArray *results = nullptr;
    //     dlg->GetResults(&results);
    //     if (results)
    //     {
    //         DWORD resultCount = 0;
    //         results->GetCount(&resultCount);
    //         for (unsigned i = 0; i < resultCount; ++i)
    //         {
    //             IShellItem *result = nullptr;
    //             if (SUCCEEDED(results->GetItemAt(i, &result)))
    //             {
    //                 PWSTR itemPath;
    //                 if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &itemPath)))
    //                 {
    //                     d->selection << NativePath(
    //                         String::fromUtf16(Block(itemPath, 2 * (wcslen(itemPath) + 1))));
    //                     CoTaskMemFree(itemPath);
    //                 }
    //                 result->Release();
    //             }
    //         }
    //         results->Release();
    //     }
    // }

    // // Cleanup.
    // dlg->Release();

    auto *dlg = d->makeDialog();
    if (dlg->exec(root))
    {
        // Get the selected items.

    }

    return !d->selection.empty();
}

} // namespace de
