/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Lex"
#include "de/TokenBuffer"

#include <cctype>

using namespace de;

Lex::Lex(String const &input,
         QChar lineCommentChar,
         QChar multiCommentChar,
         ModeFlags initialMode)
    : _input(&input)
    , _lineCommentChar(lineCommentChar)
    , _multiCommentChar(multiCommentChar)
    , _mode(initialMode)
{}

String const &Lex::input() const
{
    return *_input;
}

bool Lex::atEnd() const
{
    return _state.pos >= duint(_input->size());
}

duint Lex::pos() const
{
    return _state.pos;
}

bool Lex::atCommentStart() const
{
    if (atEnd() || _mode.testFlag(RetainComments))
    {
        return false;
    }

    QChar const c = _input->at(_state.pos);
    if (c == _lineCommentChar)
    {
        if (!_mode.testFlag(DoubleCharComment)) return true;
        if (int(_state.pos) >= _input->size() - 1) return false;

        QChar const d = _input->at(_state.pos + 1);
        if (d == _lineCommentChar || d == _multiCommentChar)
        {
            return true;
        }
    }
    return false;
}

QChar Lex::peekComment() const
{
    DENG2_ASSERT(atCommentStart());

    duint const inputSize = duint(_input->size());

    // Skipping multiple lines?
    if (_mode.testFlag(DoubleCharComment))
    {
        QChar c = _input->at(_state.pos + 1);
        if (c == _multiCommentChar)
        {
            duint p = _state.pos + 2;
            while (p < inputSize - 1 &&
                   !(_input->at(p)     == _multiCommentChar &&
                     _input->at(p + 1) == _lineCommentChar))
            {
                ++p;
            }
            p += 2; // skip the ending
            _nextPos = p + 1;
            return (p < inputSize? _input->at(p) : 0);
        }
    }

    // Skip over the line.
    duint p = _state.pos;
    while (p < inputSize && _input->at(++p) != '\n') {}
    _nextPos = p + 1;
    return (p < inputSize? '\n' : 0);
}

QChar Lex::peek() const
{
    if (atEnd())
    {
        // There is no more; trying to get() will throw an exception.
        return 0;
    }

    if (atCommentStart())
    {
        return peekComment();
    }

#if 0
    if (!_mode.testFlag(SkipComments) && (c == _lineCommentChar))
    {
        if (!_mode.testFlag(DoubleCharComment) ||
            (int(_state.pos) < _input->size() - 1 && _input->at(int(_state.pos) + 1) == _lineCommentChar))
        {
            // This isn't considered part of the input stream. Skip it.
            duint p = _state.pos;
            while (p < duint(_input->size()) && _input->at(++p) != '\n') {}
            _nextPos = p + 1;
            if (p == duint(_input->size()))
            {
                return 0;
            }
            return '\n';
        }
    }
#endif

    _nextPos = _state.pos + 1;
    return _input->at(_state.pos);
}

QChar Lex::get()
{
    if (atEnd())
    {
        /// @throw OutOfInputError  No more characters left in input.
        throw OutOfInputError("Lex::get", "No more characters in input");
    }

    QChar c = peek();

    // Keep track of the line numbers.
    for (duint p = _state.pos; p < _nextPos; ++p)
    {
        if (_input->at(p) == '\n')
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
    QChar c = 0;
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
            QChar c = get();
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
    catch (OutOfInputError const &)
    {
        _state = saved;
        return true;
    }
}

duint Lex::countLineStartSpace() const
{
    duint pos = _state.lineStartPos;
    duint count = 0;

    while (pos < duint(_input->size()) && isWhite(_input->at(pos++)))
    {
        count++;
    }
    return count;
}

bool Lex::parseLiteralNumber(QChar c, TokenBuffer &output)
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
               (isHex && !gotX && (c == 'x' || c == 'X')))
        {
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

bool Lex::isWhite(QChar c)
{
    return c.isSpace();
}

bool Lex::isAlpha(QChar c)
{
    return c.isLetter();
}

bool Lex::isNumeric(QChar c)
{
    return c.isDigit();
}

bool Lex::isHexNumeric(QChar c)
{
    return isNumeric(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool Lex::isAlphaNumeric(QChar c)
{
    return c.isLetterOrNumber() || c == '_' || c == '@';
}
