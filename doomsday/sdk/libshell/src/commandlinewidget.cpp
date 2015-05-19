/** @file commandlinewidget.cpp  Widget for command line input.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/CommandLineWidget"
#include "de/shell/TextRootWidget"
#include "de/shell/KeyEvent"
#include "de/shell/EditorHistory"

#include <de/String>
#include <QStringList>

namespace de {
namespace shell {

DENG2_PIMPL(CommandLineWidget)
{
    EditorHistory history;

    Instance(Public *i) : Base(i), history(i) {}
};

CommandLineWidget::CommandLineWidget(String const &name)
    : LineEditWidget(name), d(new Instance(this))
{
    setPrompt("> ");
}

bool CommandLineWidget::handleEvent(Event const &event)
{
    // There are only key press events.
    DENG2_ASSERT(event.type() == Event::KeyPress);
    KeyEvent const &ev = event.as<KeyEvent>();

    // Override the editor's normal Enter handling.
    if(ev.key() == Qt::Key_Enter)
    {
        String const entered = d->history.enter();
        emit commandEntered(entered);
        return true;
    }

    if(LineEditWidget::handleEvent(event))
    {
        return true;
    }

    // Final fallback: history navigation.
    return d->history.handleControlKey(ev.key());
}

void CommandLineWidget::autoCompletionBegan(String const &wordBase)
{
    LineEditWidget::autoCompletionBegan(wordBase);

    LOG_MSG("Completions for '%s':") << wordBase;
    LOG_MSG("  %s") << suggestedCompletions().join(", ");
}

} // namespace shell
} // namespace de
