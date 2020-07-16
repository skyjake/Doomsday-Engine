/** @file term/inputdialog.cpp  Dialog for querying text from the user.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/term/inputdialog.h"
#include "de/term/labelwidget.h"
#include "de/term/lineeditwidget.h"
#include "de/term/menuwidget.h"
#include "de/term/textrootwidget.h"

namespace de { namespace term {

DE_PIMPL_NOREF(InputDialogWidget)
{
    LabelWidget *   label = nullptr;
    LineEditWidget *edit  = nullptr;
    MenuWidget *    menu  = nullptr;
    String              userText;
    int                 result = 0;
};

InputDialogWidget::InputDialogWidget(const String &name)
    : DialogWidget(name)
    , d(new Impl)
{
    RuleRectangle &rect = rule();

    // Label.
    d->label = new LabelWidget;
    d->label->setExpandsToFitLines(true); // determines height independently

    d->label->rule()
            .setInput(Rule::Width, rect.width())
            .setInput(Rule::Top,   rect.top())
            .setInput(Rule::Left,  rect.left());

    // Address editor.
    d->edit = new LineEditWidget;
    d->edit->setName(d->edit->uniqueName("edit"));

    d->edit->rule()
            .setInput(Rule::Width, rect.width())
            .setInput(Rule::Left,  rect.left())
            .setInput(Rule::Top,   d->label->rule().bottom() + 1);

    // Menu for actions.
    d->menu = new MenuWidget(MenuWidget::AlwaysOpen);
    d->menu->setName(d->menu->uniqueName("menu"));
    d->menu->setBorder(MenuWidget::NoBorder);
    d->menu->setBackgroundAttribs(TextCanvas::AttribChar::DefaultAttributes);
    d->menu->setSelectionAttribs(TextCanvas::AttribChar::Reverse);
    d->menu->appendItem(new Action("OK", [this]() { accept(); }));
    d->menu->appendItem(
        new Action("Cancel", KeyEvent("c", KeyEvent::Control), [this]() { reject(); }), "Ctrl-C");

    d->menu->rule()
            .setInput(Rule::Width,  rect.width())
            .setInput(Rule::Left,   rect.left())
            .setInput(Rule::Bottom, rect.bottom());

    add(d->label);
    add(d->edit);
    add(d->menu);

    setFocusCycle(WidgetList() << d->edit << d->menu);

    // Outer dimensions.
    rect.setInput(Rule::Width, Const(50));
    rect.setInput(Rule::Height,
                  d->menu->rule().height() +
                  d->edit->rule().height() +
                  d->label->rule().height() + 2);
}

LabelWidget &InputDialogWidget::label()
{
    return *d->label;
}

LineEditWidget &InputDialogWidget::lineEdit()
{
    return *d->edit;
}

MenuWidget &InputDialogWidget::menu()
{
    return *d->menu;
}

void InputDialogWidget::setWidth(int width)
{
    rule().setInput(Rule::Width, Const(width));
}

void InputDialogWidget::setDescription(const String &desc)
{
    d->label->setLabel(desc);
}

void InputDialogWidget::setPrompt(const String &prompt)
{
    d->edit->setPrompt(prompt);
}

void InputDialogWidget::setText(const String &text)
{
    d->edit->setText(text);
}

void InputDialogWidget::setAcceptLabel(const String &label)
{
    d->menu->itemAction(0).setLabel(label);
    redraw();
}

void InputDialogWidget::setRejectLabel(const String &label)
{
    d->menu->itemAction(1).setLabel(label);
    redraw();
}

void InputDialogWidget::prepare()
{
    DialogWidget::prepare();

    d->userText.clear();
    d->result = 0;

    root().setFocus(d->edit);
}

void InputDialogWidget::finish(int result)
{
    d->result = result;
    d->userText.clear();
    if (result) d->userText = d->edit->text();

    DialogWidget::finish(result);
}

String InputDialogWidget::text() const
{
    return d->userText;
}

int InputDialogWidget::result() const
{
    return d->result;
}

}} // namespace de::shell
