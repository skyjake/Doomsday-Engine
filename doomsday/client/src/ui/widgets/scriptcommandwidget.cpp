/** @file scriptcommandwidget.cpp  Interactive Doomsday Script command line.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/Script>
#include <de/Process>
#include <de/ScriptLex>

#include "ui/widgets/scriptcommandwidget.h"
#include "ui/widgets/popupwidget.h"

using namespace de;

DENG2_PIMPL(ScriptCommandWidget)
{
    Script script;
    Process process;

    Instance(Public *i) : Base(i)
    {}

    bool shouldShowAsPopup(Error const &er)
    {
        /*if(dynamic_cast<ScriptLex::MismatchedBracketError const *>(&er))
        {
            // Brackets may be left open to insert newlines.
            return false;
        }*/
        return true;
    }
};

ScriptCommandWidget::ScriptCommandWidget(String const &name)
    : CommandWidget(name), d(new Instance(this))
{}

bool ScriptCommandWidget::handleEvent(Event const &event)
{
    if(isDisabled()) return false;

    bool wasCompl = autocompletionPopup().isOpen();
    bool eaten = CommandWidget::handleEvent(event);
    if(eaten && wasCompl && event.isKeyDown())
    {
        closeAutocompletionPopup();
    }
    return eaten;
}

bool ScriptCommandWidget::isAcceptedAsCommand(String const &text)
{
    // Try to parse the command.
    try
    {
        d->script.parse(text);
        return true; // Looks good!
    }
    catch(Error const &er)
    {
        if(d->shouldShowAsPopup(er))
        {
            showAutocompletionPopup(er.asText());
        }
        return false;
    }
}

void ScriptCommandWidget::executeCommand(String const &text)
{
    LOG_INFO(_E(1) "$ " _E(>) _E(m)) << text;

    try
    {
        d->process.run(d->script);
        d->process.execute();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Error in script:\n") << er.asText();
    }

    /*
    // Print the result.
    Value const &result = d->process.context().evaluator().result();
    if(!result.is<NoneValue>())
    {
        LOG_INFO("%s") << result.asText();
    }
    */
}
