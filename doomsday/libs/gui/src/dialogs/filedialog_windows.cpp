/** @file filedialog_windows.cpp  Native file chooser dialog.
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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace de {

DE_PIMPL_NOREF(FileDialog)
{
    String     title    = "Select File";
    String     prompt   = "OK";
    Behaviors  behavior = AcceptFiles;
    List<NativePath> selection;
    NativePath initialLocation;
    StringList fileTypes; // empty list: eveything allowed
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
    d->initialLocation = initialLocation;
}

void FileDialog::setFileTypes(const StringList &fileExtensions)
{
    d->fileTypes = fileExtensions;
}

NativePath FileDialog::selectedPath() const
{
    return d->selection ? d->selection.front() : NativePath();
}

List<NativePath> FileDialog::selectedPaths() const
{
    return d->selection;
}

bool FileDialog::exec()
{
    d->selection.clear();

    #if 0
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
                [types addObject:(NSString * _Nonnull)[NSString stringWithUTF8String:type.c_str()]];
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
    #endif
    
    return false;
}

} // namespace de

