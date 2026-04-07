/** @file udmfparser.cpp  UDMF parser.
 *
 * @authors Copyright (c) 2016-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "udmfparser.h"

#include <de/numbervalue.h>
#include <de/textvalue.h>

using namespace de;

UDMFParser::UDMFParser()
{}

void UDMFParser::setGlobalAssignmentHandler(UDMFParser::AssignmentFunc func)
{
    _assignmentHandler = std::move(func);
}

void UDMFParser::setBlockHandler(UDMFParser::BlockFunc func)
{
    _blockHandler = std::move(func);
}

const UDMFParser::Block &UDMFParser::globals() const
{
    return _globals;
}

void UDMFParser::parse(const String &input)
{
    // Lexical analyzer for Haw scripts.
    _analyzer = UDMFLex(input);

    while (nextFragment() > 0)
    {
        if (_range.lastToken().equals(UDMFLex::BRACKET_OPEN))
        {
            const String blockType = _range.firstToken().str().lower();

            Block block;
            parseBlock(block);

            if (_blockHandler)
            {
                _blockHandler(blockType, block);
            }
        }
        else
        {
            parseAssignment(_globals);
        }
    }

    // We're done, free the remaining tokens.
    _tokens.clear();
}

dsize UDMFParser::nextFragment()
{
    _analyzer.getExpressionFragment(_tokens);

    // Begin with the whole thing.
    _range = TokenRange(_tokens);

    return _tokens.size();
}

void UDMFParser::parseBlock(Block &block)
{
    // Read all the assignments in the block.
    while (nextFragment() > 0)
    {
        if (_range.firstToken().equals(UDMFLex::BRACKET_CLOSE))
            break;

        parseAssignment(block);
    }
}

void UDMFParser::parseAssignment(Block &block)
{
    if (_range.isEmpty())
        return; // Nothing here?

    if (!_range.lastToken().equals(UDMFLex::SEMICOLON))
    {
        throw SyntaxError("UDMFParser::parseAssignment",
                          "Expected expression to end in a semicolon at " +
                          _range.lastToken().asText());
    }
    if (_range.size() == 1)
        return; // Just a semicolon?

    if (!_range.token(1).equals(UDMFLex::ASSIGN))
    {
        throw SyntaxError("UDMFParser::parseAssignment",
                          "Expected expression to have an assignment operator at " +
                          _range.token(1).asText());
    }

    const String identifier = _range.firstToken().str().lower();
    const Token &valueToken = _range.token(2);

    // Store the assigned value into a variant.
    std::shared_ptr<Value> value;
    switch (valueToken.type())
    {
    case Token::KEYWORD:
        if (valueToken.equals(UDMFLex::T_TRUE))
        {
            value.reset(new NumberValue(true));
        }
        else if (valueToken.equals(UDMFLex::T_FALSE))
        {
            value.reset(new NumberValue(false));
        }
        else
        {
            throw SyntaxError("UDMFParser::parseAssignment",
                              "Unexpected value for assignment at " + valueToken.asText());
        }
        break;

    case Token::LITERAL_NUMBER:
        if (valueToken.isInteger())
        {
            value.reset(new NumberValue(valueToken.toInteger()));
        }
        else
        {
            value.reset(new NumberValue(valueToken.toDouble()));
        }
        break;

    case Token::LITERAL_STRING_QUOTED:
        value.reset(new TextValue(valueToken.unescapeStringLiteral()));
        break;

    case Token::IDENTIFIER:
        value.reset(new TextValue(valueToken.str()));
        break;

    default:
        DE_ASSERT_FAIL("[UMDFParser::parseAssignment] Invalid token type");
        break;
    }

    block.insert(identifier, value);

    if (_assignmentHandler && (&block == &_globals))
    {
        _assignmentHandler(identifier, *value);
    }
}
