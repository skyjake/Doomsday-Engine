/** @file filedialog.h  Native file chooser dialog.
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

#ifndef LIBGUI_FILEDIALOG_H
#define LIBGUI_FILEDIALOG_H

#include "libgui.h"
#include <de/NativePath>
#include <de/List>

namespace de {

/**
 * Native file chooser dialog.
 */
class LIBGUI_PUBLIC FileDialog
{
public:
    enum Behavior {
        AcceptFiles       = 0x1,
        AcceptDirectories = 0x2,
        MultipleSelection = 0x4,
    };
    using Behaviors = Flags;

public:
    FileDialog();

    void setTitle(const String &title);
    void setPrompt(const String &prompt);
    void setBehavior(Behaviors behaviors, FlagOp flagOp = SetFlags);
    void setInitialLocation(const NativePath &initialLocation);
    void setFileTypes(const StringList &fileExtensions);

    bool exec();

    NativePath       selectedPath() const;
    List<NativePath> selectedPaths() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_FILEDIALOG_H
