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

#include "de/shell/InputDialogTedget"
#include "de/shell/LabelTedget"
#include "de/shell/LineEditTedget"
#include "de/shell/MenuTedget"
#include "de/shell/TextRootWidget"

namespace de { namespace shell {

DE_PIMPL_NOREF(InputDialogTedget)
{
    LabelTedget *   label = nullptr;
    LineEditTedget *edit  = nullptr;
    MenuTedget *    menu  = nullptr;
    String              userText;
    int                 result = 0;
};

InputDialogTedget::InputDialogTedget(String const &name)
    : DialogTedget(name)
    , d(new Impl)
{
    RuleRectangle &rect = rule();

    // Label.
    d->label = new LabelTedget;
    d->label->setExpandsToFitLines(true); // determines height independently

    d->label->rule()
            .setInput(Rule::Width, rect.width())
            .setInput(Rule::Top,   rect.top())
            .setInput(Rule::Left,  rect.left());

    // Address editor.
    d->edit = new LineEditTedget;
    d->edit->setName(d->edit->uniqueName("edit"));

    d->edit->rule()
            .setInput(Rule::Width, rect.width())
            .setInput(Rule::Left,  rect.left())
            .setInput(Rule::Top,   d->label->rule().bottom() + 1);

    // Menu for actions.
    d->menu = new MenuTedget(MenuTedget::AlwaysOpen);
    d->menu->setName(d->menu->uniqueName("menu"));
    d->menu->setBorder(MenuTedget::NoBorder);
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

LabelTedget &InputDialogTedget::label()
{
    return *d->label;
}

LineEditTedget &InputDialogTedget::lineEdit()
{
    return *d->edit;
}

MenuTedget &InputDialogTedget::menu()
{
    return *d->menu;
}

void InputDialogTedget::setWidth(int width)
{
    rule().setInput(Rule::Width, Const(width));
}

void InputDialogTedget::setDescription(String const &desc)
{
    d->label->setLabel(desc);
}

void InputDialogTedget::setPrompt(String const &prompt)
{
    d->edit->setPrompt(prompt);
}

void InputDialogTedget::setText(String const &text)
{
    d->edit->setText(text);
}

void InputDialogTedget::setAcceptLabel(String const &label)
{
    d->menu->itemAction(0).setLabel(label);
    redraw();
}

void InputDialogTedget::setRejectLabel(String const &label)
{
    d->menu->itemAction(1).setLabel(label);
    redraw();
}

void InputDialogTedget::prepare()
{
    DialogTedget::prepare();

    d->userText.clear();
    d->result = 0;

    root().setFocus(d->edit);
}

void InputDialogTedget::finish(int result)
{
    d->result = result;
    d->userText.clear();
    if (result) d->userText = d->edit->text();

    DialogTedget::finish(result);
}

String InputDialogTedget::text() const
{
    return d->userText;
}

int InputDialogTedget::result() const
{
    return d->result;
}

}} // namespace de::shell
