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

#include "gemtest.h"
#include "utils.h"
#include <QDebug>

GemTest::GemTest() : Linkable(true)
{}

GemTest::GemTest(Token *initWith) : Linkable()
{
    init(initWith);
}

/**
 * Returns zero if not a match at all; one if the count is missing.
 */
int CompareCount(const String& str, const String& pat)
{
    if(!str.startsWith(pat)) return 0;
    if(str == pat) return 1;
    return str.mid(pat.size()).toInt();
}

void GemTest::newTest(GemTestID id, int arg, const String& text)
{
    _commands.addBefore(new GemTestCommand(id, arg, text, _failBit, _escalateBit));
    _addedBit = true;
}

void GemTest::init(Token *first)
{
    enum testArgType_e { TA_NONE = 0, TA_NUMBER, TA_TEXT};
    struct { const char *name; GemTestID cmdId; testArgType_e argType; } tests[] =
    {
        { "try",        BeginTry,       TA_NONE },
        { "pass",       CheckIfPassed,  TA_NONE },
        { "top",        IsTop,          TA_NONE },
        { "me",         IsMe,           TA_NONE },
        { "myparent",   IsMyParent,     TA_NONE },
        { "myancestor", IsMyAncestor,   TA_NONE },
        { "break",      IsBreak,        TA_NONE },
        { "br",         IsLineBreak,    TA_NONE },
        { "control",    IsControl,      TA_NONE },
        { "@",          GoSelf,         TA_NONE },
        { "final",      GoFinal,        TA_NONE },
        { "child",      NthChild,       TA_NUMBER },
        { "order",      NthOrder,       TA_NUMBER },
        { "count",      ChildCount,     TA_NUMBER },
        { "width",      CellWidth,      TA_NUMBER },
        { "text",       Text,           TA_TEXT },
        { "begins",     TextBegins,     TA_TEXT },
        { 0,            InvalidGemTest, TA_NONE }
    };
    struct { const char *condition; GemClass::GemType type; } gemTypes[] =
    {
        { "gem",        GemClass::Gem },
        { "indent",     GemClass::Indent },
        { "list",       GemClass::List },
        { "deflist",    GemClass::DefinitionList },
        { "table",      GemClass::Table },
        { "part",       GemClass::PartTitle },
        { "chapter",    GemClass::ChapterTitle },
        { "section",    GemClass::SectionTitle },
        { "subsec",     GemClass::SubSectionTitle },
        { "sub2sec",    GemClass::Sub2SectionTitle },
        { "sub3sec",    GemClass::Sub3SectionTitle },
        { "sub4sec",    GemClass::Sub4SectionTitle },
        { "contents",   GemClass::Contents },
        { 0,            GemClass::None }
    };
    struct { const char* condition; GemClass::FlushMode mode; } gemFlushModes[] =
    {
        { "left",       GemClass::FlushLeft },
        { "right",      GemClass::FlushRight },
        { "center",     GemClass::FlushCenter },
        { 0,            GemClass::FlushInherit }
    };
    int i;

    // Destroy existing commands.
    _commands.destroy();

    // Next we'll convert all the shards to test commands.
    for(Shard *it = first; it; it = it->next())
    {
        String con = ((Token*)it)->unEscape();
        if(con.isEmpty()) continue;

        // This'll report unknown commands.
        _addedBit = false;

        // Normally 'false' is the failing condition.
        _failBit = false;
        while(con[0] == '!')
        {
            // Use a negative test.
            con.remove(0, 1);
            _failBit = !_failBit;
        }

        // Check for escalation.
        _escalateBit = false;
        if(con[0] == '^')
        {
            con.remove(0, 1);
            _escalateBit = true;
        }

        // Navigation of the test pointer.
        if((i = CompareCount(con, "parent"))) newTest(GoParent, i);
        else if((i = CompareCount(con, "next"))) newTest(GoNext, i);
        else if((i = CompareCount(con, "prev"))) newTest(GoPrev, i);
        else if((i = CompareCount(con, "first"))) newTest(GoFirst, i);
        else if((i = CompareCount(con, "last"))) newTest(GoLast, i);
        else if((i = CompareCount(con, "following"))) newTest(GoFollowing, i);
        else if((i = CompareCount(con, "preceding"))) newTest(GoPreceding, i);
        
        for(i = 0; tests[i].name; i++)
            if(con == tests[i].name)
            {
                if(tests[i].argType == TA_NUMBER)
                {
                    int arg = 0;
                    if(it->next())
                    {
                        it = it->next();
                        arg = ((Token*)it)->token().toInt();
                    }
                    newTest(tests[i].cmdId, arg);
                }
                else if(tests[i].argType == TA_TEXT)
                {
                    String txt;
                    if(it->next())
                    {
                        it = it->next();
                        txt = ((Token*)it)->unEscape();
                    }
                    newTest(tests[i].cmdId, 0, txt);
                }
                else
                {
                    newTest(tests[i].cmdId);
                }
                break;
            }

        // Types.
        for(i = 0; gemTypes[i].condition; i++)
            if(con == gemTypes[i].condition)
            {
                newTest(GemType, gemTypes[i].type);
                break;
            }

        // Flush modes.
        for(i = 0; gemFlushModes[i].condition; ++i)
            if(con == gemFlushModes[i].condition)
            {
                newTest(GemFlushMode, gemFlushModes[i].mode);
                break;
            }

        // Flags that must be present.
        String lowCon = stringCase(con, LOWERCASE_ALL);
        bool checkForJust = con[0].isUpper();
        if((i = styleForName(lowCon)))
        {
            newTest(checkForJust? ExclusiveFlag : HasFlag, i);
        }
        
        if(!_addedBit)
        {
            // Print a warning message.         
            qWarning() << con << ": Unknown test command.";
        }
    }
}

bool GemTest::test(Gem *gem)
{
    Gem *test = gem;       // The test pointer.
    Gem *tryStart;
    bool result, trying = false, passed;
    int i;
    
    for(GemTestCommand *cmd = _commands.next(); !cmd->isRoot(); cmd = cmd->next())
    {
        // We can skip commands if trying is passed.
        if(trying && ((passed && cmd->id() != CheckIfPassed) ||
                      (!test && cmd->id() != CheckIfPassed && cmd->id() != GoSelf)))
            continue;

        result = true;
        switch(cmd->id())
        {
        case GoSelf:
            test = gem;
            break;

        case GoParent:
            for(i = 0; test && i < cmd->intArg(); i++) test = test->parentGem();
            break;

        case GoNext:
            for(i = 0; test && i < cmd->intArg(); i++) test = test->nextGem();
            break;

        case GoPrev:
            for(i = 0; test && i < cmd->intArg(); i++) test = test->prevGem();
            break;

        case GoFirst:
            for(i = 0; test && i < cmd->intArg(); i++) test = test->firstGem();
            break;

        case GoLast:
            for(i = 0; test && i < cmd->intArg(); i++) test = test->lastGem();
            break;

        case GoFollowing:
            for(i = 0; test && i < cmd->intArg(); i++) test = test->followingGem();
            break;
            
        case GoPreceding:
            for(i = 0; test && i < cmd->intArg(); i++) test = test->precedingGem();
            break;

        case GoFinal:
            test = gem->finalGem();
            break;

        case BeginTry:
            trying = true;
            passed = false;
            tryStart = test;
            break;

        case CheckIfPassed:
            trying = false;
            result = passed;
            test = tryStart; // Return to where the test began.
            break;

        default:
            // Are we escalating this check?
            if(cmd->escalating())
            {
                result = false;
                for(Gem* it = test->parentGem(); it; it = it->parentGem())
                {
                    if(cmd->execute(gem, it))
                    {
                        result = true;
                        break;
                    }
                }
            }
            else
            {
                result = cmd->execute(gem, test);
            }
            break;
        }

        // You can't fail the BeginTry command.
        if(cmd->id() == BeginTry) continue;

        // A null test pointer is an automatic failure.
        if(!test)
        {
            if(!trying) return false;
            result = false;
        }

        // Did it fail?
        if(trying)
        {
            if(result != cmd->negated()) passed = true; // Passed!
        }
        else
        {
            if(result == cmd->negated()) return false; // Failed!
        }
    }

    if(trying)
    {
        qCritical("A 'try..pass' is missing 'pass'.");
        exit(1);
    }

    // All commands were successful.
    return true;
}

bool GemTest::operator == (GemTest &other)
{
    if(count() != other.count()) return false;
    for(GemTest *mine = next(), *yours = other.next();
        !mine->isRoot(); mine = mine->next(), yours = yours->next())
    {
        if(!(mine->_commands == yours->_commands)) return false;
    }
    return true;
}
