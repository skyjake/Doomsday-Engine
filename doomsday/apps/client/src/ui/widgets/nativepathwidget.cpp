/** @file nativepathwidget.cpp  Widget for selecting a native path.
 *
 * @authors Copyright (c) 2014-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/nativepathwidget.h"
#include "ui/clientwindow.h"
#include "clientapp.h"

#include <de/popupmenuwidget.h>
#include <de/filedialog.h>

using namespace de;

DE_PIMPL(NativePathWidget)
{
    NativePath       path;
    PopupMenuWidget *menu;
    String           blankText = "(not set)";
    String           prompt    = "Select File";
    FileDialog::FileTypes filters;

    Impl(Public *i)
        : Base(i)
    {}

    String labelText() const
    {
        if (path.isEmpty())
        {
            return String(_E(l)) + blankText + _E(.);
        }
        return path.fileName();
    }

    void notifyChange()
    {
        DE_NOTIFY_PUBLIC(UserChange, i)
        {
            i->pathChangedByUser(self());
        }
    }

    DE_PIMPL_AUDIENCE(UserChange)
};

DE_AUDIENCE_METHOD(NativePathWidget, UserChange)

NativePathWidget::NativePathWidget()
    : d(new Impl(this))
{
    add(d->menu = new PopupMenuWidget);
    d->menu->setAnchorAndOpeningDirection(rule(), ui::Up);
    d->menu->items()
            << new ui::ActionItem("Browse...",
                                  [this]() { chooseUsingNativeFileDialog(); })
            << new ui::ActionItem(style().images().image("close.ring"), "Reset",
                                  [this]() { clearPath(); });

    auxiliary().setText("Browse");

    auxiliary().audienceForPress() += [this]() { chooseUsingNativeFileDialog(); };
    audienceForPress() += [this]() { showActionsPopup(); };
}

void NativePathWidget::setFilters(const FileDialog::FileTypes &filters)
{
    d->filters = filters;
}

void NativePathWidget::setBlankText(const String &text)
{
    d->blankText = text;
    setText(d->labelText());
}

void NativePathWidget::setPrompt(const String &promptText)
{
    d->prompt = promptText;
}

NativePath NativePathWidget::path() const
{
    return d->path;
}

void NativePathWidget::setPath(const NativePath &path)
{
    if (d->path != path)
    {
        d->path = path;
        setText(d->labelText());
        d->notifyChange();
    }
}

void NativePathWidget::chooseUsingNativeFileDialog()
{
    // Use a native dialog to pick the path.
    NativePath dir = d->path;
    if (d->path.isEmpty()) dir = NativePath::homePath();
    FileDialog dlg;
    dlg.setTitle(d->prompt);
    dlg.setInitialLocation(NativePath::workPath() / dir);
    dlg.setFileTypes(d->filters);
    dlg.setPrompt("Select");
    if (dlg.exec(root()))
    {
        d->path = dlg.selectedPath();
        setText(d->labelText());
        DE_NOTIFY(UserChange, i) i->pathChangedByUser(*this);
    }
}

void NativePathWidget::clearPath()
{
    setPath({});
}

void NativePathWidget::showActionsPopup()
{
    if (!d->menu->isOpen())
    {
        d->menu->open();
    }
    else
    {
        d->menu->close(0.0);
    }
}
