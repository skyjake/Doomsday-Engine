/**
 * @file downloaddialog.h
 * Dialog for downloads. @ingroup ui
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
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

#include "ui/dialogs/downloaddialog.h"

#include <de/callbackaction.h>

using namespace de;

DE_GUI_PIMPL(DownloadDialog)
{
    ProgressWidget *progress;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self().area();

        progress = new ProgressWidget;
        area.add(progress);
        progress->setAlignment(ui::AlignLeft);
        progress->setSizePolicy(ui::Fixed, ui::Expand);
        progress->setRange(Rangei(0, 100));
        progress->rule()
                .setLeftTop(area.contentRule().left(), area.contentRule().top())
                .setInput(Rule::Width, rule("dialog.download.width"));

        area.setContentSize(progress->rule());

        self().buttons() << new DialogButtonItem(DialogWidget::Reject,
                                                 "Cancel Download",
                                                 [this](){ self().cancel(); });
        updateStyle();
    }

    void updateStyle()
    {
        progress->setImageScale(.4f); //pointsToPixels(.4f));
    }
};

DownloadDialog::DownloadDialog(const String &name)
    : DialogWidget(name)
    , d(new Impl(this))
{}

DownloadDialog::~DownloadDialog()
{}

ProgressWidget &DownloadDialog::progressIndicator()
{
    return *d->progress;
}

void DownloadDialog::updateStyle()
{
    DialogWidget::updateStyle();
    d->updateStyle();
}
