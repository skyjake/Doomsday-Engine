/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AMETHYST_COMMAND_H__
#define __AMETHYST_COMMAND_H__

#include "string.h"
#include "commandrule.h"
#include "token.h"
#include "macro.h"

class Command : public Shard
{
public:
    Command(const String& commandName = "", Source *src = 0);
    Command(const CommandRule *cmdRule, Source *src = 0);
    Command(Macro *macroCall, Source *src = 0);

    const String& name() { return _name; }
    CommandRule *rule() { return &_rule; }
    Shard *macroShard();
    int argCommandIndex();

    bool isName(const String& str) { return _name == str; }
    bool isCall() { return _macro != 0; }

    bool isModeCommand() const { return _name == "output"/* || isInputCommand()*/; }
    //bool isInputCommand() const { return _name == "input"; }
    bool isMacroCommand() const { return _name == "macro"; }
    bool isConditionalCommand() const { return _name == "ifdef" || _name == "ifndef"; }
    bool isArgCommand();
    bool isReverseArgCommand();
    bool isListCommand();
    bool isDefinitionListCommand();
    bool isTableCommand() const { return _name == "table"; }
    bool isItemCommand();
    bool isRuleCommand();
    bool isSourceCommand();
    bool isApplyCommand() const { return _name == "apply"; }
    bool isSetCommand() const { return _name == "set"; }
    
    bool isIndependent();
    bool isBreaking(); // paragraph break before
    bool isPostBreaking() { return _rule.hasFlag(CRF_POST_BREAKING); }
    bool isLineBreaking(); // line break before
    bool isPostLineBreaking() { return _rule.hasFlag(CRF_POST_LINE_BREAKING); }
    bool isTidy() { return _rule.hasFlag(CRF_TIDY); }
    
    int styleFlag();
    const GemClass &gemClass() { return _rule.gemClass(); }

    bool hasArg(const String& str);
    Token *arg(int idx = 0);

protected:
    String          _name;
    CommandRule     _rule;  // How the command behaves.
    Macro           *_macro;
};

#endif
