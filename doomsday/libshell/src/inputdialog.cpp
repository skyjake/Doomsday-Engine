/** @file inputdialog.h  Dialog for querying text from the user.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/InputDialog"
#include "de/shell/LabelWidget"
#include "de/shell/LineEditWidget"
#include "de/shell/MenuWidget"
#include "de/shell/TextRootWidget"

namespace de {
namespace shell {

struct InputDialog::Instance
{
    LabelWidget *label;
    LineEditWidget *edit;
    MenuWidget *menu;
    String userText;
    int result;

    Instance() : label(0), edit(0), menu(0), result(0)
    {}

    ~Instance()
    {}
};

InputDialog::InputDialog(String const &name)
    : DialogWidget(name), d(new Instance)
{
    String prefix = '#' + String::number(id());
    RuleRectangle &rect = rule();

    // Label.
    d->label = new LabelWidget;
    d->label->setExpandsToFitLines(true); // determines height independently

    d->label->rule()
            .setInput(RuleRectangle::Width, rect.width())
            .setInput(RuleRectangle::Top,   rect.top())
            .setInput(RuleRectangle::Left,  rect.left());

    // Address editor.
    d->edit = new LineEditWidget(prefix + ".edit");
    d->edit->setFocusNext(prefix + ".menu");

    d->edit->rule()
            .setInput(RuleRectangle::Width, rect.width())
            .setInput(RuleRectangle::Left,  rect.left())
            .setInput(RuleRectangle::Top,   d->label->rule().bottom() + 1);

    // Menu for actions.
    d->menu = new MenuWidget(MenuWidget::AlwaysOpen, prefix + ".menu");
    d->menu->setFocusNext(prefix + ".edit");
    d->menu->setBorder(MenuWidget::NoBorder);
    d->menu->setBackgroundAttribs(TextCanvas::Char::DefaultAttributes);
    d->menu->setSelectionAttribs(TextCanvas::Char::Reverse);
    d->menu->appendItem(new Action(tr("OK"), this, SLOT(accept())));
    d->menu->appendItem(new Action(tr("Cancel"), KeyEvent(Qt::Key_C, KeyEvent::Control),
                                   this, SLOT(reject())), "Ctrl-C");

    d->menu->rule()
            .setInput(RuleRectangle::Width,  rect.width())
            .setInput(RuleRectangle::Left,   rect.left())
            .setInput(RuleRectangle::Bottom, rect.bottom());

    add(d->label);
    add(d->edit);
    add(d->menu);

    // Outer dimensions.
    rect.setInput(RuleRectangle::Width, Const(50));
    rect.setInput(RuleRectangle::Height,
                  d->menu->rule().height() +
                  d->edit->rule().height() +
                  d->label->rule().height() + 2);
}

InputDialog::~InputDialog()
{
    delete d;
}

void InputDialog::setWidth(int width)
{
    rule().setInput(RuleRectangle::Width, Const(width));
}

void InputDialog::setDescription(String const &desc)
{
    d->label->setLabel(desc);
}

void InputDialog::setPrompt(String const &prompt)
{
    d->edit->setPrompt(prompt);
}

void InputDialog::setText(String const &text)
{
    d->edit->setText(text);
}

void InputDialog::setAcceptLabel(String const &label)
{
    d->menu->itemAction(0).setLabel(label);
    redraw();
}

void InputDialog::setRejectLabel(String const &label)
{
    d->menu->itemAction(1).setLabel(label);
    redraw();
}

void InputDialog::prepare()
{
    DialogWidget::prepare();

    d->userText.clear();
    d->result = 0;

    root().setFocus(d->edit);
}

void InputDialog::finish(int result)
{
    d->result = result;
    d->userText.clear();
    if(result) d->userText = d->edit->text();

    DialogWidget::finish(result);
}

String InputDialog::text() const
{
    return d->userText;
}

int InputDialog::result() const
{
    return d->result;
}

} // namespace shell
} // namespace de
