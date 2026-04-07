/** @file messagedialog.cpp  Dialog for showing a message.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/messagedialog.h"
#include "de/sequentiallayout.h"
#include "de/dialogcontentstylist.h"

namespace de {

using namespace ui;

DE_GUI_PIMPL(MessageDialog)
{
    LabelWidget *        title;
    LabelWidget *        message;
    DialogContentStylist buttonStylist;
    const Rule *         layoutWidth = nullptr;

    Impl(Public *i) : Base(i)
    {
        ScrollAreaWidget &area = self().area();

        // Create widgets.
        area.add(title   = new LabelWidget("title"));
        area.add(message = new LabelWidget("message"));

        // Configure style.
        title->setFont("title");
        title->setTextColor("accent");
        title->setSizePolicy(ui::Fixed, ui::Expand);
        title->setAlignment(ui::AlignLeft);
        title->setTextAlignment(ui::AlignRight);
        title->setOverrideImageSize(title->font().ascent());
        title->setImageColor(style().colors().colorf("accent"));
        title->setTextGap("gap");
        title->setTextLineAlignment(ui::AlignLeft);
        message->setSizePolicy(ui::Fixed, ui::Expand);
        message->setAlignment(ui::AlignLeft);
        message->setTextLineAlignment(ui::AlignLeft);

        layoutWidth = holdRef(rule("dialog.message.width"));

        updateLayout();
    }

    ~Impl() override
    {
        releaseRef(layoutWidth);
    }

    void updateLayout(LayoutBehavior behavior = ExcludeHidden)
    {
        ScrollAreaWidget &area = self().area();

        // Simple vertical layout.
        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top());
        layout.setOverrideWidth(*layoutWidth);

        // Put all the widgets into the layout.
        for (GuiWidget *w : area.childWidgets())
        {
            // Individual children can be hidden to exclude them from the layout.
            if (behavior == IncludeHidden || !w->behavior().testFlag(Widget::Hidden))
            {
                layout << *w;
            }
        }

        area.setContentSize(layout);
    }
};

MessageDialog::MessageDialog(const String &name)
    : DialogWidget(name)
    , d(new Impl(this))
{}

void MessageDialog::useInfoStyle()
{
    DialogWidget::useInfoStyle();

    title().setTextColor("inverted.accent");
    message().setTextColor("inverted.text");

    d->buttonStylist.addContainer(buttonsMenu());
    d->buttonStylist.addContainer(extraButtonsMenu());
    d->buttonStylist.setAdjustMargins(false);
    d->buttonStylist.setInfoStyle(true);
}

LabelWidget &MessageDialog::title()
{
    return *d->title;
}

LabelWidget &MessageDialog::message()
{
    return *d->message;
}

void MessageDialog::setLayoutWidth(const Rule &layoutWidth)
{
    changeRef(d->layoutWidth, layoutWidth);
}

void MessageDialog::updateLayout(LayoutBehavior behavior)
{
    d->updateLayout(behavior);
}

} // namespace de
