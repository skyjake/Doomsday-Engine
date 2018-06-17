/** @file libshell/src/inputdialog.cpp  Dialog for querying text from the user.
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

#include "de/shell/InputDialogTextWidget"
#include "de/shell/LabelTextWidget"
#include "de/shell/LineEditTextWidget"
#include "de/shell/MenuTextWidget"
#include "de/shell/TextRootWidget"

namespace de { namespace shell {

DE_PIMPL_NOREF(InputDialogTextWidget)
{
    LabelTextWidget *   label = nullptr;
    LineEditTextWidget *edit  = nullptr;
    MenuTextWidget *    menu  = nullptr;
    String              userText;
    int                 result = 0;
};

InputDialogTextWidget::InputDialogTextWidget(String const &name)
    : DialogTextWidget(name)
    , d(new Impl)
{
    RuleRectangle &rect = rule();

    // Label.
    d->label = new LabelTextWidget;
    d->label->setExpandsToFitLines(true); // determines height independently

    d->label->rule()
            .setInput(Rule::Width, rect.width())
            .setInput(Rule::Top,   rect.top())
            .setInput(Rule::Left,  rect.left());

    // Address editor.
    d->edit = new LineEditTextWidget;
    d->edit->setName(d->edit->uniqueName("edit"));

    d->edit->rule()
            .setInput(Rule::Width, rect.width())
            .setInput(Rule::Left,  rect.left())
            .setInput(Rule::Top,   d->label->rule().bottom() + 1);

    // Menu for actions.
    d->menu = new MenuTextWidget(MenuTextWidget::AlwaysOpen);
    d->menu->setName(d->menu->uniqueName("menu"));
    d->menu->setBorder(MenuTextWidget::NoBorder);
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

LabelTextWidget &InputDialogTextWidget::label()
{
    return *d->label;
}

LineEditTextWidget &InputDialogTextWidget::lineEdit()
{
    return *d->edit;
}

MenuTextWidget &InputDialogTextWidget::menu()
{
    return *d->menu;
}

void InputDialogTextWidget::setWidth(int width)
{
    rule().setInput(Rule::Width, Const(width));
}

void InputDialogTextWidget::setDescription(String const &desc)
{
    d->label->setLabel(desc);
}

void InputDialogTextWidget::setPrompt(String const &prompt)
{
    d->edit->setPrompt(prompt);
}

void InputDialogTextWidget::setText(String const &text)
{
    d->edit->setText(text);
}

void InputDialogTextWidget::setAcceptLabel(String const &label)
{
    d->menu->itemAction(0).setLabel(label);
    redraw();
}

void InputDialogTextWidget::setRejectLabel(String const &label)
{
    d->menu->itemAction(1).setLabel(label);
    redraw();
}

void InputDialogTextWidget::prepare()
{
    DialogTextWidget::prepare();

    d->userText.clear();
    d->result = 0;

    root().setFocus(d->edit);
}

void InputDialogTextWidget::finish(int result)
{
    d->result = result;
    d->userText.clear();
    if (result) d->userText = d->edit->text();

    DialogTextWidget::finish(result);
}

String InputDialogTextWidget::text() const
{
    return d->userText;
}

int InputDialogTextWidget::result() const
{
    return d->result;
}

}} // namespace de::shell
