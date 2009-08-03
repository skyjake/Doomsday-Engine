/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

Lex::Lex(const String& input) : input_(&input), lineCommentChar_('#')
{}

const String& Lex::input() const
{
    return *input_;
}
    
duint Lex::pos() const
{
    return state_.pos;
}

duchar Lex::peek() const
{
    if(state_.pos >= input_->size())
    {
        // There is no more; trying to get() will throw an exception.
        return 0;
    }
    
    duchar c = input_->at(state_.pos);

    if(!mode_[SKIP_COMMENTS_BIT] && (c == lineCommentChar_))
    {
        // This isn't considered part of the input stream. Skip it.
        duint p = state_.pos;
        while(p < input_->size() && input_->at(++p) != '\n');
        nextPos_ = p + 1;
        if(p == input_->size())
        {
            return 0;
        }
        return '\n';
    }
    
    nextPos_ = state_.pos + 1;
    return input_->at(state_.pos);
}
    
duchar Lex::get()
{
    duchar c = peek();
    if(!c)
    {
        /// @throw OutOfInputError  No more characters left in input.
        throw OutOfInputError("Lex::get", "No more characters in input");
    }
    
    // The next position is determined by peek().
    state_.pos = nextPos_;
    
    // Did we move to a new line?
    if(c == '\n')
    {
        state_.lineNumber++;
        state_.lineStartPos = state_.pos;
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
    duchar c = 0;
    while(isWhite(c = peek()) && c != '\n')
    {
        get();
    }
}

void Lex::skipToNextLine()
{
    while(get() != '\n');
}

bool Lex::onlyWhiteOnLine()
{
    State saved = state_;    
    try
    {
        for(;;)
        {
            duchar c = get();
            if(c == '\n')
            {
                state_ = saved;
                return true;
            }
            if(!isWhite(c))
            {
                state_ = saved;
                return false;
            }
        }
    }
    catch(const OutOfInputError&)
    {
        state_ = saved;
        return true;
    }
}

duint Lex::countLineStartSpace() const
{
    duint pos = state_.lineStartPos;
    duint count = 0;
    
    while(pos < input_->size() && isWhite(input_->at(pos++))) count++;
    return count;
}

bool Lex::isWhite(duchar c)
{
    return std::isspace(c) != 0;
}

bool Lex::isAlpha(duchar c)
{
    return std::isalpha(c) != 0;   
}

bool Lex::isNumeric(duchar c)
{
    return std::isdigit(c) != 0;
}

bool Lex::isHexNumeric(duchar c)
{
    return isNumeric(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool Lex::isAlphaNumeric(duchar c)
{
    return std::isalnum(c) != 0 || c == '_' || c == '@';
}
