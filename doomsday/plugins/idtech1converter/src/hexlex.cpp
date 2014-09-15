/** @file hexlex.cpp  Lexical analyzer for Hexen definition/script syntax.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include <cstdio>
#include <cstring>
#include "hexlex.h"
#include <de/Log>

using namespace de;

namespace idtech1 {

DENG2_PIMPL(HexLex)
{
    String sourcePath;                   ///< Used to identify the source in error messages.
    ddstring_s const *script = nullptr;  ///< The start of the script being parsed.
    int scriptLength         = 0;
    int readPos              = 0;        ///< Current read position.
    int lineNumber           = 0;
    ddstring_s token;
    bool alreadyGot          = false;
    bool multiline           = false;    ///< @c true= current token spans multiple lines.

    Instance(Public *i) : Base(i)
    {
        Str_InitStd(&token);
    }

    ~Instance()
    {
        Str_Free(&token);
    }

    void checkOpen()
    {
        if(script) return;
        throw Error("HexLex::checkOpen", "No script to parse!");
    }

    String readPosAsText()
    {
        return "\"" + NativePath(sourcePath).pretty() + "\" on line #" + String::number(lineNumber);
    }

    bool atEnd()
    {
        checkOpen();
        return (readPos >= scriptLength);
    }
};

HexLex::HexLex(ddstring_s const *script, String const &sourcePath)
    : d(new Instance(this))
{
    setSourcePath(sourcePath);
    if(script)
    {
        parse(script);
    }
}

void HexLex::parse(ddstring_s const *script)
{
    LOG_AS("HexLex");

    d->script       = script;
    d->scriptLength = (d->script? Str_Length(d->script) : 0);
    d->readPos      = 0;
    d->lineNumber   = 1;
    d->alreadyGot   = false;
    Str_Clear(&d->token);
}

void HexLex::setSourcePath(String const &sourcePath)
{
    d->sourcePath = sourcePath;
}

/// @todo Revise with get/peek character mechanics.
bool HexLex::readToken()
{
    LOG_AS("HexLex");

    d->checkOpen();

    if(d->alreadyGot)
    {
        d->alreadyGot = false;
        return true;
    }

    d->multiline = false;

    if(d->atEnd())
    {
        return false;
    }

    // Skip any whitespace before the beginning of the next token.
    bool foundToken = false;
    while(!foundToken)
    {
        char ch;
        while((ch = Str_At(d->script, d->readPos)) <= ' ')
        {
            if(d->atEnd()) return false;

            d->readPos++;

            if(ch == '\n')
            {
                d->lineNumber++;
                d->multiline = true;
            }
        }

        if(d->atEnd()) return false;

        ch = Str_At(d->script, d->readPos);

        // A single line comment?
        if(ch == ';' || (ch == '/' && d->readPos + 1 < d->scriptLength && Str_At(d->script, d->readPos + 1) == '/'))
        {
            while((ch = Str_At(d->script, d->readPos++)) != '\n')
            {
                if(d->atEnd()) return false;
            }

            d->lineNumber++;
            d->multiline = true;
        }
        else
        {
            foundToken = true;
        }
    }

    Str_Clear(&d->token);

    // A quoted string?
    if(Str_At(d->script, d->readPos) == '"')
    {
        char ch;
        d->readPos++;
        while((ch = Str_At(d->script, d->readPos)) != '"')
        {
            if(ch != '\r')
            {
                Str_AppendChar(&d->token, ch);
            }
            d->readPos++;

            if(ch == '\n')
            {
                d->lineNumber++;
            }

            if(d->atEnd()) break;
        }
        d->readPos++;
    }
    else
    {
        // Normal token.
        char ch;
        while((ch = Str_At(d->script, d->readPos)) > ' ')
        {
            // A single line comment?
            if(ch == ';'|| (ch == '/' && d->readPos + 1 < d->scriptLength && Str_At(d->script, d->readPos + 1) == '/'))
                break;

            Str_AppendChar(&d->token, ch);
            d->readPos++;

            if(d->atEnd()) break;
        }
    }

    return true;
}

void HexLex::unreadToken()
{
    if(!d->readPos) return;

    d->alreadyGot = true;
}

ddstring_s const *HexLex::token()
{
    return &d->token;
}

ddouble HexLex::readNumber()
{
    LOG_AS("HexLex");

    if(!readToken())
    {
        /// @throw SyntaxError Expected a number value.
        throw SyntaxError("HexLex", String("Missing number value\nIn ") + d->readPosAsText());
    }

    char *stopper;
    ddouble number = strtod(Str_Text(&d->token), &stopper);
    if(*stopper != 0)
    {
        return 0;
        //Con_Error("HexLex: Non-numeric constant '%s'\nIn %s",
        //          Str_Text(&d->token), d->readPosAsText().toUtf8().constData());
    }

    return number;
}

ddstring_s const *HexLex::readString()
{
    LOG_AS("HexLex");
    if(!readToken())
    {
        /// @throw SyntaxError Expected a string value.
        throw SyntaxError("HexLex", String("Missing string value\nIn ") + d->readPosAsText());
    }
    return &d->token;
}

de::Uri HexLex::readUri(String const &defaultScheme)
{
    LOG_AS("HexLex");
    if(!readToken())
    {
        /// @throw SyntaxError Expected a URI value.
        throw SyntaxError("HexLex", String("Missing URI value\nIn ") + d->readPosAsText());
    }
    return de::Uri(defaultScheme, Path(Str_Text(Str_PercentEncode(AutoStr_FromTextStd(Str_Text(&d->token))))));
}

int HexLex::lineNumber() const
{
    return d->lineNumber;
}

} // namespace idtech1
