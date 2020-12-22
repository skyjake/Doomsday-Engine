/** @file hexlex.cpp  Lexical analyzer for Hexen definition/script syntax.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "hexlex.h"
#include <cstdio>
#include <cstring>

using namespace de;

#define T_COMMENT ';' ///< Single-line comment.
#define T_QUOTE   '"'

void HexLex::checkOpen()
{
    if(!_script) Con_Error("HexLex: No script to parse!");
}

bool HexLex::atEnd()
{
    checkOpen();
    return (_readPos >= Str_Length(_script));
}

void HexLex::syntaxError(const char *message)
{
    Con_Error("HexLex: SyntaxError in \"%s\" on line #%i.\n%s",
              F_PrettyPath(Str_Text(&_sourcePath)), _lineNumber, message);
}

HexLex::HexLex(const ddstring_s *script, const ddstring_s *sourcePath)
    : _script    (0)
    , _readPos   (0)
    , _lineNumber(0)
    , _alreadyGot(false)
    , _multiline (false)
{
    Str_InitStd(&_sourcePath);
    Str_InitStd(&_token);

    if(script)
    {
        parse(script);
    }
    if(sourcePath)
    {
        setSourcePath(sourcePath);
    }
}

HexLex::~HexLex()
{
    Str_Free(&_sourcePath);
    Str_Free(&_token);
}

void HexLex::parse(const ddstring_s *script)
{
    _script     = script;
    _readPos    = 0;
    _lineNumber = 1;
    _alreadyGot = false;
    Str_Clear(&_token);
}

void HexLex::setSourcePath(const ddstring_s *sourcePath)
{
    if(!sourcePath)
    {
        Str_Clear(&_sourcePath);
    }
    else
    {
        Str_Copy(&_sourcePath, sourcePath);
    }
}

bool HexLex::readToken()
{
    checkOpen();
    if(_alreadyGot)
    {
        _alreadyGot = false;
        return true;
    }

    _multiline = false;

    if(atEnd())
    {
        return false;
    }

    bool foundToken = false;
    while(!foundToken)
    {
        while(Str_At(_script, _readPos) <= ' ')
        {
            if(atEnd())
            {
                return false;
            }

            if(Str_At(_script, _readPos++) == '\n')
            {
                _lineNumber++;
                _multiline = true;
            }
        }

        if(atEnd())
        {
            return false;
        }

        if(Str_At(_script, _readPos) != T_COMMENT)
        {
            // Found a token
            foundToken = true;
        }
        else
        {
            // Skip comment.
            while(Str_At(_script, _readPos++) != '\n')
            {
                if(atEnd())
                {
                    return false;
                }
            }

            _lineNumber++;
            _multiline = true;
        }
    }

    Str_Clear(&_token);
    if(Str_At(_script, _readPos) == T_QUOTE)
    {
        // Quoted string.
        _readPos++;
        while(Str_At(_script, _readPos) != T_QUOTE)
        {
            const char ch = Str_At(_script, _readPos++);
            if(ch != '\r')
            {
                Str_AppendChar(&_token, ch);
            }
            if(ch == '\n')
            {
                _lineNumber++;
            }

            if(atEnd())
            {
                break;
            }
        }
        _readPos++;
    }
    else
    {
        // Normal string.
        while(Str_At(_script, _readPos) > ' ' &&
              Str_At(_script, _readPos) != T_COMMENT)
        {
            Str_AppendChar(&_token, Str_At(_script, _readPos++));
            if(atEnd())
            {
                break;
            }
        }
    }

    return true;
}

void HexLex::unreadToken()
{
    if(_readPos == 0)
    {
        return;
    }
    _alreadyGot = true;
}

const ddstring_s *HexLex::token()
{
    return &_token;
}

ddouble HexLex::readNumber()
{
    if(!readToken())
    {
        syntaxError("Missing number value");
    }

    char *stopper;
    ddouble number = strtod(Str_Text(&_token), &stopper);
    if(*stopper != 0)
    {
        Con_Error("HexLex: Non-numeric constant '%s' in \"%s\" on line #%i",
                  Str_Text(&_token), F_PrettyPath(Str_Text(&_sourcePath)), _lineNumber);
    }

    return number;
}

const ddstring_s *HexLex::readString()
{
    if(!readToken())
    {
        syntaxError("Missing string");
    }
    return &_token;
}

res::Uri HexLex::readUri(const String &defaultScheme)
{
    if(!readToken())
    {
        syntaxError("Missing uri");
    }
    return res::Uri(defaultScheme, Path(Str_Text(Str_PercentEncode(AutoStr_FromTextStd(Str_Text(&_token))))));
}

int HexLex::lineNumber() const
{
    return _lineNumber;
}
