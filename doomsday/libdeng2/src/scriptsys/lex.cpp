/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Lex"

#include <cctype>

using namespace de;

Lex::Lex(String const &input) : _input(&input), _lineCommentChar('#'), _mode(0)
{}

String const &Lex::input() const
{
    return *_input;
}
    
duint Lex::pos() const
{
    return _state.pos;
}

QChar Lex::peek() const
{
    if(_state.pos >= duint(_input->size()))
    {
        // There is no more; trying to get() will throw an exception.
        return 0;
    }
    
    QChar c = _input->at(_state.pos);

    if(!_mode.testFlag(SkipComments) && (c == _lineCommentChar))
    {
        // This isn't considered part of the input stream. Skip it.
        duint p = _state.pos;
        while(p < duint(_input->size()) && _input->at(++p) != '\n') {}
        _nextPos = p + 1;
        if(p == duint(_input->size()))
        {
            return 0;
        }
        return '\n';
    }
    
    _nextPos = _state.pos + 1;
    return _input->at(_state.pos);
}
    
QChar Lex::get()
{
    QChar c = peek();
    if(c == 0)
    {
        /// @throw OutOfInputError  No more characters left in input.
        throw OutOfInputError("Lex::get", "No more characters in input");
    }
    
    // The next position is determined by peek().
    _state.pos = _nextPos;
    
    // Did we move to a new line?
    if(c == '\n')
    {
        _state.lineNumber++;
        _state.lineStartPos = _state.pos;
    }
    return c;
}

void Lex::skipWhite()
{
    while(isWhite(peek()))
    {
        get();
    }
}

void Lex::skipWhiteExceptNewline()
{
    QChar c = 0;
    while(isWhite(c = peek()) && c != '\n')
    {
        get();
    }
}

void Lex::skipToNextLine()
{
    while(get() != '\n') {}
}

bool Lex::onlyWhiteOnLine()
{
    State saved = _state;    
    try
    {
        for(;;)
        {
            QChar c = get();
            if(c == '\n')
            {
                _state = saved;
                return true;
            }
            if(!isWhite(c))
            {
                _state = saved;
                return false;
            }
        }
    }
    catch(OutOfInputError const &)
    {
        _state = saved;
        return true;
    }
}

duint Lex::countLineStartSpace() const
{
    duint pos = _state.lineStartPos;
    duint count = 0;
    
    while(pos < duint(_input->size()) && isWhite(_input->at(pos++))) count++;
    return count;
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
