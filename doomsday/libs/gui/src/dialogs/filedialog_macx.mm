/** @file filedialog_macx.mm  Native file chooser dialog.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/filedialog.h"
#if 0
// Testing: use the generic dialog instead.
#  include "filedialog_x11.cpp"
#else

#import <Cocoa/Cocoa.h>

namespace de {

DE_PIMPL_NOREF(FileDialog)
{
    String     title    = "Select File";
    String     prompt   = "OK";
    Behaviors  behavior = AcceptFiles;
    List<NativePath> selection;
    NativePath initialLocation;
    FileTypes  fileTypes; // empty list: eveything allowed
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

bool FileDialog::exec(GuiRootWidget &)
{
    d->selection.clear();

    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    [openDlg setCanChooseFiles:(d->behavior.testFlag(AcceptFiles) ? YES : NO)];
    [openDlg setCanChooseDirectories:(d->behavior.testFlag(AcceptDirectories) ? YES : NO)];
    [openDlg setAllowsMultipleSelection:(d->behavior.testFlag(MultipleSelection) ? YES : NO)];
    [openDlg setDirectoryURL:
        [NSURL fileURLWithPath:(NSString * _Nonnull)
                               [NSString stringWithUTF8String:d->initialLocation.c_str()]]];
    [openDlg setMessage:[NSString stringWithUTF8String:d->title.c_str()]];
    [openDlg setPrompt:[NSString stringWithUTF8String:d->prompt.c_str()]];

    // The allowed file types.
    {
        NSMutableArray<NSString *> *types = nil;
        if (d->fileTypes)
        {
            types = [NSMutableArray<NSString *> array];
            for (const auto &type : d->fileTypes)
            {
                for (const auto &ext : type.extensions)
                {
                    [types addObject:(NSString * _Nonnull)[NSString stringWithUTF8String:ext.c_str()]];
                }
            }
        }
        [openDlg setAllowedFileTypes:types];
    }

    if ([openDlg runModal] == NSModalResponseOK)
    {
        // Check the selected paths.
        for (NSURL *url in [openDlg URLs])
        {
            d->selection << [url.path UTF8String];
        }
        return true;
    }
    return false;
}

} // namespace de
#endif
