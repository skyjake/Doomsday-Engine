/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/lex.h"
#include "de/scripting/tokenbuffer.h"

#include <cctype>
#include <cwctype>

using namespace de;

Lex::Lex(const String &input, Char lineCommentChar, Char multiCommentChar, ModeFlags initialMode)
    : _input(&input)
    , _lineCommentChar(lineCommentChar)
    , _multiCommentChar(multiCommentChar)
    , _mode(initialMode)
{
    _state.pos = _state.lineStartPos = _input->begin();
}

const String &Lex::input() const
{
    return *_input;
}

bool Lex::atEnd() const
{
    return _state.pos.pos() >= _input->sizeb();
}

String::const_iterator Lex::pos() const
{
    return _state.pos;
}

bool Lex::atCommentStart() const
{
    if (atEnd() || (_mode & RetainComments))
    {
        return false;
    }

    const Char c = *_state.pos;
    if (c == _lineCommentChar)
    {
        if (!(_mode & DoubleCharComment)) return true;
        if (_state.pos.pos() >= _input->sizeb() - 1) return false;

        const Char d = *(_state.pos + 1);
        if (d == _lineCommentChar || d == _multiCommentChar)
        {
            return true;
        }
    }
    return false;
}

Char Lex::peekComment() const
{
    DE_ASSERT(atCommentStart());

    const auto inputSize = _input->sizeb();

    // Skipping multiple lines?
    if (_mode & DoubleCharComment)
    {
        Char c = *(_state.pos + 1);
        if (c == _multiCommentChar)
        {
            auto p = _state.pos + 2;
            while (p.pos() < inputSize - 1 &&
                   !(*p       == _multiCommentChar &&
                     *(p + 1) == _lineCommentChar))
            {
                ++p;
            }
            p += 2; // skip the ending
            _nextPos = p + 1;
            return (p.pos() < inputSize? *p : Char());
        }
    }

    // Skip over the line.
    auto p = _state.pos;
    while (p.pos() < inputSize && *++p != '\n') {}
    _nextPos = p + 1;
    return (p.pos() < inputSize? '\n' : '\0');
}

Char Lex::peek() const
{
    if (atEnd())
    {
        // There is no more; trying to get() will throw an exception.
        return '\0';
    }

    if (atCommentStart())
    {
        return peekComment();
    }

    _nextPos = _state.pos + 1;
    return *_state.pos;
}

Char Lex::get()
{
    if (atEnd())
    {
        /// @throw OutOfInputError  No more characters left in input.
        throw OutOfInputError("Lex::get", "No more characters in input");
    }

    Char c = peek();

    // Keep track of the line numbers.
    for (auto p = _state.pos; p < _nextPos; ++p)
    {
        if (*p == '\n')
        {
            _state.lineNumber++;
            _state.lineStartPos = p + 1;
        }
    }

    // The next position is determined by peek().
    _state.pos = _nextPos;

    return c;
}

void Lex::skipWhite()
{
    while (isWhite(peek()))
    {
        get();
    }
}

void Lex::skipWhiteExceptNewline()
{
    Char c;
    while (isWhite(c = peek()) && c != '\n')
    {
        get();
    }
}

void Lex::skipToNextLine()
{
    while (get() != '\n') {}
}

bool Lex::onlyWhiteOnLine()
{
    State saved = _state;
    try
    {
        for (;;)
        {
            Char c = get();
            if (c == '\n')
            {
                _state = saved;
                return true;
            }
            if (!isWhite(c))
            {
                _state = saved;
                return false;
            }
        }
    }
    catch (const OutOfInputError &)
    {
        _state = saved;
        return true;
    }
}

duint Lex::countLineStartSpace() const
{
    auto pos = _state.lineStartPos;
    duint count = 0;
    while (pos.pos() < _input->sizeb() && isWhite(*pos++))
    {
        count++;
    }
    return count;
}

bool Lex::parseLiteralNumber(Char c, TokenBuffer &output)
{
    if ((c == '.' && isNumeric(peek())) ||
        (_mode.testFlag(NegativeNumbers) && c == '-' && isNumeric(peek())) ||
        isNumeric(c))
    {
        if (c == '-')
        {
            c = get();
            output.appendChar(c);
        }
        bool gotPoint = (c == '.');
        bool isHex = (c == '0' && (peek() == 'x' || peek() == 'X'));
        bool gotX = false;

        output.setType(Token::LITERAL_NUMBER);

        // Read until a non-numeric character is found.
        while (isNumeric((c = peek())) || (isHex && isHexNumeric(c)) ||
               (!isHex && !gotPoint && c == '.') ||
               (isHex && !gotX && (c == 'x' || c == 'X')) ||
               c == '_')
        {
            if (c == '_')
            {
                // Ignore separators.
                get();
                continue;
            }
            // Just one decimal point.
            if (c == '.') gotPoint = true;
            // Just one 'x'.
            if (c == 'x' || c == 'X') gotX = true;
            output.appendChar(get());
        }
        output.endToken();
        return true;
    }
    return false;
}

bool Lex::isWhite(Char c)
{
    return c.isSpace();
}

bool Lex::isAlpha(Char c)
{
    return c.isAlpha();
}

bool Lex::isNumeric(Char c)
{
    return c.isNumeric();
}

bool Lex::isHexNumeric(Char c)
{
    return c.isNumeric() || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool Lex::isAlphaNumeric(Char c)
{
    return c.isAlphaNumeric() || c == '_' || c == '@';
}
