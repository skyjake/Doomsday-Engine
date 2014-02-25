/** @file messagedialog.cpp  Dialog for showing a message.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/MessageDialog"
#include "de/SequentialLayout"

namespace de {

using namespace ui;

DENG_GUI_PIMPL(MessageDialog)
{
    LabelWidget *title;
    LabelWidget *message;

    Instance(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        // Create widgets.
        area.add(title   = new LabelWidget);
        area.add(message = new LabelWidget);

        // Configure style.
        title->setFont("title");
        title->setTextColor("accent");
        title->setSizePolicy(ui::Fixed, ui::Expand);
        title->setAlignment(ui::AlignLeft);
        title->setTextLineAlignment(ui::AlignLeft);
        message->setSizePolicy(ui::Fixed, ui::Expand);
        message->setAlignment(ui::AlignLeft);
        message->setTextLineAlignment(ui::AlignLeft);

        updateLayout();
    }

    void updateLayout()
    {
        ScrollAreaWidget &area = self.area();

        // Simple vertical layout.
        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top());
        layout.setOverrideWidth(style().rules().rule("dialog.message.width"));

        // Put all the widgets into the layout.
        foreach(Widget *w, area.childWidgets())
        {
            layout << w->as<GuiWidget>();
        }

        area.setContentSize(layout.width(), layout.height());
    }
};

MessageDialog::MessageDialog(String const &name)
    : DialogWidget(name)
    , d(new Instance(this))
{}

LabelWidget &MessageDialog::title()
{
    return *d->title;
}

LabelWidget &MessageDialog::message()
{
    return *d->message;
}

void MessageDialog::updateLayout()
{
    d->updateLayout();
}

} // namespace de
