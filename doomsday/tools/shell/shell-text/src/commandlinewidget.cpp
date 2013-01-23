/** @file commandlinewidget.cpp  Widget for command line input.
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

#include "commandlinewidget.h"
#include <de/String>

using namespace de;

struct CommandLineWidget::Instance
{
    ConstantRule *height;
    String command;
    int cursorPos;

    Instance() : cursorPos(0)
    {
        // Initial height of the command line (1 row).
        height = new ConstantRule(1);
    }

    ~Instance()
    {
        releaseRef(height);
    }
};

CommandLineWidget::CommandLineWidget(de::String const &name) : TextWidget(name), d(new Instance)
{
    rule().setInput(RectangleRule::Height, d->height);
}

CommandLineWidget::~CommandLineWidget()
{
    delete d;
}

Vector2i CommandLineWidget::cursorPosition()
{
    de::Rectanglei pos = rule().recti();
    return pos.topLeft + Vector2i(2 + d->cursorPos);
}

void CommandLineWidget::draw()
{
    TextCanvas *cv = targetCanvas();
    if(!cv) return;

    de::Rectanglei pos = rule().recti();

    TextCanvas::Char::Attribs attr = TextCanvas::Char::Reverse;
    TextCanvas::Char bg(' ', attr);

    cv->fill(pos, bg);
    cv->put(pos.topLeft, TextCanvas::Char('>', attr | TextCanvas::Char::Bold));
    cv->drawText(pos.topLeft + Vector2i(2, 0), d->command, attr);
}
