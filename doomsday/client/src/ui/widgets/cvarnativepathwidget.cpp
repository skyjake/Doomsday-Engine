/** @file cvarnativepathwidget.cpp  Console variable with a native path.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/cvarnativepathwidget.h"
#include "ui/clientwindow.h"

#include <doomsday/console/var.h>
#include <de/PopupMenuWidget>
#include <de/SignalAction>
#include <QFileDialog>

using namespace de;

DENG2_PIMPL_NOREF(CVarNativePathWidget)
{
    char const *cvar;
    NativePath path;
    QStringList filters;
    PopupMenuWidget *menu;
    String blankText = "(not set)";

    cvar_t *var() const
    {
        cvar_t *cv = Con_FindVariable(cvar);
        DENG2_ASSERT(cv != 0);
        return cv;
    }

    String labelText() const
    {
        if(path.isEmpty())
        {
            return String(_E(l)) + blankText + _E(.);
        }
        return path.fileName();
    }
};

CVarNativePathWidget::CVarNativePathWidget(char const *cvarPath)
    : d(new Instance)
{
    add(d->menu = new PopupMenuWidget);
    d->menu->setAnchorAndOpeningDirection(rule(), ui::Up);
    d->menu->items()
            << new ui::ActionItem(tr("Browse..."),
                                  new SignalAction(this, SLOT(chooseUsingNativeFileDialog())))
            << new ui::ActionItem(style().images().image("close.ring"), tr("Clear"),
                                  new SignalAction(this, SLOT(clearPath())));

    d->cvar = cvarPath;
    updateFromCVar();

    auxiliary().setText(tr("Browse"));

    connect(&auxiliary(), SIGNAL(pressed()), this, SLOT(chooseUsingNativeFileDialog()));
    connect(this, SIGNAL(pressed()), this, SLOT(showActionsPopup()));
}

void CVarNativePathWidget::setFilters(StringList const &filters)
{
    d->filters.clear();
    for(auto const &f : filters)
    {
        d->filters << f;
    }
}

void CVarNativePathWidget::setBlankText(String const &text)
{
    d->blankText = text;
}

char const *CVarNativePathWidget::cvarPath() const
{
    return d->cvar;
}

void CVarNativePathWidget::updateFromCVar()
{
    d->path = CVar_String(d->var());
    setText(d->labelText());
}

void CVarNativePathWidget::chooseUsingNativeFileDialog()
{
    auto &win = ClientWindow::main();

#ifndef MACOSX
    // Switch temporarily to windowed mode. Not needed on OS X because the display mode
    // is never changed on that platform.
    win.saveState();
    int windowedMode[] = {
        ClientWindow::Fullscreen, false,
        ClientWindow::End
    };
    win.changeAttributes(windowedMode);
#endif

    // Use a native dialog to pick the path.
    QDir dir(d->path);
    if(d->path.isEmpty()) dir = QDir::home();
    QFileDialog dlg(&win, tr("Select File for \"%1\"").arg(d->cvar), dir.absolutePath());
    if(!d->filters.isEmpty())
    {
        dlg.setNameFilters(d->filters);
    }
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setOption(QFileDialog::ReadOnly, true);
    dlg.setLabelText(QFileDialog::Accept, tr("Select"));
    if(dlg.exec())
    {
        d->path = dlg.selectedFiles().at(0);
        setCVarValueFromWidget();
        setText(d->labelText());
    }

#ifndef MACOSX
    win.restoreState();
#endif
}

void CVarNativePathWidget::clearPath()
{
    d->path.clear();
    setCVarValueFromWidget();
    setText(d->labelText());
}

void CVarNativePathWidget::showActionsPopup()
{
    if(!d->menu->isOpen())
    {
        d->menu->open();
    }
    else
    {
        d->menu->close(0);
    }
}

void CVarNativePathWidget::setCVarValueFromWidget()
{
    CVar_SetString(d->var(), d->path.toString().toUtf8());
}
