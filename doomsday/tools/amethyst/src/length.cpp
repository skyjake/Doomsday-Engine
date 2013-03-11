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

#include "length.h"
#include "token.h"

Length::Length()
{
    clear();
}

Length::Length(const Length &other)
{
    memcpy(_values, other._values, sizeof(_values));
}

bool Length::isClear() const
{
    for(int i = 0; i < NumValues; i++) 
        if(_values[i] != NotSet) return false;
    return true;
}

bool Length::allSet() const
{
    for(int i = 0; i < NumValues; i++)
        if(_values[i] == NotSet) return false;
    return true;
}

void Length::clear()
{
    for(int i = 0; i < NumValues; i++) _values[i] = NotSet;
}

void Length::defaults()
{
    for(int i = 0; i < NumValues; i++)
        if(_values[i] == NotSet) _values[i] = 0;
}

Length::ID Length::IDForName(const String& name)
{
    const char *lengthNames[NumValues] = {
        "leftmargin", "rightmargin", "spacing", "indent"
    };
    for(int i = 0; i < NumValues; i++)
        if(name == lengthNames[i]) return (Length::ID) i;
    return Invalid;
}

void Length::init(Token *token)
{
    clear();
    for(; token; token = (Token*) token->next())
    {
        String str = token->unEscape();
        ID id = IDForName(str);
        // If it was a length ID, and there is a parameter following,
        // set the corresponding length value.
        if(id != Invalid && token->next())
        {
            token = (Token*) token->next();
            set(id, token->unEscape().toInt());
        }
    }
}

/**
 * Returns true if @a other contains values for our unset lengths.
 */
bool Length::canLearnFrom(const Length &other)
{
    for(int i = 0; i < NumValues; i++)
        if(_values[i] == NotSet && other._values[i] != NotSet)
            return true;
    // The other can't make a contribution.
    return false;
}

Length &Length::operator += (const Length &other)
{
    for(int i = 0; i < NumValues; i++)
        if(_values[i] == NotSet && other._values[i] != NotSet)
            _values[i] = other._values[i];
    return *this;
}
