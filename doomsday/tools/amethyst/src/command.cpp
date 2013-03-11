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

#include "command.h"
#include "block.h"

Command::Command(const String& commandName, Source *src)
: Shard(Shard::COMMAND, src)
{
    _name = commandName;
    _macro = 0;
}

Command::Command(const CommandRule *cmdRule, Source *src)
: Shard(Shard::COMMAND, src)
{
    _rule = *cmdRule;
    _name = cmdRule->name();
    _macro = 0;
}

Command::Command(Macro *macroCall, Source *src)
: Shard(Shard::COMMAND, src)
{
    _macro = macroCall;
    _rule = CommandRule(_macro->name(), GemClass(), 0,
        _macro->argTypes());
    _name = _macro->name();
}

/**
 * Independent commands have their own gems.
 */
bool Command::isIndependent()
{
    return _rule.isIndependent();
}

/**
 * Breaking commands get a Break gem before them.
 */
bool Command::isBreaking()
{
    return _rule.isBreaking();
}

bool Command::isLineBreaking()
{
    return _rule.isLineBreaking();
}

bool Command::isListCommand()
{
    return _rule.isGemType(GemClass::List);
}

bool Command::isDefinitionListCommand()
{
    return _rule.isGemType(GemClass::DefinitionList);
}

bool Command::isItemCommand()
{
    return _name == "item";
}

bool Command::isRuleCommand()
{
    return _name == "format" || _name == "length";
}

bool Command::isSourceCommand()
{
    return _name == "include" || _name == "require";
}

bool Command::isArgCommand()
{
    return _name == "arg";
}

bool Command::isReverseArgCommand()
{
    return _name == "rarg";
}

int Command::argCommandIndex()
{
    if(!isArgCommand() && !isReverseArgCommand()) return 0;
    Token *tok = arg();
    if(!tok) return 1; // The first argument, by default.
    return tok->token().toInt();
}

int Command::styleFlag()
{
    return _rule.gemClass().style();
}

Shard *Command::macroShard()
{
    if(!_macro) return NULL;
    return _macro->shard();
}

bool Command::hasArg(const String& str)
{
    for(Shard *it = first(); it; it = it->next())
    {
        Block *block = (Block*) it->first();
        if(!block || block->type() != BLOCK) continue;
        for(Shard *arg = block->first(); arg; arg = arg->next())
        {
            // Blocks have only Tokens as children!
            if(((Token*)arg)->token() == str) return true;
        }
    }
    return false;
}

Token *Command::arg(int idx)
{
    for(Shard *it = first(); it; it = it->next())
    {
        if(idx--) continue;
        it = it->first();
        if(!it || it->type() != BLOCK) break;
        return (Token*) it->first();
    }
    return 0;
}
