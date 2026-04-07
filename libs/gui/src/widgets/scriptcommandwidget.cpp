/** @file scriptcommandwidget.cpp  Interactive Doomsday Script command line.
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

#include "de/scriptcommandwidget.h"
#include "de/popupwidget.h"

#include <de/charsymbols.h>
#include <de/lexicon.h>
#include <de/app.h>
#include <de/dscript.h>
#include <de/scripting/scriptlex.h>
#include <de/scripting/builtinexpression.h>
#include <de/refvalue.h>

namespace de {

DE_PIMPL(ScriptCommandWidget)
, DE_OBSERVES(App, StartupComplete)
{
    Script script;
    Process process;

    Impl(Public *i) : Base(i)
    {
        App::app().audienceForStartupComplete() += this;
    }

    void appStartupCompleted()
    {
        self().updateCompletion();
    }

    void importNativeModules()
    {
        // Automatically import all native modules into the interactive process.
        for (const String &name : App::scriptSystem().nativeModules())
        {
            process.globals().add(new Variable(name,
                    new RecordValue(App::scriptSystem().nativeModule(name))));
        }
    }

    void updateLexicon()
    {
        Lexicon lexi;
        lexi.setCaseSensitive(true);
        lexi.setAdditionalWordChars("_");

        // Add the variables in the global scope.
        /// @todo Should be determined dynamically based on the scope at the cursor position.
        DE_FOR_EACH_CONST(Record::Members, i, process.globals().members())
        {
            lexi.addTerm(i->first);
        }

        // Add all built-in Doomsday Script functions.
        for (const String &name : BuiltInExpression::identifiers())
        {
            lexi.addTerm(name);
        }

        // Add all Doomsday Script keywords.
        for (const String &keyword : ScriptLex::keywords())
        {
            lexi.addTerm(keyword);
        }

        self().setLexicon(lexi);
    }

    bool shouldShowAsPopup(const Error &)
    {
        /*if (dynamic_cast<const ScriptLex::MismatchedBracketError *>(&er))
        {
            // Brackets may be left open to insert newlines.
            return false;
        }*/
        return true;
    }
};

ScriptCommandWidget::ScriptCommandWidget(const String &name)
    : CommandWidget(name), d(new Impl(this))
{}

bool ScriptCommandWidget::handleEvent(const Event &event)
{
    if (isDisabled()) return false;

    bool wasCompl = autocompletionPopup().isOpen();
    bool eaten = CommandWidget::handleEvent(event);
    if (eaten && wasCompl && event.isKeyDown())
    {
        closeAutocompletionPopup();
    }
    return eaten;
}

void ScriptCommandWidget::updateCompletion()
{
    d->importNativeModules();
    d->updateLexicon();
}

bool ScriptCommandWidget::isAcceptedAsCommand(const String &text)
{
    // Try to parse the command.
    try
    {
        d->script.parse(text);
        return true; // Looks good!
    }
    catch (const Error &er)
    {
        if (d->shouldShowAsPopup(er))
        {
            showAutocompletionPopup(er.asText());
        }
        return false;
    }
}

void ScriptCommandWidget::executeCommand(const String &text)
{
    LOG_SCR_NOTE(_E(1) "$ " _E(>) _E(m)) << text;

    try
    {
        d->process.run(d->script);
        d->process.execute();
    }
    catch (const Error &er)
    {
        LOG_SCR_WARNING("Error in script:\n") << er.asText();
    }

    // Print the result (if possible).
    try
    {
        const Value &result = d->process.context().evaluator().result();
        if (!is<NoneValue>(result))
        {
            String msg = DE_CHAR_RIGHT_DOUBLEARROW " " _E(>)_E(m) + result.asText();
            LOG_SCR_MSG(msg);
        }
    }
    catch (const Error &)
    {}
}

void ScriptCommandWidget::autoCompletionBegan(const String &prefix)
{
    // Prepare a list of annotated completions to show in the popup.
    const auto compls = suggestedCompletions();
    if (compls)
    {
        showAutocompletionPopup(
            Stringf("Completions for %s:\n",
                           (_E(b) + prefix + _E(.)_E(m) + String::join(compls, "\n")).c_str()));
    }
}

} // namespace de
