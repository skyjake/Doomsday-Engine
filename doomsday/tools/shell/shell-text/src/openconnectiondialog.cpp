/** @file openconnectiondialog.cpp  Dialog for specifying address for opening a connection.
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

#include "openconnectiondialog.h"
#include <de/widgets/rules.h>
#include <de/shell/TextRootWidget>
#include <de/shell/LabelWidget>
#include <de/shell/LineEditWidget>
#include <de/shell/MenuWidget>

using namespace de;
using namespace de::shell;

struct OpenConnectionDialog::Instance
{
    LabelWidget *label;
    LineEditWidget *address;
    MenuWidget *menu;
    String result;

    Instance() : label(0), address(0), menu(0)
    {}

    ~Instance()
    {}
};

OpenConnectionDialog::OpenConnectionDialog(String const &name)
    : DialogWidget(name), d(new Instance)
{
    RuleRectangle &rect = rule();

    // Label.
    d->label = new LabelWidget;
    d->label->setExpandsToFitLines(true); // determines height independently
    d->label->setLabel(tr("Enter the address of the server you want to connect to. "
                       "The address can be a domain name or an IP address. "
                       "Optionally, you may include a TCP port number, for example "
                       "\"10.0.1.1:13209\"."));

    d->label->rule()
            .setInput(RuleRectangle::Width, rect.width())
            .setInput(RuleRectangle::Top,   rect.top())
            .setInput(RuleRectangle::Left,  rect.left());

    // Address editor.
    d->address = new LineEditWidget("OpenConnectionDialog.address");
    d->address->setFocusNext("OpenConnectionDialog.menu");
    d->address->setPrompt(tr("Address: "));

    d->address->rule()
            .setInput(RuleRectangle::Width, rect.width())
            .setInput(RuleRectangle::Left,  rect.left())
            .setInput(RuleRectangle::Top,   d->label->rule().bottom() + 1);

    // Menu for actions.
    d->menu = new MenuWidget(MenuWidget::AlwaysOpen, "OpenConnectionDialog.menu");
    d->menu->setFocusNext("OpenConnectionDialog.address");
    d->menu->setBorder(MenuWidget::NoBorder);
    d->menu->setBackgroundAttribs(TextCanvas::Char::DefaultAttributes);
    d->menu->setSelectionAttribs(TextCanvas::Char::Reverse);
    d->menu->appendItem(new Action(tr("Connect to server"), this, SLOT(accept())));
    d->menu->appendItem(new Action(tr("Cancel"), KeyEvent(Qt::Key_C, KeyEvent::Control),
                                   this, SLOT(reject())), "Ctrl-C");

    d->menu->rule()
            .setInput(RuleRectangle::Width,  rect.width())
            .setInput(RuleRectangle::Left,   rect.left())
            .setInput(RuleRectangle::Bottom, rect.bottom());

    add(d->label);
    add(d->address);
    add(d->menu);

    // Outer dimensions.
    rect.setInput(RuleRectangle::Width, Const(50));
    rect.setInput(RuleRectangle::Height,
                  d->menu->rule().height() +
                  d->address->rule().height() +
                  d->label->rule().height() + 2);

}

OpenConnectionDialog::~OpenConnectionDialog()
{
    delete d;
}

void OpenConnectionDialog::prepare()
{
    DialogWidget::prepare();

    d->result.clear();
    root().setFocus(d->address);
}

void OpenConnectionDialog::finish(int result)
{
    d->result.clear();
    if(result) d->result = d->address->text();

    DialogWidget::finish(result);
}

String OpenConnectionDialog::address()
{
    return d->result;
}
